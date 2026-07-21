// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "GameServerStats/ESteamWebGameServerStatsSubsystem.h"
#include "Core/ESteamWebSettings.h"

void UESteamWebGameServerStatsSubsystem::GetGameServerPlayerStatsForGame(FString GameServerSteamId, int32 AppId, FString RangeStart, FString RangeEnd, int32 MaxResults, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ISteamGameServerStats"), TEXT("GetGameServerPlayerStatsForGame"), 1, EESteamWebVerb::Get, /*bUsePartnerHost*/ true);

	const int32 ResolvedAppId = ResolveAppId(AppId);
	Request.AddParam(TEXT("gameid"), GameServerSteamId.IsEmpty() ? FString::FromInt(ResolvedAppId) : GameServerSteamId);
	Request.AddParam(TEXT("appid"), ResolvedAppId);
	Request.AddParam(TEXT("rangestart"), RangeStart);
	Request.AddParam(TEXT("rangeend"), RangeEnd);
	if (MaxResults > 0)
	{
		Request.AddParam(TEXT("maxresults"), MaxResults);
	}

	SendWebRequest(MoveTemp(Request), OnResponse);
}
