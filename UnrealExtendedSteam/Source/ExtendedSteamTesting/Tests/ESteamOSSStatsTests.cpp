// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "ExtendedSteamSharedModule.h"
#include "Shared/ESteamTypes.h"
#include "OnlineSubsystem.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Interfaces/OnlineAchievementsInterface.h"
#include "Interfaces/OnlineLeaderboardInterface.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESteamOSSAchievementsLeaderboardsOfflineStateTest,
	"UnrealExtendedSteam.OSS.AchievementsLeaderboards.OfflineState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FESteamOSSAchievementsLeaderboardsOfflineStateTest::RunTest(const FString& Parameters)
{
	IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get(ESTEAM_SUBSYSTEM);

	// Sampled AFTER Get: FOnlineSubsystemExtendedSteam::Init attempts a one-shot Steam client
	// initialization through the shared module, so Get itself can bring the client up.
	const bool bSteamClientUp = FExtendedSteamSharedModule::IsModuleAvailable()
		&& FExtendedSteamSharedModule::Get().IsSteamClientInitialized();

	if (!bSteamClientUp || Subsystem == nullptr)
	{
		// Without an initialized Steam client the subsystem never comes up; the null-subsystem
		// contract itself is covered by UnrealExtendedSteam.OSS.Registration.
		return true;
	}

	// With Steam up, both interfaces must be implemented (non-null) on this subsystem.
	const IOnlineAchievementsPtr Achievements = Subsystem->GetAchievementsInterface();
	TestTrue(TEXT("Achievements interface is implemented"), Achievements.IsValid());

	const IOnlineLeaderboardsPtr Leaderboards = Subsystem->GetLeaderboardsInterface();
	TestTrue(TEXT("Leaderboards interface is implemented"), Leaderboards.IsValid());

	const IOnlineIdentityPtr Identity = Subsystem->GetIdentityInterface();
	TestTrue(TEXT("Identity interface is implemented"), Identity.IsValid());
	if (!Achievements.IsValid() || !Identity.IsValid())
	{
		return false;
	}

	const FUniqueNetIdPtr LocalUserId = Identity->GetUniquePlayerId(0);
	TestTrue(TEXT("Local unique net id resolves"), LocalUserId.IsValid());
	if (!LocalUserId.IsValid())
	{
		return false;
	}

	// The achievement cache is only populated by QueryAchievements; before any query the cached
	// getters must report NotFound and leave the out parameter untouched (empty).
	TArray<FOnlineAchievement> CachedAchievements;
	const EOnlineCachedResult::Type CachedResult = Achievements->GetCachedAchievements(*LocalUserId, CachedAchievements);
	TestEqual(TEXT("GetCachedAchievements before any query returns NotFound"), CachedResult, EOnlineCachedResult::NotFound);
	TestEqual(TEXT("GetCachedAchievements before any query returns no rows"), CachedAchievements.Num(), 0);

	FOnlineAchievement SingleAchievement;
	TestEqual(TEXT("GetCachedAchievement before any query returns NotFound"),
		Achievements->GetCachedAchievement(*LocalUserId, TEXT("ACH_DOES_NOT_EXIST"), SingleAchievement), EOnlineCachedResult::NotFound);

	FOnlineAchievementDesc SingleDesc;
	TestEqual(TEXT("GetCachedAchievementDescription before any query returns NotFound"),
		Achievements->GetCachedAchievementDescription(TEXT("ACH_DOES_NOT_EXIST"), SingleDesc), EOnlineCachedResult::NotFound);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
