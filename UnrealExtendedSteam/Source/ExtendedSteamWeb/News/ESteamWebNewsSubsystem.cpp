// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "News/ESteamWebNewsSubsystem.h"
#include "Core/ESteamWebSettings.h"

void UESteamWebNewsSubsystem::GetNewsForApp(int32 AppId, int32 Count, int32 MaxLength, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ISteamNews"), TEXT("GetNewsForApp"), 2, EESteamWebVerb::Get, /*bUsePartnerHost*/ false, /*bRequiresApiKey*/ false);

	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	if (Count > 0)
	{
		Request.AddParam(TEXT("count"), Count);
	}
	if (MaxLength > 0)
	{
		Request.AddParam(TEXT("maxlength"), MaxLength);
	}

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebNewsSubsystem::GetNewsForAppAuthed(int32 AppId, int32 Count, int32 MaxLength, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ISteamNews"), TEXT("GetNewsForAppAuthed"), 2, EESteamWebVerb::Get, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	if (Count > 0)
	{
		Request.AddParam(TEXT("count"), Count);
	}
	if (MaxLength > 0)
	{
		Request.AddParam(TEXT("maxlength"), MaxLength);
	}

	SendWebRequest(MoveTemp(Request), OnResponse);
}
