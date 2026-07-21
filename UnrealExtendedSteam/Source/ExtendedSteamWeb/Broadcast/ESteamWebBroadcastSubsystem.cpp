// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Broadcast/ESteamWebBroadcastSubsystem.h"
#include "Core/ESteamWebSettings.h"

void UESteamWebBroadcastSubsystem::PostGameDataFrame(int32 AppId, FString SteamId, FString BroadcastId, FString FrameDataJson, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("IBroadcastService"), TEXT("PostGameDataFrame"), 1, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	Request.AddParam(TEXT("steamid"), ResolveSteamId(SteamId));
	Request.AddParam(TEXT("broadcast_id"), BroadcastId);
	Request.AddParam(TEXT("frame_data"), FrameDataJson);

	SendWebRequest(MoveTemp(Request), OnResponse);
}
