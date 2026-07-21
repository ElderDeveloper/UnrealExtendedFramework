// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "GameNotifications/ESteamWebGameNotificationsSubsystem.h"
#include "Core/ESteamWebSettings.h"

void UESteamWebGameNotificationsSubsystem::CreateSession(int32 AppId, FString ContextJson, FString TitleJson, FString UsersJson, FString SteamId, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("IGameNotificationsService"), TEXT("CreateSession"), 1, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	Request.AddParam(TEXT("context"), ContextJson);
	Request.AddParam(TEXT("title"), TitleJson);
	Request.AddParam(TEXT("users"), UsersJson);
	if (!SteamId.IsEmpty())
	{
		Request.AddParam(TEXT("steamid"), SteamId);
	}

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebGameNotificationsSubsystem::UpdateSession(int64 SessionId, int32 AppId, FString TitleJson, FString UsersJson, FString SteamId, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("IGameNotificationsService"), TEXT("UpdateSession"), 1, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("sessionid"), SessionId);
	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	if (!TitleJson.IsEmpty())
	{
		Request.AddParam(TEXT("title"), TitleJson);
	}
	if (!UsersJson.IsEmpty())
	{
		Request.AddParam(TEXT("users"), UsersJson);
	}
	if (!SteamId.IsEmpty())
	{
		Request.AddParam(TEXT("steamid"), SteamId);
	}

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebGameNotificationsSubsystem::EnumerateSessionsForApp(int32 AppId, FString SteamId, bool bIncludeAllUserMessages, bool bIncludeAuthMessages, FString Language, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("IGameNotificationsService"), TEXT("EnumerateSessionsForApp"), 1, EESteamWebVerb::Get, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	Request.AddParam(TEXT("steamid"), ResolveSteamId(SteamId));
	Request.AddParam(TEXT("include_all_user_messages"), bIncludeAllUserMessages);
	Request.AddParam(TEXT("include_auth_user_message"), bIncludeAuthMessages);
	if (!Language.IsEmpty())
	{
		Request.AddParam(TEXT("language"), Language);
	}

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebGameNotificationsSubsystem::GetSessionDetailsForApp(int32 AppId, FString SessionIdsJson, FString Language, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("IGameNotificationsService"), TEXT("GetSessionDetailsForApp"), 1, EESteamWebVerb::Get, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	Request.AddParam(TEXT("sessions"), SessionIdsJson);
	if (!Language.IsEmpty())
	{
		Request.AddParam(TEXT("language"), Language);
	}

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebGameNotificationsSubsystem::RequestNotifications(FString SteamId, int32 AppId, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("IGameNotificationsService"), TEXT("RequestNotifications"), 1, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("steamid"), ResolveSteamId(SteamId));
	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebGameNotificationsSubsystem::DeleteSession(int64 SessionId, int32 AppId, FString SteamId, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("IGameNotificationsService"), TEXT("DeleteSession"), 1, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("sessionid"), SessionId);
	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	if (!SteamId.IsEmpty())
	{
		Request.AddParam(TEXT("steamid"), SteamId);
	}

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebGameNotificationsSubsystem::DeleteSessionBatch(FString SessionIdsCsv, int32 AppId, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("IGameNotificationsService"), TEXT("DeleteSessionBatch"), 1, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("sessionid"), SessionIdsCsv);
	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));

	SendWebRequest(MoveTemp(Request), OnResponse);
}
