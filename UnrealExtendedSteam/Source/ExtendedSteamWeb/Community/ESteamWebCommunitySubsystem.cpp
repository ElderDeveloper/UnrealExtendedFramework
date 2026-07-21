// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Community/ESteamWebCommunitySubsystem.h"
#include "Core/ESteamWebSettings.h"

void UESteamWebCommunitySubsystem::ReportAbuse(FString SteamIdActor, FString SteamIdTarget, int32 AppId, int32 AbuseType, int32 ContentType, FString Description, FString Gid, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ISteamCommunity"), TEXT("ReportAbuse"), 1, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("steamidActor"), ResolveSteamId(SteamIdActor));
	Request.AddParam(TEXT("steamidTarget"), SteamIdTarget);
	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	Request.AddParam(TEXT("abuseType"), AbuseType);
	Request.AddParam(TEXT("contentType"), ContentType);
	Request.AddParam(TEXT("description"), Description);
	if (!Gid.IsEmpty())
	{
		Request.AddParam(TEXT("gid"), Gid);
	}

	SendWebRequest(MoveTemp(Request), OnResponse);
}
