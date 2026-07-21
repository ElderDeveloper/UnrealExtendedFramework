// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Core/ESteamWebClient.h"
#include "Core/ESteamWebTypes.h"
#include "PlayerService/ESteamWebPlayerServiceSubsystem.h"
#include "UserStats/ESteamWebUserStatsSubsystem.h"
#include "Apps/ESteamWebAppsSubsystem.h"
#include "InventoryService/ESteamWebInventoryServiceSubsystem.h"
#include "EconService/ESteamWebEconServiceSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

// Deterministic no-network tests for the batch-1 Steam Web interface subsystems
// (PlayerService, UserStats, Apps, InventoryService, EconService).
// The endpoint methods send HTTP, so only construction and request/URL building are unit-tested.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESteamWebBatch1ConstructionTest,
	"UnrealExtendedSteam.Web.Batch1.Construction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FESteamWebBatch1ConstructionTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	TestNotNull(TEXT("GameInstance outer"), GameInstance);

	TestNotNull(TEXT("PlayerService subsystem constructs"), NewObject<UESteamWebPlayerServiceSubsystem>(GameInstance));
	TestNotNull(TEXT("UserStats subsystem constructs"), NewObject<UESteamWebUserStatsSubsystem>(GameInstance));
	TestNotNull(TEXT("Apps subsystem constructs"), NewObject<UESteamWebAppsSubsystem>(GameInstance));
	TestNotNull(TEXT("InventoryService subsystem constructs"), NewObject<UESteamWebInventoryServiceSubsystem>(GameInstance));
	TestNotNull(TEXT("EconService subsystem constructs"), NewObject<UESteamWebEconServiceSubsystem>(GameInstance));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESteamWebBatch1UrlBuildingTest,
	"UnrealExtendedSteam.Web.Batch1.UrlBuilding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FESteamWebBatch1UrlBuildingTest::RunTest(const FString& Parameters)
{
	// One representative request per batch-1 subsystem, mirroring what the subsystem method builds.
	// BuildUrl/BuildBody never send — these assertions are pure string checks.

	// PlayerService: IPlayerService/GetRecentlyPlayedGames/v1 (user key).
	{
		FESteamWebRequest Request;
		Request.Interface = TEXT("IPlayerService");
		Request.Method = TEXT("GetRecentlyPlayedGames");
		Request.Version = 1;
		Request.AddParam(TEXT("steamid"), TEXT("76561197960287930"));
		Request.AddParam(TEXT("count"), 5);

		TestEqual(TEXT("PlayerService url"), Request.BuildUrl(TEXT("KEY")),
			FString(TEXT("https://api.steampowered.com/IPlayerService/GetRecentlyPlayedGames/v1/?key=KEY&steamid=76561197960287930&count=5")));
	}

	// UserStats: ISteamUserStats/GetNumberOfCurrentPlayers/v1 (no key).
	{
		FESteamWebRequest Request;
		Request.Interface = TEXT("ISteamUserStats");
		Request.Method = TEXT("GetNumberOfCurrentPlayers");
		Request.Version = 1;
		Request.bRequiresApiKey = false;
		Request.AddParam(TEXT("appid"), 480);

		TestEqual(TEXT("UserStats url"), Request.BuildUrl(TEXT("")),
			FString(TEXT("https://api.steampowered.com/ISteamUserStats/GetNumberOfCurrentPlayers/v1/?appid=480")));
	}

	// Apps: ISteamApps/GetAppBetas/v1 (partner host, publisher key).
	{
		FESteamWebRequest Request;
		Request.Interface = TEXT("ISteamApps");
		Request.Method = TEXT("GetAppBetas");
		Request.Version = 1;
		Request.bUsePartnerHost = true;
		Request.AddParam(TEXT("appid"), 480);

		TestEqual(TEXT("Apps partner url"), Request.BuildUrl(TEXT("PUBKEY")),
			FString(TEXT("https://partner.steam-api.com/ISteamApps/GetAppBetas/v1/?key=PUBKEY&appid=480")));
	}

	// InventoryService: IInventoryService/AddPromoItem/v1 (partner host, POST — params go in the body).
	{
		FESteamWebRequest Request;
		Request.Interface = TEXT("IInventoryService");
		Request.Method = TEXT("AddPromoItem");
		Request.Version = 1;
		Request.Verb = EESteamWebVerb::Post;
		Request.bUsePartnerHost = true;
		Request.AddParam(TEXT("appid"), 480);
		Request.AddParam(TEXT("steamid"), TEXT("76561197960287930"));
		Request.AddParam(TEXT("itemdefid"), 100);

		TestEqual(TEXT("InventoryService POST url has no query"), Request.BuildUrl(TEXT("PUBKEY")),
			FString(TEXT("https://partner.steam-api.com/IInventoryService/AddPromoItem/v1/")));
		TestEqual(TEXT("InventoryService POST body"), Request.BuildBody(TEXT("PUBKEY")),
			FString(TEXT("key=PUBKEY&appid=480&steamid=76561197960287930&itemdefid=100")));
	}

	// EconService: IEconService/GetTradeOffers/v1 (partner host, bools as 1/0).
	{
		FESteamWebRequest Request;
		Request.Interface = TEXT("IEconService");
		Request.Method = TEXT("GetTradeOffers");
		Request.Version = 1;
		Request.bUsePartnerHost = true;
		Request.AddParam(TEXT("get_sent_offers"), true);
		Request.AddParam(TEXT("active_only"), true);

		TestEqual(TEXT("EconService url"), Request.BuildUrl(TEXT("PUBKEY")),
			FString(TEXT("https://partner.steam-api.com/IEconService/GetTradeOffers/v1/?key=PUBKEY&get_sent_offers=1&active_only=1")));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
