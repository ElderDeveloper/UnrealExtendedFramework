// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "Broadcast/ESteamWebBroadcastSubsystem.h"
#include "Community/ESteamWebCommunitySubsystem.h"
#include "EconMarket/ESteamWebEconMarketSubsystem.h"
#include "GameInventory/ESteamWebGameInventorySubsystem.h"
#include "GameServerStats/ESteamWebGameServerStatsSubsystem.h"
#include "LobbyMatchmaking/ESteamWebLobbyMatchmakingSubsystem.h"
#include "PublishedFile/ESteamWebPublishedFileSubsystem.h"
#include "PublishedItemSearch/ESteamWebPublishedItemSearchSubsystem.h"
#include "PublishedItemVoting/ESteamWebPublishedItemVotingSubsystem.h"
#include "RemoteStorage/ESteamWebRemoteStorageSubsystem.h"
#include "UserAuth/ESteamWebUserAuthSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

// Deterministic construction smoke test for the batch-3 Steam Web interface subsystems.
// No network traffic: objects are only constructed, no request is ever sent.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESteamWebBatch3ConstructionTest,
	"UnrealExtendedSteam.Web.Batch3.Construction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FESteamWebBatch3ConstructionTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	TestNotNull(TEXT("GameInstance outer"), GameInstance);

	TestNotNull(TEXT("EconMarket subsystem"), NewObject<UESteamWebEconMarketSubsystem>(GameInstance));
	TestNotNull(TEXT("GameInventory subsystem"), NewObject<UESteamWebGameInventorySubsystem>(GameInstance));
	TestNotNull(TEXT("LobbyMatchmaking subsystem"), NewObject<UESteamWebLobbyMatchmakingSubsystem>(GameInstance));
	TestNotNull(TEXT("PublishedFile subsystem"), NewObject<UESteamWebPublishedFileSubsystem>(GameInstance));
	TestNotNull(TEXT("PublishedItemSearch subsystem"), NewObject<UESteamWebPublishedItemSearchSubsystem>(GameInstance));
	TestNotNull(TEXT("PublishedItemVoting subsystem"), NewObject<UESteamWebPublishedItemVotingSubsystem>(GameInstance));
	TestNotNull(TEXT("Community subsystem"), NewObject<UESteamWebCommunitySubsystem>(GameInstance));
	TestNotNull(TEXT("GameServerStats subsystem"), NewObject<UESteamWebGameServerStatsSubsystem>(GameInstance));
	TestNotNull(TEXT("RemoteStorage subsystem"), NewObject<UESteamWebRemoteStorageSubsystem>(GameInstance));
	TestNotNull(TEXT("UserAuth subsystem"), NewObject<UESteamWebUserAuthSubsystem>(GameInstance));
	TestNotNull(TEXT("Broadcast subsystem"), NewObject<UESteamWebBroadcastSubsystem>(GameInstance));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
