// Copyright Moon Punch Games. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/FileManager.h"
#include "Messages/EFMessageSubsystem.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Platform/EFLocalPlatformProvider.h"
#include "Platform/EFPlatformCatalog.h"
#include "Platform/EFPlatformMessages.h"
#include "Platform/EFPlatformSettings.h"
#include "Platform/EFPlatformSubsystem.h"
#include "Testing/EFTestHelpers.h"

namespace EFPlatformTestPrivate
{
	/** Settings the platform tests change, restored afterwards; storage goes to a throwaway folder. */
	struct FScopedPlatformTestSettings
	{
		FScopedPlatformTestSettings()
		{
			UEFPlatformSettings* Settings = GetMutableDefault<UEFPlatformSettings>();
			bSavedEnableLocal = Settings->bEnableLocalProvider;
			bSavedLocalCloud = Settings->bLocalProviderCloudStorage;
			bSavedQueue = Settings->bQueueWritesWhileUnavailable;
			SavedMaxQueued = Settings->MaxQueuedWrites;
			SavedRetryInterval = Settings->QueueRetryIntervalSeconds;
			SavedFeatureProviders = Settings->FeatureProviders;

			Settings->bEnableLocalProvider = true;
			Settings->bLocalProviderCloudStorage = true;
			Settings->bQueueWritesWhileUnavailable = true;
			Settings->QueueRetryIntervalSeconds = 0.5f;
			Settings->FeatureProviders.Reset();

			Root = FPaths::Combine(FPaths::AutomationTransientDir(), TEXT("EFPlatform"), FGuid::NewGuid().ToString());
			UEFLocalPlatformProvider::SetStorageRootForTests(Root);
		}

		~FScopedPlatformTestSettings()
		{
			UEFPlatformSettings* Settings = GetMutableDefault<UEFPlatformSettings>();
			Settings->bEnableLocalProvider = bSavedEnableLocal;
			Settings->bLocalProviderCloudStorage = bSavedLocalCloud;
			Settings->bQueueWritesWhileUnavailable = bSavedQueue;
			Settings->MaxQueuedWrites = SavedMaxQueued;
			Settings->QueueRetryIntervalSeconds = SavedRetryInterval;
			Settings->FeatureProviders = SavedFeatureProviders;
			UEFLocalPlatformProvider::SetStorageRootForTests(FString());
			IFileManager::Get().DeleteDirectory(*Root, /*RequireExists*/ false, /*Tree*/ true);
		}

		FString Root;
		bool bSavedEnableLocal = true;
		bool bSavedLocalCloud = false;
		bool bSavedQueue = true;
		int32 SavedMaxQueued = 256;
		float SavedRetryInterval = 5.f;
		TMap<EEFPlatformFeature, FName> SavedFeatureProviders;
	};

	/** Keeps only the Local provider, so a store plugin enabled in the test project never receives test writes. */
	UEFLocalPlatformProvider* IsolateLocalProvider(UEFPlatformSubsystem* Platform, UGameInstance* GameInstance)
	{
		UEFLocalPlatformProvider* Local = GameInstance->GetSubsystem<UEFLocalPlatformProvider>();
		for (UEFPlatformProviderSubsystem* Provider : Platform->GetProviders())
		{
			if (Provider != Local)
			{
				Platform->UnregisterProvider(Provider);
			}
		}
		return Local;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEFPlatformLocalProviderTest, "ExtendedFoundation.Platform.LocalProvider",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEFPlatformLocalProviderTest::RunTest(const FString& Parameters)
{
	struct FState
	{
		TSharedPtr<EFPlatformTestPrivate::FScopedPlatformTestSettings> Settings;
		TSharedPtr<EFTest::FScopedGameInstance> Game;
		TStrongObjectPtr<UEFPlatformCatalog> Catalog;
		FEFMessageListenerHandle UnlockListener;
		TArray<FName> UnlockMessages;
		TOptional<FEFResult> Unlock;
		TOptional<FEFResult> Kills1;
		TOptional<FEFResult> Kills2;
		TOptional<FEFResult> Score1;
		TOptional<FEFResult> Score2;
		TOptional<FEFResult> Write;
		TOptional<FEFResult> AchievementsResult;
		TArray<FEFAchievementState> Achievements;
		TOptional<FEFResult> StatsResult;
		TArray<FEFStatValue> Stats;
		TOptional<FEFResult> BoardResult;
		TArray<FEFLeaderboardEntry> Board;
		TOptional<FEFResult> ReadResult;
		TArray<uint8> ReadData;
		TOptional<FEFResult> MissingResult;
		TOptional<FEFResult> ListResult;
		TArray<FEFCloudFileInfo> Files;
	};
	const TSharedRef<FState> State = MakeShared<FState>();
	State->Settings = MakeShared<EFPlatformTestPrivate::FScopedPlatformTestSettings>();
	State->Game = MakeShared<EFTest::FScopedGameInstance>();

	UEFPlatformSubsystem* Platform = State->Game->GetSubsystem<UEFPlatformSubsystem>();
	UEFMessageSubsystem* Messages = State->Game->GetSubsystem<UEFMessageSubsystem>();
	if (!TestNotNull(TEXT("the platform subsystem exists"), Platform) || !TestNotNull(TEXT("the message subsystem exists"), Messages))
	{
		return false;
	}
	UEFLocalPlatformProvider* Local = EFPlatformTestPrivate::IsolateLocalProvider(Platform, State->Game->Get());
	if (!TestNotNull(TEXT("the Local provider exists"), Local))
	{
		return false;
	}
	TestEqual(TEXT("Local serves achievements"), Platform->GetProviderName(EEFPlatformFeature::Achievements), UEFLocalPlatformProvider::ProviderName);
	TestEqual(TEXT("Local serves cloud storage when allowed"), Platform->GetProviderName(EEFPlatformFeature::CloudStorage), UEFLocalPlatformProvider::ProviderName);
	TestTrue(TEXT("the Local user is signed in"), Platform->GetLocalUser().bSignedIn);

	// The game says FirstBlood; the Local provider stores it as ACH_FIRST_BLOOD.
	State->Catalog.Reset(NewObject<UEFPlatformCatalog>());
	FEFPlatformCatalogEntry& Entry = State->Catalog->Achievements.AddDefaulted_GetRef();
	Entry.Id = TEXT("FirstBlood");
	Entry.DisplayName = FText::FromString(TEXT("First Blood"));
	Entry.ProviderIds.Add(UEFLocalPlatformProvider::ProviderName, TEXT("ACH_FIRST_BLOOD"));
	TestEqual(TEXT("the catalog is consistent"), State->Catalog->FindProblems().Num(), 0);
	Platform->SetCatalogOverride(State->Catalog.Get());
	TestEqual(TEXT("ids map to the provider"), Platform->ToProviderId(EEFPlatformCatalogKind::Achievement, TEXT("FirstBlood"), UEFLocalPlatformProvider::ProviderName), FString(TEXT("ACH_FIRST_BLOOD")));
	TestEqual(TEXT("unknown ids pass through"), Platform->ToProviderId(EEFPlatformCatalogKind::Stat, TEXT("Kills"), UEFLocalPlatformProvider::ProviderName), FString(TEXT("Kills")));

	State->UnlockListener = Messages->RegisterListener<FEFAchievementUnlockedMessage>(EFPlatformMessages::AchievementUnlocked.GetTag(),
		[State](FGameplayTag, const FEFAchievementUnlockedMessage& Message) { State->UnlockMessages.Add(Message.AchievementId); });

	Platform->UnlockAchievement(TEXT("FirstBlood"), [State](const FEFResult& Result) { State->Unlock = Result; });
	TestFalse(TEXT("a callback never runs before its call returns"), State->Unlock.IsSet());
	Platform->UpdateStat(TEXT("Kills"), 3, EEFStatUpdateMode::Increment, [State](const FEFResult& Result) { State->Kills1 = Result; });
	Platform->UpdateStat(TEXT("Kills"), 2, EEFStatUpdateMode::Increment, [State](const FEFResult& Result) { State->Kills2 = Result; });
	Platform->SubmitScore(TEXT("HighScore"), 100, [State](const FEFResult& Result) { State->Score1 = Result; });
	Platform->SubmitScore(TEXT("HighScore"), 250, [State](const FEFResult& Result) { State->Score2 = Result; });
	Platform->WriteCloudFile(TEXT("save.bin"), { 1, 2, 3 }, [State](const FEFResult& Result) { State->Write = Result; });

	ADD_LATENT_AUTOMATION_COMMAND(FEFWaitUntilCommand(this, TEXT("the writes"), [State]()
	{
		return State->Unlock.IsSet() && State->Kills1.IsSet() && State->Kills2.IsSet() && State->Score1.IsSet() && State->Score2.IsSet() && State->Write.IsSet();
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FEFRunLatentCommand([this, State]()
	{
		TestTrue(TEXT("unlock succeeds"), State->Unlock.IsSet() && State->Unlock->IsSuccess());
		TestTrue(TEXT("stat updates succeed"), State->Kills1.IsSet() && State->Kills1->IsSuccess() && State->Kills2.IsSet() && State->Kills2->IsSuccess());
		TestTrue(TEXT("scores are submitted"), State->Score1.IsSet() && State->Score1->IsSuccess() && State->Score2.IsSet() && State->Score2->IsSuccess());
		TestTrue(TEXT("the cloud file is written"), State->Write.IsSet() && State->Write->IsSuccess());
		TestTrue(TEXT("the unlock was announced on the message bus under the game's id"), State->UnlockMessages.Num() == 1 && State->UnlockMessages[0] == FName(TEXT("FirstBlood")));

		UEFPlatformSubsystem* LatentPlatform = State->Game->GetSubsystem<UEFPlatformSubsystem>();
		LatentPlatform->QueryAchievements([State](const FEFResult& Result, const TArray<FEFAchievementState>& Achievements) { State->AchievementsResult = Result; State->Achievements = Achievements; });
		LatentPlatform->QueryStats({ FName(TEXT("Kills")), FName(TEXT("Deaths")) }, [State](const FEFResult& Result, const TArray<FEFStatValue>& Stats) { State->StatsResult = Result; State->Stats = Stats; });
		LatentPlatform->QueryLeaderboard(TEXT("HighScore"), EEFLeaderboardRange::Top, 10, [State](const FEFResult& Result, const TArray<FEFLeaderboardEntry>& Board) { State->BoardResult = Result; State->Board = Board; });
		LatentPlatform->ReadCloudFile(TEXT("save.bin"), [State](const FEFResult& Result, const TArray<uint8>& Data) { State->ReadResult = Result; State->ReadData = Data; });
		LatentPlatform->ReadCloudFile(TEXT("missing.bin"), [State](const FEFResult& Result, const TArray<uint8>&) { State->MissingResult = Result; });
		LatentPlatform->ListCloudFiles([State](const FEFResult& Result, const TArray<FEFCloudFileInfo>& Files) { State->ListResult = Result; State->Files = Files; });
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FEFWaitUntilCommand(this, TEXT("the queries"), [State]()
	{
		return State->AchievementsResult.IsSet() && State->StatsResult.IsSet() && State->BoardResult.IsSet() && State->ReadResult.IsSet() && State->MissingResult.IsSet() && State->ListResult.IsSet();
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FEFRunLatentCommand([this, State]()
	{
		if (TestEqual(TEXT("one achievement is known"), State->Achievements.Num(), 1))
		{
			const FEFAchievementState& Achievement = State->Achievements[0];
			TestEqual(TEXT("it comes back under the game's id"), Achievement.Id, FName(TEXT("FirstBlood")));
			TestEqual(TEXT("with the provider's id"), Achievement.ProviderId, FString(TEXT("ACH_FIRST_BLOOD")));
			TestTrue(TEXT("unlocked, with full progress and a time"), Achievement.bUnlocked && Achievement.Progress == 1.f && Achievement.UnlockTimeUtc.GetTicks() > 0);
			TestEqual(TEXT("the catalog supplies the display name"), Achievement.DisplayName.ToString(), FString(TEXT("First Blood")));
		}
		if (TestEqual(TEXT("both stats are returned"), State->Stats.Num(), 2))
		{
			TestEqual(TEXT("increments add up"), State->Stats[0].Value, int64(5));
			TestEqual(TEXT("a stat never written reads as 0"), State->Stats[1].Value, int64(0));
			TestEqual(TEXT("stats come back under the game's id"), State->Stats[0].Id, FName(TEXT("Kills")));
		}
		if (TestEqual(TEXT("both scores are on the board"), State->Board.Num(), 2))
		{
			TestTrue(TEXT("best first"), State->Board[0].Score == 250 && State->Board[0].Rank == 1 && State->Board[1].Score == 100 && State->Board[1].Rank == 2);
			TestTrue(TEXT("entries are the local user's"), State->Board[0].bIsLocalUser);
		}
		TestTrue(TEXT("the cloud file reads back"), State->ReadResult->IsSuccess() && State->ReadData == TArray<uint8>({ 1, 2, 3 }));
		TestTrue(TEXT("a missing cloud file fails with NotFound"), !State->MissingResult->IsSuccess() && State->MissingResult->HasCode(EFPlatformResultCodes::NotFound));
		TestTrue(TEXT("the cloud file is listed"), State->Files.Num() == 1 && State->Files[0].Name == TEXT("save.bin") && State->Files[0].SizeBytes == 3);
		TestTrue(TEXT("the profile was written to the test folder"), IFileManager::Get().FileExists(*FPaths::Combine(State->Settings->Root, TEXT("Profile.json"))));

		State->UnlockListener.Unregister();
		State->Game.Reset();
		State->Settings.Reset();
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEFPlatformQueueTest, "ExtendedFoundation.Platform.QueuedWrites",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEFPlatformQueueTest::RunTest(const FString& Parameters)
{
	struct FState
	{
		TSharedPtr<EFPlatformTestPrivate::FScopedPlatformTestSettings> Settings;
		TSharedPtr<EFTest::FScopedGameInstance> Game;
		TOptional<FEFResult> Dropped;
		TOptional<FEFResult> Kept1;
		TOptional<FEFResult> Kept2;
		TOptional<FEFResult> Query;
	};
	const TSharedRef<FState> State = MakeShared<FState>();
	State->Settings = MakeShared<EFPlatformTestPrivate::FScopedPlatformTestSettings>();
	GetMutableDefault<UEFPlatformSettings>()->MaxQueuedWrites = 2;
	State->Game = MakeShared<EFTest::FScopedGameInstance>();

	UEFPlatformSubsystem* Platform = State->Game->GetSubsystem<UEFPlatformSubsystem>();
	if (!TestNotNull(TEXT("the platform subsystem exists"), Platform))
	{
		return false;
	}
	UEFLocalPlatformProvider* Local = EFPlatformTestPrivate::IsolateLocalProvider(Platform, State->Game->Get());
	if (!TestNotNull(TEXT("the Local provider exists"), Local))
	{
		return false;
	}

	// No provider at all: writes wait, reads fail.
	Platform->UnregisterProvider(Local);
	TestFalse(TEXT("nothing serves achievements"), Platform->IsFeatureAvailable(EEFPlatformFeature::Achievements));

	AddExpectedMessage(TEXT("the write queue is full"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1, false);
	Platform->UnlockAchievement(TEXT("A"), [State](const FEFResult& Result) { State->Dropped = Result; });
	Platform->UnlockAchievement(TEXT("B"), [State](const FEFResult& Result) { State->Kept1 = Result; });
	Platform->SubmitScore(TEXT("Board"), 10, [State](const FEFResult& Result) { State->Kept2 = Result; });
	TestEqual(TEXT("the queue holds at most two writes"), Platform->GetQueuedWriteCount(), 2);
	Platform->QueryAchievements([State](const FEFResult& Result, const TArray<FEFAchievementState>&) { State->Query = Result; });

	ADD_LATENT_AUTOMATION_COMMAND(FEFWaitUntilCommand(this, TEXT("the dropped write and the read"), [State]() { return State->Dropped.IsSet() && State->Query.IsSet(); }));
	ADD_LATENT_AUTOMATION_COMMAND(FEFRunLatentCommand([this, State]()
	{
		TestTrue(TEXT("the oldest write is dropped with QueueFull"), State->Dropped->HasCode(EFPlatformResultCodes::QueueFull));
		TestTrue(TEXT("reads are not queued"), State->Query->HasCode(EFResultCodes::NotAvailable));
		TestFalse(TEXT("queued writes wait"), State->Kept1.IsSet() || State->Kept2.IsSet());

		// A provider arrives: the queue drains on its own.
		UEFPlatformSubsystem* LatentPlatform = State->Game->GetSubsystem<UEFPlatformSubsystem>();
		LatentPlatform->RegisterProvider(State->Game->Get()->GetSubsystem<UEFLocalPlatformProvider>());
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FEFWaitUntilCommand(this, TEXT("the queued writes"), [State]() { return State->Kept1.IsSet() && State->Kept2.IsSet(); }));
	ADD_LATENT_AUTOMATION_COMMAND(FEFRunLatentCommand([this, State]()
	{
		TestTrue(TEXT("queued writes are sent once a provider can take them"), State->Kept1->IsSuccess() && State->Kept2->IsSuccess());
		TestEqual(TEXT("the queue is empty"), State->Game->GetSubsystem<UEFPlatformSubsystem>()->GetQueuedWriteCount(), 0);
		State->Game.Reset();
		State->Settings.Reset();
	}));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
