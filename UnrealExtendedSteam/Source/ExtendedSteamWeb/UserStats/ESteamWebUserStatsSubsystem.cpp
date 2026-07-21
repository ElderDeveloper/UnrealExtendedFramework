// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "UserStats/ESteamWebUserStatsSubsystem.h"
#include "Core/ESteamWebSettings.h"

void UESteamWebUserStatsSubsystem::GetGlobalAchievementPercentagesForApp(int32 GameId, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ISteamUserStats"), TEXT("GetGlobalAchievementPercentagesForApp"), 2, EESteamWebVerb::Get, false, /*bRequiresApiKey*/ false);

	Request.AddParam(TEXT("gameid"), ResolveAppId(GameId));

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebUserStatsSubsystem::GetGlobalStatsForGame(int32 AppId, const TArray<FString>& StatNames, int64 StartDate, int64 EndDate, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ISteamUserStats"), TEXT("GetGlobalStatsForGame"), 1, EESteamWebVerb::Get, false, /*bRequiresApiKey*/ false);

	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	Request.AddParam(TEXT("count"), StatNames.Num());
	for (int32 Index = 0; Index < StatNames.Num(); ++Index)
	{
		Request.AddParam(FString::Printf(TEXT("name[%d]"), Index), StatNames[Index]);
	}
	if (StartDate > 0)
	{
		Request.AddParam(TEXT("startdate"), StartDate);
	}
	if (EndDate > 0)
	{
		Request.AddParam(TEXT("enddate"), EndDate);
	}

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebUserStatsSubsystem::GetNumberOfCurrentPlayers(int32 AppId, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ISteamUserStats"), TEXT("GetNumberOfCurrentPlayers"), 1, EESteamWebVerb::Get, false, /*bRequiresApiKey*/ false);

	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebUserStatsSubsystem::GetPlayerAchievements(FString SteamId, int32 AppId, FString Language, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ISteamUserStats"), TEXT("GetPlayerAchievements"), 1);

	Request.AddParam(TEXT("steamid"), ResolveSteamId(SteamId));
	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	if (!Language.IsEmpty())
	{
		Request.AddParam(TEXT("l"), Language);
	}

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebUserStatsSubsystem::GetSchemaForGame(int32 AppId, FString Language, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ISteamUserStats"), TEXT("GetSchemaForGame"), 2);

	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	if (!Language.IsEmpty())
	{
		Request.AddParam(TEXT("l"), Language);
	}

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebUserStatsSubsystem::GetUserStatsForGame(FString SteamId, int32 AppId, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ISteamUserStats"), TEXT("GetUserStatsForGame"), 2);

	Request.AddParam(TEXT("steamid"), ResolveSteamId(SteamId));
	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebUserStatsSubsystem::SetUserStatsForGame(FString SteamId, int32 AppId, const TArray<FString>& StatNames, const TArray<int32>& StatValues, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ISteamUserStats"), TEXT("SetUserStatsForGame"), 1, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("steamid"), ResolveSteamId(SteamId));
	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	Request.AddParam(TEXT("count"), StatNames.Num());
	for (int32 Index = 0; Index < StatNames.Num(); ++Index)
	{
		Request.AddParam(FString::Printf(TEXT("name[%d]"), Index), StatNames[Index]);
	}
	for (int32 Index = 0; Index < StatValues.Num(); ++Index)
	{
		Request.AddParam(FString::Printf(TEXT("value[%d]"), Index), StatValues[Index]);
	}

	SendWebRequest(MoveTemp(Request), OnResponse);
}
