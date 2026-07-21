// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "PlayerService/ESteamWebPlayerServiceSubsystem.h"
#include "Core/ESteamWebSettings.h"

void UESteamWebPlayerServiceSubsystem::GetRecentlyPlayedGames(FString SteamId, int32 Count, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("IPlayerService"), TEXT("GetRecentlyPlayedGames"), 1);

	Request.AddParam(TEXT("steamid"), ResolveSteamId(SteamId));
	if (Count > 0)
	{
		Request.AddParam(TEXT("count"), Count);
	}

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebPlayerServiceSubsystem::GetOwnedGames(FString SteamId, bool bIncludeAppInfo, bool bIncludePlayedFreeGames, const TArray<int32>& FilterAppIds, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("IPlayerService"), TEXT("GetOwnedGames"), 1);

	Request.AddParam(TEXT("steamid"), ResolveSteamId(SteamId));
	if (bIncludeAppInfo)
	{
		Request.AddParam(TEXT("include_appinfo"), true);
	}
	if (bIncludePlayedFreeGames)
	{
		Request.AddParam(TEXT("include_played_free_games"), true);
	}
	// Service interfaces accept indexed array params — appids_filter[N] — no input_json blob needed.
	for (int32 Index = 0; Index < FilterAppIds.Num(); ++Index)
	{
		Request.AddParam(FString::Printf(TEXT("appids_filter[%d]"), Index), FilterAppIds[Index]);
	}

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebPlayerServiceSubsystem::GetSteamLevel(FString SteamId, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("IPlayerService"), TEXT("GetSteamLevel"), 1);

	Request.AddParam(TEXT("steamid"), ResolveSteamId(SteamId));

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebPlayerServiceSubsystem::GetBadges(FString SteamId, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("IPlayerService"), TEXT("GetBadges"), 1);

	Request.AddParam(TEXT("steamid"), ResolveSteamId(SteamId));

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebPlayerServiceSubsystem::GetCommunityBadgeProgress(FString SteamId, int32 BadgeId, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("IPlayerService"), TEXT("GetCommunityBadgeProgress"), 1);

	Request.AddParam(TEXT("steamid"), ResolveSteamId(SteamId));
	if (BadgeId > 0)
	{
		Request.AddParam(TEXT("badgeid"), BadgeId);
	}

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebPlayerServiceSubsystem::IsPlayingSharedGame(FString SteamId, int32 AppIdPlaying, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("IPlayerService"), TEXT("IsPlayingSharedGame"), 1);

	Request.AddParam(TEXT("steamid"), ResolveSteamId(SteamId));
	Request.AddParam(TEXT("appid_playing"), ResolveAppId(AppIdPlaying));

	SendWebRequest(MoveTemp(Request), OnResponse);
}
