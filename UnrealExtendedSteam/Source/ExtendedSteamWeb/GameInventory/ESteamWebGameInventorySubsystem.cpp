// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "GameInventory/ESteamWebGameInventorySubsystem.h"
#include "Core/ESteamWebSettings.h"

void UESteamWebGameInventorySubsystem::GetItemDefArchive(int32 AppId, FString Digest, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("IGameInventory"), TEXT("GetItemDefArchive"), 1, EESteamWebVerb::Get, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	Request.AddParam(TEXT("digest"), Digest);

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebGameInventorySubsystem::UpdateItemDefs(int32 AppId, FString ItemDefsJson, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("IGameInventory"), TEXT("UpdateItemDefs"), 1, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	Request.AddParam(TEXT("itemdefs"), ItemDefsJson);

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebGameInventorySubsystem::GetUserHistory(int32 AppId, FString SteamId, int32 ContextId, int32 StartTime, int32 EndTime, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("IGameInventory"), TEXT("GetUserHistory"), 1, EESteamWebVerb::Get, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	Request.AddParam(TEXT("steamid"), ResolveSteamId(SteamId));
	Request.AddParam(TEXT("contextid"), ContextId);
	Request.AddParam(TEXT("starttime"), StartTime);
	Request.AddParam(TEXT("endtime"), EndTime);

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebGameInventorySubsystem::GetHistoryCommandDetails(int32 AppId, FString SteamId, FString Command, int32 ContextId, FString Arguments, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("IGameInventory"), TEXT("GetHistoryCommandDetails"), 1, EESteamWebVerb::Get, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	Request.AddParam(TEXT("steamid"), ResolveSteamId(SteamId));
	Request.AddParam(TEXT("command"), Command);
	Request.AddParam(TEXT("contextid"), ContextId);
	Request.AddParam(TEXT("arguments"), Arguments);

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebGameInventorySubsystem::HistoryExecuteCommands(int32 AppId, FString SteamId, int32 ContextId, int32 ActorId, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("IGameInventory"), TEXT("HistoryExecuteCommands"), 1, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	Request.AddParam(TEXT("steamid"), ResolveSteamId(SteamId));
	Request.AddParam(TEXT("contextid"), ContextId);
	Request.AddParam(TEXT("actorid"), ActorId);

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebGameInventorySubsystem::SupportGetAssetHistory(int32 AppId, FString AssetId, int32 ContextId, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("IGameInventory"), TEXT("SupportGetAssetHistory"), 1, EESteamWebVerb::Get, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	Request.AddParam(TEXT("assetid"), AssetId);
	Request.AddParam(TEXT("contextid"), ContextId);

	SendWebRequest(MoveTemp(Request), OnResponse);
}
