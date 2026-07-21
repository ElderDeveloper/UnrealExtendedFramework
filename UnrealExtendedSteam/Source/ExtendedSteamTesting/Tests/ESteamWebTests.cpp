// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Core/ESteamWebClient.h"
#include "Core/ESteamWebSettings.h"
#include "Core/ESteamWebTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

// Deterministic unit tests for the Steam Web API module — no network traffic:
// FESteamWebRequest::BuildUrl/BuildBody only build strings, they never send.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESteamWebSettingsDefaultsTest,
	"UnrealExtendedSteam.Web.Settings.Defaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FESteamWebSettingsDefaultsTest::RunTest(const FString& Parameters)
{
	const UESteamWebSettings* Settings = UESteamWebSettings::Get();
	TestNotNull(TEXT("Settings CDO"), Settings);
	TestEqual(TEXT("Default app id is Spacewar (480)"), Settings->AppId, 480);
	TestEqual(TEXT("Default request timeout"), Settings->RequestTimeoutSeconds, 10.0f);
	TestFalse(TEXT("Dev mode off by default"), Settings->bUseDevMode);
	TestFalse(TEXT("Verbose logging off by default"), Settings->bVerboseLogging);
	TestEqual(TEXT("Settings live under the shared Extended Framework category"),
		Settings->GetCategoryName(), FName(TEXT("Extended Framework")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESteamWebRequestUrlBuildingTest,
	"UnrealExtendedSteam.Web.Request.UrlBuilding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FESteamWebRequestUrlBuildingTest::RunTest(const FString& Parameters)
{
	// Public endpoint, no key: exact URL — param order is insertion order.
	{
		FESteamWebRequest Request;
		Request.Interface = TEXT("ISteamNews");
		Request.Method = TEXT("GetNewsForApp");
		Request.Version = 2;
		Request.bRequiresApiKey = false;
		Request.AddParam(TEXT("appid"), 480);
		Request.AddParam(TEXT("count"), 3);

		TestEqual(TEXT("Public GET url"), Request.BuildUrl(TEXT("")),
			FString(TEXT("https://api.steampowered.com/ISteamNews/GetNewsForApp/v2/?appid=480&count=3")));
	}

	// Partner host variant.
	{
		FESteamWebRequest Request;
		Request.Interface = TEXT("ISteamNews");
		Request.Method = TEXT("GetNewsForApp");
		Request.Version = 2;
		Request.bRequiresApiKey = false;
		Request.bUsePartnerHost = true;

		TestTrue(TEXT("Partner host prefix"),
			Request.BuildUrl(TEXT("")).StartsWith(TEXT("https://partner.steam-api.com/ISteamNews/GetNewsForApp/v2/")));
	}

	// Key injection: bRequiresApiKey with an explicit override must land in the query.
	{
		FESteamWebRequest Request;
		Request.Interface = TEXT("ISteamUser");
		Request.Method = TEXT("GetPlayerSummaries");
		Request.Version = 2;
		Request.AddParam(TEXT("steamids"), TEXT("76561197960287930"));

		const FString Url = Request.BuildUrl(TEXT("SECRET"));
		TestTrue(TEXT("Api key injected"), Url.Contains(TEXT("key=SECRET")));
		TestTrue(TEXT("Params kept alongside the key"), Url.Contains(TEXT("steamids=76561197960287930")));
	}

	// URL-encoding: a value with a space must be percent-encoded.
	{
		FESteamWebRequest Request;
		Request.Interface = TEXT("ISteamUser");
		Request.Method = TEXT("ResolveVanityURL");
		Request.Version = 1;
		Request.bRequiresApiKey = false;
		Request.AddParam(TEXT("vanityurl"), TEXT("some name"));

		TestEqual(TEXT("Space encoded as %20"), Request.BuildUrl(TEXT("")),
			FString(TEXT("https://api.steampowered.com/ISteamUser/ResolveVanityURL/v1/?vanityurl=some%20name")));
	}

	// Leaderboards interface name regression guard: the correct interface is ISteamLeaderboards.
	{
		FESteamWebRequest Request;
		Request.Interface = TEXT("ISteamLeaderboards");
		Request.Method = TEXT("GetLeaderboardsForGame");
		Request.Version = 2;
		Request.bUsePartnerHost = true;
		Request.AddParam(TEXT("appid"), 480);

		TestTrue(TEXT("Leaderboards uses ISteamLeaderboards on the partner host"),
			Request.BuildUrl(TEXT("SECRET")).Contains(
				TEXT("https://partner.steam-api.com/ISteamLeaderboards/GetLeaderboardsForGame/v2/")));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
