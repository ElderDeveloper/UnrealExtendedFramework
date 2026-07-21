// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "ExtendedSteamSharedModule.h"
#include "Matchmaking/ESteamMatchmakingSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

// Steam may genuinely be initialized when tests run interactively (auto-init in the editor),
// so the test branches: strict offline defaults when Steam is down, weak invariants when up.
// The subsystem is constructed directly without Initialize — every method is guarded by
// IsSteamAvailable(), which only consults the shared module.
namespace ESteamMatchmakingTestHelpers
{
	bool IsSteamClientUp()
	{
		return FExtendedSteamSharedModule::IsModuleAvailable()
			&& FExtendedSteamSharedModule::Get().IsSteamClientInitialized();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESteamMatchmakingOfflineStateTest,
	"UnrealExtendedSteam.Matchmaking.OfflineState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FESteamMatchmakingOfflineStateTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	UESteamMatchmakingSubsystem* Matchmaking = NewObject<UESteamMatchmakingSubsystem>(GameInstance);
	TestNotNull(TEXT("Matchmaking subsystem constructed"), Matchmaking);

	const bool bSteamUp = ESteamMatchmakingTestHelpers::IsSteamClientUp();
	const FESteamId FakeLobby(76561198000000000ull);

	if (bSteamUp)
	{
		// Weak invariants only: no live lobby is available in an automation run.
		TestEqual(TEXT("Unknown lobby has no members while Steam is up"), Matchmaking->GetNumLobbyMembers(FESteamId()), 0);
		TestFalse(TEXT("Unknown lobby has no owner while Steam is up"), Matchmaking->GetLobbyOwner(FESteamId()).IsValid());
		TestTrue(TEXT("Unknown lobby data is empty while Steam is up"), Matchmaking->GetLobbyData(FESteamId(), TEXT("Key")).IsEmpty());
		return true;
	}

	// The unavailable guards log a standard warning; that is the expected behavior under test.
	AddExpectedMessage(TEXT("Steam is not available"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 0, false);

	TestFalse(TEXT("CreateLobby fails offline"), Matchmaking->CreateLobby(EESteamLobbyType::Public, 4));
	TestFalse(TEXT("JoinLobby fails offline"), Matchmaking->JoinLobby(FakeLobby));
	TestFalse(TEXT("RequestLobbyList fails offline"), Matchmaking->RequestLobbyList());
	TestFalse(TEXT("InviteUserToLobby fails offline"), Matchmaking->InviteUserToLobby(FakeLobby, FakeLobby));

	TestTrue(TEXT("GetLobbyData is empty offline"), Matchmaking->GetLobbyData(FakeLobby, TEXT("Key")).IsEmpty());
	TestFalse(TEXT("SetLobbyData fails offline"), Matchmaking->SetLobbyData(FakeLobby, TEXT("Key"), TEXT("Value")));
	TestFalse(TEXT("DeleteLobbyData fails offline"), Matchmaking->DeleteLobbyData(FakeLobby, TEXT("Key")));
	TestTrue(TEXT("GetLobbyMemberData is empty offline"), Matchmaking->GetLobbyMemberData(FakeLobby, FakeLobby, TEXT("Key")).IsEmpty());

	TMap<FString, FString> AllData;
	TestFalse(TEXT("GetAllLobbyData fails offline"), Matchmaking->GetAllLobbyData(FakeLobby, AllData));
	TestEqual(TEXT("GetAllLobbyData outputs an empty map offline"), AllData.Num(), 0);

	TestEqual(TEXT("GetNumLobbyMembers is 0 offline"), Matchmaking->GetNumLobbyMembers(FakeLobby), 0);
	TestFalse(TEXT("GetLobbyMemberByIndex is invalid offline"), Matchmaking->GetLobbyMemberByIndex(FakeLobby, 0).IsValid());
	TestFalse(TEXT("GetLobbyOwner is invalid offline"), Matchmaking->GetLobbyOwner(FakeLobby).IsValid());
	TestFalse(TEXT("SetLobbyOwner fails offline"), Matchmaking->SetLobbyOwner(FakeLobby, FakeLobby));
	TestFalse(TEXT("SetLobbyType fails offline"), Matchmaking->SetLobbyType(FakeLobby, EESteamLobbyType::Private));
	TestFalse(TEXT("SetLobbyJoinable fails offline"), Matchmaking->SetLobbyJoinable(FakeLobby, true));
	TestFalse(TEXT("SetLobbyMemberLimit fails offline"), Matchmaking->SetLobbyMemberLimit(FakeLobby, 4));
	TestEqual(TEXT("GetLobbyMemberLimit is 0 offline"), Matchmaking->GetLobbyMemberLimit(FakeLobby), 0);

	TestFalse(TEXT("SendLobbyChatMessage fails offline"), Matchmaking->SendLobbyChatMessage(FakeLobby, TEXT("Hello")));

	// Newer surface: lobby data request, game-server association, linked lobbies and favorites.
	TestFalse(TEXT("RequestLobbyData fails offline"), Matchmaking->RequestLobbyData(FakeLobby));
	TestFalse(TEXT("SetLinkedLobby fails offline"), Matchmaking->SetLinkedLobby(FakeLobby, FakeLobby));

	// Setter no-ops must be safe offline (no return value to assert).
	Matchmaking->SetLobbyGameServer(FakeLobby, TEXT("127.0.0.1"), 27015, FESteamId());
	Matchmaking->AddRequestLobbyListNearValueFilter(TEXT("skill"), 100);
	Matchmaking->AddRequestLobbyListFilterSlotsAvailable(2);
	Matchmaking->AddRequestLobbyListCompatibleMembersFilter(FakeLobby);

	FString ServerIp = TEXT("dirty");
	int32 ServerPort = 42;
	FESteamId ServerId(123ull);
	TestFalse(TEXT("GetLobbyGameServer fails offline"), Matchmaking->GetLobbyGameServer(FakeLobby, ServerIp, ServerPort, ServerId));
	TestTrue(TEXT("GetLobbyGameServer resets its ip out value"), ServerIp.IsEmpty());
	TestEqual(TEXT("GetLobbyGameServer resets its port out value"), ServerPort, 0);
	TestFalse(TEXT("GetLobbyGameServer resets its server id out value"), ServerId.IsValid());

	TestEqual(TEXT("GetFavoriteGameCount is 0 offline"), Matchmaking->GetFavoriteGameCount(), 0);

	int32 AppId = 5;
	FString FavIp = TEXT("dirty");
	int32 ConnPort = 5;
	int32 QueryPort = 5;
	int32 Flags = 5;
	int32 LastPlayed = 5;
	TestFalse(TEXT("GetFavoriteGame fails offline"), Matchmaking->GetFavoriteGame(0, AppId, FavIp, ConnPort, QueryPort, Flags, LastPlayed));
	TestEqual(TEXT("GetFavoriteGame resets its out values"), AppId + ConnPort + QueryPort + Flags + LastPlayed, 0);
	TestTrue(TEXT("GetFavoriteGame resets its ip out value"), FavIp.IsEmpty());

	TestEqual(TEXT("AddFavoriteGame returns -1 offline"), Matchmaking->AddFavoriteGame(480, TEXT("127.0.0.1"), 27015, 27016, 1, 0), -1);
	TestFalse(TEXT("RemoveFavoriteGame fails offline"), Matchmaking->RemoveFavoriteGame(480, TEXT("127.0.0.1"), 27015, 27016, 1));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
