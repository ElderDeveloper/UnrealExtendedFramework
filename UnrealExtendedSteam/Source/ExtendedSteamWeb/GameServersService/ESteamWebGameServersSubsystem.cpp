// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "GameServersService/ESteamWebGameServersSubsystem.h"
#include "Core/ESteamWebSettings.h"

void UESteamWebGameServersSubsystem::GetAccountList(const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("IGameServersService"), TEXT("GetAccountList"), 1, EESteamWebVerb::Get, /*bUsePartnerHost*/ true);

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebGameServersSubsystem::CreateAccount(int32 AppId, FString Memo, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("IGameServersService"), TEXT("CreateAccount"), 1, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	Request.AddParam(TEXT("memo"), Memo);

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebGameServersSubsystem::SetMemo(FString SteamId, FString Memo, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("IGameServersService"), TEXT("SetMemo"), 1, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("steamid"), SteamId);
	Request.AddParam(TEXT("memo"), Memo);

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebGameServersSubsystem::ResetLoginToken(FString SteamId, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("IGameServersService"), TEXT("ResetLoginToken"), 1, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("steamid"), SteamId);

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebGameServersSubsystem::DeleteAccount(FString SteamId, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("IGameServersService"), TEXT("DeleteAccount"), 1, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("steamid"), SteamId);

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebGameServersSubsystem::GetAccountPublicInfo(FString SteamId, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("IGameServersService"), TEXT("GetAccountPublicInfo"), 1, EESteamWebVerb::Get, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("steamid"), SteamId);

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebGameServersSubsystem::QueryLoginToken(FString LoginToken, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("IGameServersService"), TEXT("QueryLoginToken"), 1, EESteamWebVerb::Get, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("login_token"), LoginToken);

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebGameServersSubsystem::GetServerSteamIDsByIP(const TArray<FString>& ServerIps, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("IGameServersService"), TEXT("GetServerSteamIDsByIP"), 1, EESteamWebVerb::Get, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("server_ips"), ServerIps);

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebGameServersSubsystem::GetServerIPsBySteamID(const TArray<FString>& ServerSteamIds, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("IGameServersService"), TEXT("GetServerIPsBySteamID"), 1, EESteamWebVerb::Get, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("server_steamids"), ServerSteamIds);

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebGameServersSubsystem::SetBanStatus(FString SteamId, bool bBanned, int32 BanSeconds, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("IGameServersService"), TEXT("SetBanStatus"), 1, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("steamid"), SteamId);
	Request.AddParam(TEXT("banned"), bBanned);
	Request.AddParam(TEXT("ban_seconds"), BanSeconds);

	SendWebRequest(MoveTemp(Request), OnResponse);
}
