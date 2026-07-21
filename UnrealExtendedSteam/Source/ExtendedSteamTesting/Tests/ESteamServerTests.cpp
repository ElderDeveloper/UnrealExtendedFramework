// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "GameServer/ESteamGameServerSubsystem.h"
#include "GameServerStats/ESteamGameServerStatsSubsystem.h"
#include "MatchmakingServers/ESteamMatchmakingServersSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

// The game server API is NEVER auto-initialized (FExtendedSteamSharedModule only brings it up on
// an explicit InitializeSteamGameServer call), and the subsystems below are constructed directly
// without Initialize — so their lifecycle delegates are unbound and their callback/query holders
// stay null. That makes every assertion here strict in ALL environments, including editors where
// the Steam CLIENT happens to be up.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESteamGameServerOfflineStateTest,
	"UnrealExtendedSteam.GameServer.OfflineState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FESteamGameServerOfflineStateTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	UESteamGameServerSubsystem* GameServer = NewObject<UESteamGameServerSubsystem>(GameInstance);
	TestNotNull(TEXT("GameServer subsystem constructed"), GameServer);

	// The unavailable guards log a standard warning; that is the expected behavior under test.
	AddExpectedMessage(TEXT("is not available"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 0, false);

	TestFalse(TEXT("Not logged on while the game server API is down"), GameServer->IsLoggedOn());
	TestFalse(TEXT("Not secure while the game server API is down"), GameServer->IsSecure());
	TestFalse(TEXT("Server Steam id is invalid while the game server API is down"), GameServer->GetServerSteamId().IsValid());
	TestTrue(TEXT("Public IP is empty while the game server API is down"), GameServer->GetPublicIP().IsEmpty());
	TestFalse(TEXT("Restart is not requested while the game server API is down"), GameServer->WasRestartRequested());
	TestFalse(TEXT("BUpdateUserData fails while the game server API is down"), GameServer->BUpdateUserData(FESteamId(1), TEXT("Player"), 10));

	// Configuration setters and session calls must no-op safely.
	GameServer->SetProduct(TEXT("DevilOfPlague"));
	GameServer->SetGameDescription(TEXT("Test description"));
	GameServer->SetModDir(TEXT("dop"));
	GameServer->SetDedicatedServer(true);
	GameServer->SetMaxPlayerCount(4);
	GameServer->SetBotPlayerCount(0);
	GameServer->SetServerName(TEXT("Test server"));
	GameServer->SetMapName(TEXT("TestMap"));
	GameServer->SetPasswordProtected(false);
	GameServer->SetSpectatorPort(0);
	GameServer->SetSpectatorServerName(TEXT("Spectate"));
	GameServer->SetGameTags(TEXT("coop,horror"));
	GameServer->SetGameData(TEXT("test"));
	GameServer->SetRegion(TEXT(""));
	GameServer->SetAdvertiseServerActive(false);
	GameServer->SetKeyValue(TEXT("rule"), TEXT("value"));
	GameServer->ClearAllKeyValues();
	GameServer->AssociateWithClan(FESteamId(1)); // no-op offline, broadcasts failure
	GameServer->ComputeNewPlayerCompatibility(FESteamId(1)); // no-op offline, broadcasts failure
	GameServer->LogOnAnonymous();
	GameServer->LogOn(TEXT("token"));
	GameServer->LogOff();

	TestFalse(TEXT("Still not logged on after no-op session calls"), GameServer->IsLoggedOn());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESteamServerBrowserOfflineStateTest,
	"UnrealExtendedSteam.ServerBrowser.OfflineState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FESteamServerBrowserOfflineStateTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	UESteamMatchmakingServersSubsystem* Browser = NewObject<UESteamMatchmakingServersSubsystem>(GameInstance);
	TestNotNull(TEXT("ServerBrowser subsystem constructed"), Browser);

	AddExpectedMessage(TEXT("is not available"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 0, false);

	// Without Initialize the query holder was never created, so requests fail regardless of
	// whether the Steam client is up.
	const TMap<FString, FString> NoFilters;
	TestFalse(TEXT("RequestInternetServerList fails without a query holder"), Browser->RequestInternetServerList(NoFilters));
	TestFalse(TEXT("RequestLANServerList fails without a query holder"), Browser->RequestLANServerList());
	TestFalse(TEXT("RequestFriendsServerList fails without a query holder"), Browser->RequestFriendsServerList(NoFilters));
	TestFalse(TEXT("RequestFavoritesServerList fails without a query holder"), Browser->RequestFavoritesServerList(NoFilters));
	TestFalse(TEXT("RequestHistoryServerList fails without a query holder"), Browser->RequestHistoryServerList(NoFilters));
	TestFalse(TEXT("RequestSpectatorServerList fails without a query holder"), Browser->RequestSpectatorServerList(480, NoFilters));
	TestFalse(TEXT("PingServer fails without a query holder"), Browser->PingServer(TEXT("127.0.0.1"), 27015));
	TestFalse(TEXT("ServerRules fails without a query holder"), Browser->ServerRules(TEXT("127.0.0.1"), 27015));
	TestFalse(TEXT("RequestPlayerDetails fails without a query holder"), Browser->RequestPlayerDetails(TEXT("127.0.0.1"), 27015));
	TestFalse(TEXT("PingServer rejects an invalid address"), Browser->PingServer(TEXT("not-an-ip"), 27015));
	TestFalse(TEXT("ServerRules rejects an out-of-range port"), Browser->ServerRules(TEXT("127.0.0.1"), 70000));

	// Cancelling with no active request must be safe.
	Browser->CancelServerListRequest();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESteamGameServerStatsOfflineStateTest,
	"UnrealExtendedSteam.GameServerStats.OfflineState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FESteamGameServerStatsOfflineStateTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	UESteamGameServerStatsSubsystem* Stats = NewObject<UESteamGameServerStatsSubsystem>(GameInstance);
	TestNotNull(TEXT("GameServerStats subsystem constructed"), Stats);

	AddExpectedMessage(TEXT("is not available"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 0, false);

	const FESteamId User(76561197960265729ull);
	TestFalse(TEXT("RequestUserStats fails while the game server API is down"), Stats->RequestUserStats(User));
	TestFalse(TEXT("StoreUserStats fails while the game server API is down"), Stats->StoreUserStats(User));

	int32 IntValue = 42;
	TestFalse(TEXT("GetUserStatInt fails while the game server API is down"), Stats->GetUserStatInt(User, TEXT("stat_int"), IntValue));
	TestEqual(TEXT("GetUserStatInt resets the out value"), IntValue, 0);

	float FloatValue = 42.0f;
	TestFalse(TEXT("GetUserStatFloat fails while the game server API is down"), Stats->GetUserStatFloat(User, TEXT("stat_float"), FloatValue));
	TestEqual(TEXT("GetUserStatFloat resets the out value"), FloatValue, 0.0f);

	bool bAchieved = true;
	TestFalse(TEXT("GetUserAchievement fails while the game server API is down"), Stats->GetUserAchievement(User, TEXT("ACH_TEST"), bAchieved));
	TestFalse(TEXT("GetUserAchievement resets the out value"), bAchieved);

	TestFalse(TEXT("SetUserStatInt fails while the game server API is down"), Stats->SetUserStatInt(User, TEXT("stat_int"), 1));
	TestFalse(TEXT("SetUserStatFloat fails while the game server API is down"), Stats->SetUserStatFloat(User, TEXT("stat_float"), 1.0f));
	TestFalse(TEXT("UpdateUserAvgRateStat fails while the game server API is down"), Stats->UpdateUserAvgRateStat(User, TEXT("stat_avg"), 1.0f, 10.0));
	TestFalse(TEXT("SetUserAchievement fails while the game server API is down"), Stats->SetUserAchievement(User, TEXT("ACH_TEST")));
	TestFalse(TEXT("ClearUserAchievement fails while the game server API is down"), Stats->ClearUserAchievement(User, TEXT("ACH_TEST")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
