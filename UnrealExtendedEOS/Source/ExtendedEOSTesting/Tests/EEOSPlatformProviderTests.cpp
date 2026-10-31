// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Algo/Reverse.h"
#include "Engine/GameInstance.h"
#include "Platform/EEOSPlatformProvider.h"
#include "Platform/EFPlatformProvider.h"
#include "Platform/EFPlatformTypes.h"
#include "Result/EFResult.h"
#include "Shared/EEOSSettings.h"

// ═══════════════════════════════════════════════════════════════════════════
// The EOS platform provider without a running EOS platform
// ═══════════════════════════════════════════════════════════════════════════
//
// The provider is constructed without Initialize, like the other EOS initial-state tests: no EOS
// subsystem starts and no EOS platform is created, so this runs without credentials and shows
// what the platform layer gets from EOS before it has started or anyone has logged in.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEEOSPlatformProviderWithoutEOSTest,
	"UnrealExtendedEOS.Platform.Provider.WithoutEOS",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEEOSPlatformProviderWithoutEOSTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UEEOSPlatformProvider* Provider = NewObject<UEEOSPlatformProvider>(GameInstance);
	if (!TestNotNull(TEXT("the provider can be created"), Provider))
	{
		return false;
	}

	TestEqual(TEXT("the provider is named EOS"), Provider->GetProviderName(), FName(TEXT("EOS")));

	for (const EEFPlatformFeature Feature : EFPlatform::AllFeatures())
	{
		TestFalse(*FString::Printf(TEXT("%s is unavailable while EOS is not running"), EFPlatform::FeatureToString(Feature)), Provider->IsFeatureAvailable(Feature));
		TestFalse(*FString::Printf(TEXT("CheckFeature(%s) fails"), EFPlatform::FeatureToString(Feature)), Provider->CheckFeature(Feature).IsSuccess());
	}

	// Supported means "served once EOS is up", so the Local provider does not take the feature over.
	const UEEOSSettings* Settings = GetDefault<UEEOSSettings>();
	TestTrue(TEXT("stats are supported"), Provider->SupportsFeature(EEFPlatformFeature::Stats));
	TestTrue(TEXT("cloud storage is supported"), Provider->SupportsFeature(EEFPlatformFeature::CloudStorage));
	TestTrue(TEXT("presence is supported"), Provider->SupportsFeature(EEFPlatformFeature::Presence));
	TestTrue(TEXT("achievements are supported while enabled in the settings"), Provider->SupportsFeature(EEFPlatformFeature::Achievements) == Settings->bEnableAchievements);
	TestTrue(TEXT("leaderboards are supported while enabled in the settings"), Provider->SupportsFeature(EEFPlatformFeature::Leaderboards) == Settings->bEnableLeaderboards);

	const FEFPlatformUser User = Provider->GetLocalUser();
	TestEqual(TEXT("the user is reported by EOS"), User.Provider, FName(TEXT("EOS")));
	TestFalse(TEXT("nobody is signed in"), User.bSignedIn);
	TestTrue(TEXT("there is no Product User ID"), User.UserId.IsEmpty());

	// Every call answers exactly once, from EOS, with a failure that says why.
	TArray<FEFResult> Results;
	const auto Record = [&Results](const FEFResult& Result) { Results.Add(Result); };
	int32 Calls = 0;

	Provider->UnlockAchievement(TEXT("ACH_TEST"), Record); ++Calls;
	Provider->SetAchievementProgress(TEXT("ACH_TEST"), 0.5f, Record); ++Calls;
	Provider->SetAchievementProgress(TEXT("ACH_TEST"), 1.f, Record); ++Calls;
	Provider->QueryAchievements([&Results](const FEFResult& Result, const TArray<FEFAchievementState>&) { Results.Add(Result); }); ++Calls;

	FEFStatUpdate Update;
	Update.Id = TEXT("Kills");
	Update.ProviderId = TEXT("KILLS");
	Update.Value = 1;
	Provider->UpdateStats(TArray<FEFStatUpdate>{ Update }, Record); ++Calls;
	Provider->QueryStats(TArray<FString>{ FString(TEXT("KILLS")) }, [&Results](const FEFResult& Result, const TArray<FEFStatValue>&) { Results.Add(Result); }); ++Calls;

	Provider->SubmitScore(TEXT("HIGHSCORE"), 10, Record); ++Calls;
	const EEFLeaderboardRange Ranges[] = { EEFLeaderboardRange::Top, EEFLeaderboardRange::AroundUser, EEFLeaderboardRange::Friends };
	for (const EEFLeaderboardRange Range : Ranges)
	{
		Provider->QueryLeaderboard(TEXT("HIGHSCORE"), Range, 10, [&Results](const FEFResult& Result, const TArray<FEFLeaderboardEntry>&) { Results.Add(Result); }); ++Calls;
	}

	Provider->WriteCloudFile(TEXT("Save.bin"), TArray<uint8>{ 1, 2, 3 }, Record); ++Calls;
	Provider->ReadCloudFile(TEXT("Save.bin"), [&Results](const FEFResult& Result, const TArray<uint8>&) { Results.Add(Result); }); ++Calls;
	Provider->DeleteCloudFile(TEXT("Save.bin"), Record); ++Calls;
	Provider->ListCloudFiles([&Results](const FEFResult& Result, const TArray<FEFCloudFileInfo>&) { Results.Add(Result); }); ++Calls;

	Provider->SetPresence(TEXT("In the menu"), TMap<FString, FString>(), Record); ++Calls;

	TestEqual(TEXT("every call answered exactly once"), Results.Num(), Calls);
	for (const FEFResult& Result : Results)
	{
		TestFalse(*FString::Printf(TEXT("'%s' is a failure"), *Result.ToString()), Result.IsSuccess());
		TestEqual(*FString::Printf(TEXT("'%s' comes from EOS"), *Result.ToString()), Result.Source, FName(TEXT("EOS")));
		TestTrue(*FString::Printf(TEXT("'%s' says EOS is not available or nobody is signed in"), *Result.ToString()),
			Result.HasCode(EFResultCodes::NotAvailable) || Result.HasCode(EFPlatformResultCodes::NotSignedIn));
	}
	TestEqual(TEXT("no call is left waiting"), Provider->GetPendingCallCount(), 0);

	return true;
}

// ═══════════════════════════════════════════════════════════════════════════
// Mapping helpers
// ═══════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEEOSPlatformIngestAmountsTest,
	"UnrealExtendedEOS.Platform.Helpers.IngestAmounts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEEOSPlatformIngestAmountsTest::RunTest(const FString& Parameters)
{
	TArray<FEFStatUpdate> Updates;
	const auto Add = [&Updates](const TCHAR* Id, const TCHAR* ProviderId, int64 Value, EEFStatUpdateMode Mode)
	{
		FEFStatUpdate& Update = Updates.AddDefaulted_GetRef();
		Update.Id = FName(Id);
		Update.ProviderId = ProviderId;
		Update.Value = Value;
		Update.Mode = Mode;
	};

	Add(TEXT("Kills"), TEXT("kills"), 5, EEFStatUpdateMode::Set);
	Add(TEXT("Kills"), TEXT("KILLS"), 3, EEFStatUpdateMode::Increment);
	Add(TEXT("Wins"), TEXT(""), 2, EEFStatUpdateMode::Increment);
	Add(TEXT("Gold"), TEXT("GOLD"), 10000000000LL, EEFStatUpdateMode::Set);
	Add(TEXT("Debt"), TEXT("DEBT"), -10000000000LL, EEFStatUpdateMode::Increment);
	Add(TEXT("Debt"), TEXT("DEBT"), -10000000000LL, EEFStatUpdateMode::Increment);
	Add(TEXT("Best"), TEXT("BEST"), 7, EEFStatUpdateMode::Increment);
	Add(TEXT("Best"), TEXT("BEST"), 4, EEFStatUpdateMode::Set);

	const TMap<FString, int32> Amounts = EEOSPlatform::ToIngestAmounts(Updates);
	TestEqual(TEXT("one amount per stat"), Amounts.Num(), 5);
	TestEqual(TEXT("an increment adds to an earlier set of the same stat, whatever its case"), Amounts.FindRef(TEXT("KILLS")), 8);
	TestEqual(TEXT("the game id is used when there is no provider id"), Amounts.FindRef(TEXT("WINS")), 2);
	TestEqual(TEXT("large values clamp to int32"), Amounts.FindRef(TEXT("GOLD")), TNumericLimits<int32>::Max());
	TestEqual(TEXT("large negative sums clamp to int32"), Amounts.FindRef(TEXT("DEBT")), TNumericLimits<int32>::Min());
	TestEqual(TEXT("a later set replaces an earlier increment"), Amounts.FindRef(TEXT("BEST")), 4);
	for (const TPair<FString, int32>& Pair : Amounts)
	{
		TestTrue(*FString::Printf(TEXT("%s is upper case, as EOS stat names are"), *Pair.Key), Pair.Key.Equals(Pair.Key.ToUpper(), ESearchCase::CaseSensitive));
	}

	TestEqual(TEXT("ClampToInt32 keeps small values"), EEOSPlatform::ClampToInt32(-42), -42);
	TestEqual(TEXT("ClampToInt32 clamps large values"), EEOSPlatform::ClampToInt32(TNumericLimits<int64>::Max()), TNumericLimits<int32>::Max());
	return true;
}

namespace EEOSPlatformTests
{
	/** Ranks 1..Num with scores Num*10 down to 10; the entry at LocalIndex is the local user (INDEX_NONE for nobody). */
	TArray<FEFLeaderboardEntry> MakeBoard(int32 Num, int32 LocalIndex)
	{
		TArray<FEFLeaderboardEntry> Board;
		for (int32 Index = 0; Index < Num; ++Index)
		{
			FEFLeaderboardEntry& Entry = Board.AddDefaulted_GetRef();
			Entry.Rank = Index + 1;
			Entry.UserId = FString::Printf(TEXT("user%d"), Index);
			Entry.Score = (Num - Index) * 10;
			Entry.bIsLocalUser = Index == LocalIndex;
		}
		return Board;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEEOSPlatformLeaderboardHelpersTest,
	"UnrealExtendedEOS.Platform.Helpers.Leaderboards",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEEOSPlatformLeaderboardHelpersTest::RunTest(const FString& Parameters)
{
	using EEOSPlatformTests::MakeBoard;

	// SliceAroundUser
	{
		const TArray<FEFLeaderboardEntry> Middle = EEOSPlatform::SliceAroundUser(MakeBoard(20, 10), 5);
		if (TestEqual(TEXT("a window around a user in the middle has Count entries"), Middle.Num(), 5))
		{
			TestEqual(TEXT("the window is centred on the user"), Middle[2].Rank, 11);
			TestTrue(TEXT("the user is in the window"), Middle[2].bIsLocalUser);
		}

		const TArray<FEFLeaderboardEntry> Top = EEOSPlatform::SliceAroundUser(MakeBoard(20, 0), 5);
		if (TestEqual(TEXT("a window around the leader has Count entries"), Top.Num(), 5))
		{
			TestEqual(TEXT("the window starts at rank 1"), Top[0].Rank, 1);
		}

		const TArray<FEFLeaderboardEntry> Bottom = EEOSPlatform::SliceAroundUser(MakeBoard(20, 19), 5);
		if (TestEqual(TEXT("a window around the last entry has Count entries"), Bottom.Num(), 5))
		{
			TestEqual(TEXT("the window ends at the last rank"), Bottom.Last().Rank, 20);
		}

		TestEqual(TEXT("a short board is returned whole"), EEOSPlatform::SliceAroundUser(MakeBoard(3, 1), 10).Num(), 3);
		TestEqual(TEXT("no window without the user"), EEOSPlatform::SliceAroundUser(MakeBoard(20, INDEX_NONE), 5).Num(), 0);
	}

	// RankByScore
	{
		TArray<FEFLeaderboardEntry> Friends = MakeBoard(6, 5);
		Algo::Reverse(Friends);
		EEOSPlatform::RankByScore(Friends, 3);
		if (TestEqual(TEXT("at most Count entries are kept"), Friends.Num(), 3))
		{
			TestEqual(TEXT("the best score ranks first"), Friends[0].Score, int64(60));
			TestEqual(TEXT("ranks are numbered from 1"), Friends[0].Rank, 1);
			TestEqual(TEXT("ranks follow the scores"), Friends[1].Rank, 2);
			TestTrue(TEXT("the local user replaces the last entry kept"), Friends[2].bIsLocalUser);
			TestEqual(TEXT("the local user keeps their rank"), Friends[2].Rank, 6);
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
