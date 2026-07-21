// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "ExtendedSteamSharedModule.h"
#include "Stats/ESteamUserStatsSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESteamUserStatsOfflineStateTest,
	"UnrealExtendedSteam.Stats.OfflineState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FESteamUserStatsOfflineStateTest::RunTest(const FString& Parameters)
{
	// Constructed directly (Initialize is intentionally not called): the subsystem's
	// availability guards must make every entry point safe and offline-correct anyway.
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	UESteamUserStatsSubsystem* Subsystem = NewObject<UESteamUserStatsSubsystem>(GameInstance);
	TestNotNull(TEXT("Subsystem constructed"), Subsystem);
	if (!Subsystem)
	{
		return false;
	}

	const bool bSteamUp = FExtendedSteamSharedModule::IsModuleAvailable()
		&& FExtendedSteamSharedModule::Get().IsSteamClientInitialized();

	if (!bSteamUp)
	{
		bool bUnlocked = true;
		TestEqual(TEXT("GetNumAchievements returns 0 offline"), Subsystem->GetNumAchievements(), 0);
		TestTrue(TEXT("GetAchievementName returns empty offline"), Subsystem->GetAchievementName(0).IsEmpty());
		TestFalse(TEXT("GetAchievement fails offline"), Subsystem->GetAchievement(TEXT("ACH_X"), bUnlocked));
		TestFalse(TEXT("SetAchievement fails offline"), Subsystem->SetAchievement(TEXT("ACH_X")));
		TestFalse(TEXT("StoreStats fails offline"), Subsystem->StoreStats());
		TestFalse(TEXT("FindLeaderboard fails offline"), Subsystem->FindLeaderboard(TEXT("x")));
		TestFalse(TEXT("UploadLeaderboardScore fails offline"), Subsystem->UploadLeaderboardScore(0, 0, false));

		// Newly added surface: every async op refuses to start and every getter is offline-safe.
		TestFalse(TEXT("GetNumberOfCurrentPlayers fails offline"), Subsystem->GetNumberOfCurrentPlayers());
		TestFalse(TEXT("RequestGlobalStats fails offline"), Subsystem->RequestGlobalStats(1));
		TestFalse(TEXT("RequestGlobalAchievementPercentages fails offline"), Subsystem->RequestGlobalAchievementPercentages());
		TestFalse(TEXT("AttachLeaderboardUGC fails offline"), Subsystem->AttachLeaderboardUGC(0, 0));
		TestFalse(TEXT("DownloadLeaderboardEntriesForUsers fails offline"), Subsystem->DownloadLeaderboardEntriesForUsers(0, TArray<FESteamId>()));
		TestFalse(TEXT("UploadLeaderboardScoreWithDetails fails offline"), Subsystem->UploadLeaderboardScoreWithDetails(0, 0, false, TArray<int32>()));
		TestEqual(TEXT("GetLeaderboardEntryCount returns 0 offline"), Subsystem->GetLeaderboardEntryCount(0), 0);
		TestTrue(TEXT("GetLeaderboardName returns empty offline"), Subsystem->GetLeaderboardName(0).IsEmpty());

		int64 GlobalInt = 42;
		TestFalse(TEXT("GetGlobalStatInt fails offline"), Subsystem->GetGlobalStatInt(TEXT("g_int"), GlobalInt));
		TestEqual(TEXT("GetGlobalStatInt resets the out value"), GlobalInt, static_cast<int64>(0));

		double GlobalFloat = 42.0;
		TestFalse(TEXT("GetGlobalStatFloat fails offline"), Subsystem->GetGlobalStatFloat(TEXT("g_float"), GlobalFloat));
		TestEqual(TEXT("GetGlobalStatFloat resets the out value"), GlobalFloat, 0.0);

		float Percent = 42.0f;
		TestFalse(TEXT("GetAchievementAchievedPercent fails offline"), Subsystem->GetAchievementAchievedPercent(TEXT("ACH_X"), Percent));

		FString MostName;
		float MostPercent = 0.0f;
		bool bMostAchieved = false;
		TestEqual(TEXT("GetMostAchievedAchievementInfo returns -1 offline"), Subsystem->GetMostAchievedAchievementInfo(MostName, MostPercent, bMostAchieved), -1);
	}
	else
	{
		// Steam is live on this machine/run: only assert weak, non-flaky invariants.
		TestTrue(TEXT("GetNumAchievements is non-negative"), Subsystem->GetNumAchievements() >= 0);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
