// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "ExtendedSteamSharedModule.h"
#include "Platform/EFPlatformProvider.h"
#include "Platform/EFPlatformSubsystem.h"
#include "Platform/ESteamPlatformProvider.h"
#include "Result/EFResult.h"
#include "Testing/EFTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESteamPlatformProviderOfflineTest,
	"UnrealExtendedSteam.Platform.OfflineProvider",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FESteamPlatformProviderOfflineTest::RunTest(const FString& Parameters)
{
	EFTest::FScopedGameInstance Game;
	UESteamPlatformProvider* Provider = Game.GetSubsystem<UESteamPlatformProvider>();
	if (!TestNotNull(TEXT("The Steam platform provider exists in a game instance"), Provider))
	{
		return false;
	}

	TestEqual(TEXT("The provider is named Steam"), Provider->GetProviderName().ToString(), FString(TEXT("Steam")));
	if (UEFPlatformSubsystem* Platform = Game.GetSubsystem<UEFPlatformSubsystem>())
	{
		TestTrue(TEXT("The provider registered with the platform subsystem"), Platform->FindProvider(UESteamPlatformProvider::ProviderName) == Provider);
	}
	else
	{
		AddError(TEXT("The platform subsystem does not exist in the test game instance"));
	}

	const bool bSteamUp = FExtendedSteamSharedModule::IsModuleAvailable()
		&& FExtendedSteamSharedModule::Get().IsSteamClientInitialized();
	if (bSteamUp)
	{
		// Steam is live on this machine: only check what holds either way, and touch nothing on the account.
		TestEqual(TEXT("The local user reports the Steam provider"), Provider->GetLocalUser().Provider.ToString(), FString(TEXT("Steam")));
		return true;
	}

	for (const EEFPlatformFeature Feature : EFPlatform::AllFeatures())
	{
		TestFalse(*FString::Printf(TEXT("%s is unavailable without the Steam client"), EFPlatform::FeatureToString(Feature)),
			Provider->IsFeatureAvailable(Feature));
	}

	const FEFPlatformUser User = Provider->GetLocalUser();
	TestEqual(TEXT("The local user reports the Steam provider"), User.Provider.ToString(), FString(TEXT("Steam")));
	TestFalse(TEXT("No Steam user is signed in"), User.bSignedIn);
	TestTrue(TEXT("No Steam user id is reported"), User.UserId.IsEmpty());

	// Every call answers exactly once, before it returns, with NotAvailable from Steam.
	TArray<FEFResult> Results;
	const auto OnResult = [&Results](const FEFResult& Result) { Results.Add(Result); };

	Provider->UnlockAchievement(TEXT("ACH_ESTEST"), OnResult);
	Provider->SetAchievementProgress(TEXT("ACH_ESTEST"), 0.5f, OnResult);
	Provider->QueryAchievements([&Results](const FEFResult& Result, const TArray<FEFAchievementState>&) { Results.Add(Result); });

	FEFStatUpdate Update;
	Update.ProviderId = TEXT("estest_stat");
	Update.Value = 1;
	Update.Mode = EEFStatUpdateMode::Increment;
	Provider->UpdateStats(TArray<FEFStatUpdate>{ Update }, OnResult);
	Provider->QueryStats(TArray<FString>{ FString(TEXT("estest_stat")) }, [&Results](const FEFResult& Result, const TArray<FEFStatValue>&) { Results.Add(Result); });

	Provider->SubmitScore(TEXT("estest_board"), 42, OnResult);
	Provider->QueryLeaderboard(TEXT("estest_board"), EEFLeaderboardRange::Top, 10,
		[&Results](const FEFResult& Result, const TArray<FEFLeaderboardEntry>&) { Results.Add(Result); });

	Provider->WriteCloudFile(TEXT("estest_file.bin"), TArray<uint8>{ 1, 2, 3 }, OnResult);
	Provider->ReadCloudFile(TEXT("estest_file.bin"), [&Results](const FEFResult& Result, const TArray<uint8>&) { Results.Add(Result); });
	Provider->DeleteCloudFile(TEXT("estest_file.bin"), OnResult);
	Provider->ListCloudFiles([&Results](const FEFResult& Result, const TArray<FEFCloudFileInfo>&) { Results.Add(Result); });

	TMap<FString, FString> Properties;
	Properties.Add(TEXT("estest_key"), TEXT("value"));
	Provider->SetPresence(TEXT("Testing"), Properties, OnResult);

	// Twelve calls above: three achievement, two stat, two leaderboard, four cloud, one presence.
	TestEqual(TEXT("Every call answered exactly once"), Results.Num(), 12);
	for (int32 Index = 0; Index < Results.Num(); ++Index)
	{
		const FEFResult& Result = Results[Index];
		TestFalse(*FString::Printf(TEXT("Call %d failed without Steam"), Index), Result.IsSuccess());
		TestEqual(*FString::Printf(TEXT("Call %d is answered by Steam"), Index), Result.Source.ToString(), FString(TEXT("Steam")));
		TestTrue(*FString::Printf(TEXT("Call %d reports NotAvailable (%s)"), Index, *Result.ToString()), Result.HasCode(EFResultCodes::NotAvailable));
	}
	TestEqual(TEXT("No call is left waiting for Steam"), Provider->GetPendingCallCount(), 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
