// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Apps/ESteamWebAppsSubsystem.h"
#include "Core/ESteamWebSettings.h"

void UESteamWebAppsSubsystem::GetAppList(const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ISteamApps"), TEXT("GetAppList"), 2, EESteamWebVerb::Get, /*bUsePartnerHost*/ false, /*bRequiresApiKey*/ false);

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebAppsSubsystem::GetServersAtAddress(FString Address, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ISteamApps"), TEXT("GetServersAtAddress"), 1, EESteamWebVerb::Get, /*bUsePartnerHost*/ false, /*bRequiresApiKey*/ false);

	Request.AddParam(TEXT("addr"), Address);

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebAppsSubsystem::UpToDateCheck(int32 AppId, int32 Version, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ISteamApps"), TEXT("UpToDateCheck"), 1, EESteamWebVerb::Get, /*bUsePartnerHost*/ false, /*bRequiresApiKey*/ false);

	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	Request.AddParam(TEXT("version"), Version);

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebAppsSubsystem::GetAppBetas(int32 AppId, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ISteamApps"), TEXT("GetAppBetas"), 1, EESteamWebVerb::Get, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebAppsSubsystem::GetAppBuilds(int32 AppId, int32 Count, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ISteamApps"), TEXT("GetAppBuilds"), 1, EESteamWebVerb::Get, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	if (Count > 0)
	{
		Request.AddParam(TEXT("count"), Count);
	}

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebAppsSubsystem::GetAppDepotVersions(int32 AppId, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ISteamApps"), TEXT("GetAppDepotVersions"), 1, EESteamWebVerb::Get, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebAppsSubsystem::GetPartnerAppListForWebAPIKey(FString TypeFilter, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ISteamApps"), TEXT("GetPartnerAppListForWebAPIKey"), 2, EESteamWebVerb::Get, /*bUsePartnerHost*/ true);

	if (!TypeFilter.IsEmpty())
	{
		Request.AddParam(TEXT("type_filter"), TypeFilter);
	}

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebAppsSubsystem::SetAppBuildLive(int32 AppId, int32 BuildId, FString BetaKey, FString Description, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ISteamApps"), TEXT("SetAppBuildLive"), 1, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	Request.AddParam(TEXT("buildid"), BuildId);
	Request.AddParam(TEXT("betakey"), BetaKey);
	if (!Description.IsEmpty())
	{
		Request.AddParam(TEXT("description"), Description);
	}

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebAppsSubsystem::GetPlayersBanned(int32 AppId, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ISteamApps"), TEXT("GetPlayersBanned"), 1, EESteamWebVerb::Get, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebAppsSubsystem::GetServerList(FString Filter, int32 Limit, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ISteamApps"), TEXT("GetServerList"), 1, EESteamWebVerb::Get, /*bUsePartnerHost*/ true);

	if (!Filter.IsEmpty())
	{
		Request.AddParam(TEXT("filter"), Filter);
	}
	if (Limit > 0)
	{
		Request.AddParam(TEXT("limit"), Limit);
	}

	SendWebRequest(MoveTemp(Request), OnResponse);
}
