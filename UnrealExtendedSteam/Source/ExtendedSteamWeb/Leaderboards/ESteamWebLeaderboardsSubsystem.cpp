// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Leaderboards/ESteamWebLeaderboardsSubsystem.h"
#include "Core/ESteamWebSettings.h"

void UESteamWebLeaderboardsSubsystem::GetLeaderboardsForGame(int32 AppId, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ISteamLeaderboards"), TEXT("GetLeaderboardsForGame"), 2, EESteamWebVerb::Get, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebLeaderboardsSubsystem::GetLeaderboardEntries(int32 AppId, int32 LeaderboardId, int32 RangeStart, int32 RangeEnd, int32 DataRequest, FString SteamId, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ISteamLeaderboards"), TEXT("GetLeaderboardEntries"), 1, EESteamWebVerb::Get, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	Request.AddParam(TEXT("leaderboardid"), LeaderboardId);
	Request.AddParam(TEXT("rangestart"), RangeStart);
	Request.AddParam(TEXT("rangeend"), RangeEnd);
	Request.AddParam(TEXT("datarequest"), DataRequest);

	// Only the user-relative modes (GlobalAroundUser / Friends) reference a user.
	if (DataRequest != 0)
	{
		Request.AddParam(TEXT("steamid"), ResolveSteamId(SteamId));
	}

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebLeaderboardsSubsystem::DeleteLeaderboard(int32 AppId, FString Name, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ISteamLeaderboards"), TEXT("DeleteLeaderboard"), 1, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	Request.AddParam(TEXT("name"), Name);

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebLeaderboardsSubsystem::FindOrCreateLeaderboard(int32 AppId, FString Name, FString SortMethod, FString DisplayType, bool bCreateIfNotFound, bool bOnlyTrustedWrites, bool bOnlyFriendsReads, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ISteamLeaderboards"), TEXT("FindOrCreateLeaderboard"), 2, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	Request.AddParam(TEXT("name"), Name);
	if (!SortMethod.IsEmpty())
	{
		Request.AddParam(TEXT("sortmethod"), SortMethod);
	}
	if (!DisplayType.IsEmpty())
	{
		Request.AddParam(TEXT("displaytype"), DisplayType);
	}
	Request.AddParam(TEXT("createifnotfound"), bCreateIfNotFound);
	Request.AddParam(TEXT("onlytrustedwrites"), bOnlyTrustedWrites);
	Request.AddParam(TEXT("onlyfriendsreads"), bOnlyFriendsReads);

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebLeaderboardsSubsystem::ResetLeaderboard(int32 AppId, int32 LeaderboardId, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ISteamLeaderboards"), TEXT("ResetLeaderboard"), 1, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	Request.AddParam(TEXT("leaderboardid"), LeaderboardId);

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebLeaderboardsSubsystem::SetLeaderboardScore(int32 AppId, int32 LeaderboardId, FString SteamId, int32 Score, FString ScoreMethod, FString DetailsJson, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ISteamLeaderboards"), TEXT("SetLeaderboardScore"), 1, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	Request.AddParam(TEXT("leaderboardid"), LeaderboardId);
	Request.AddParam(TEXT("steamid"), ResolveSteamId(SteamId));
	Request.AddParam(TEXT("score"), Score);
	if (!ScoreMethod.IsEmpty())
	{
		Request.AddParam(TEXT("scoremethod"), ScoreMethod);
	}
	if (!DetailsJson.IsEmpty())
	{
		Request.AddParam(TEXT("details"), DetailsJson);
	}

	SendWebRequest(MoveTemp(Request), OnResponse);
}
