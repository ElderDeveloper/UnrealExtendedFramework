// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "User/ESteamWebUserSubsystem.h"
#include "Core/ESteamWebSettings.h"

void UESteamWebUserSubsystem::GetPlayerSummaries(const TArray<FString>& SteamIds, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ISteamUser"), TEXT("GetPlayerSummaries"), 2);

	Request.AddParam(TEXT("steamids"), SteamIds);

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebUserSubsystem::GetFriendList(FString SteamId, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ISteamUser"), TEXT("GetFriendList"), 1);

	Request.AddParam(TEXT("steamid"), ResolveSteamId(SteamId));
	Request.AddParam(TEXT("relationship"), TEXT("friend"));

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebUserSubsystem::GetPlayerBans(const TArray<FString>& SteamIds, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ISteamUser"), TEXT("GetPlayerBans"), 1);

	Request.AddParam(TEXT("steamids"), SteamIds);

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebUserSubsystem::ResolveVanityUrl(FString VanityName, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ISteamUser"), TEXT("ResolveVanityURL"), 1);

	Request.AddParam(TEXT("vanityurl"), VanityName);

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebUserSubsystem::CheckAppOwnership(FString SteamId, int32 AppId, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ISteamUser"), TEXT("CheckAppOwnership"), 4, EESteamWebVerb::Get, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("steamid"), ResolveSteamId(SteamId));
	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebUserSubsystem::GetAppPriceInfo(FString SteamId, const TArray<int32>& AppIds, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ISteamUser"), TEXT("GetAppPriceInfo"), 1, EESteamWebVerb::Get, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("steamid"), ResolveSteamId(SteamId));
	// "appids" is a single comma-separated list.
	TArray<FString> AppIdStrings;
	AppIdStrings.Reserve(AppIds.Num());
	for (int32 Id : AppIds)
	{
		AppIdStrings.Add(FString::FromInt(Id));
	}
	Request.AddParam(TEXT("appids"), AppIdStrings);

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebUserSubsystem::GetPublisherAppOwnership(FString SteamId, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ISteamUser"), TEXT("GetPublisherAppOwnership"), 4, EESteamWebVerb::Get, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("steamid"), ResolveSteamId(SteamId));

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebUserSubsystem::GetPublisherAppOwnershipChanges(FString PackageRowVersion, FString CdKeyRowVersion, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ISteamUser"), TEXT("GetPublisherAppOwnershipChanges"), 1, EESteamWebVerb::Get, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("packagerowversion"), PackageRowVersion);
	Request.AddParam(TEXT("cdkeyrowversion"), CdKeyRowVersion);

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebUserSubsystem::GetUserGroupList(FString SteamId, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ISteamUser"), TEXT("GetUserGroupList"), 1, EESteamWebVerb::Get, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("steamid"), ResolveSteamId(SteamId));

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebUserSubsystem::GrantPackage(FString SteamId, int32 PackageId, FString ThirdPartyKey, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ISteamUser"), TEXT("GrantPackage"), 1, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("steamid"), ResolveSteamId(SteamId));
	Request.AddParam(TEXT("packageid"), PackageId);
	if (!ThirdPartyKey.IsEmpty())
	{
		Request.AddParam(TEXT("thirdpartykey"), ThirdPartyKey);
	}

	SendWebRequest(MoveTemp(Request), OnResponse);
}
