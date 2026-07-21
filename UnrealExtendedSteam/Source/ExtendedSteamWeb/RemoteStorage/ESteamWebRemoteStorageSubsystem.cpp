// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "RemoteStorage/ESteamWebRemoteStorageSubsystem.h"
#include "Core/ESteamWebSettings.h"

void UESteamWebRemoteStorageSubsystem::EnumerateUserSubscribedFiles(FString SteamId, int32 AppId, int32 ListType, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ISteamRemoteStorage"), TEXT("EnumerateUserSubscribedFiles"), 1, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("steamid"), ResolveSteamId(SteamId));
	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	if (ListType > 0)
	{
		Request.AddParam(TEXT("listtype"), ListType);
	}

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebRemoteStorageSubsystem::EnumerateUserPublishedFiles(FString SteamId, int32 AppId, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ISteamRemoteStorage"), TEXT("EnumerateUserPublishedFiles"), 1, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("steamid"), ResolveSteamId(SteamId));
	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebRemoteStorageSubsystem::GetCollectionDetails(const TArray<FString>& CollectionIds, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ISteamRemoteStorage"), TEXT("GetCollectionDetails"), 1, EESteamWebVerb::Post, false, /*bRequiresApiKey*/ false);

	Request.AddParam(TEXT("collectioncount"), CollectionIds.Num());
	for (int32 Index = 0; Index < CollectionIds.Num(); ++Index)
	{
		Request.AddParam(FString::Printf(TEXT("publishedfileids[%d]"), Index), CollectionIds[Index]);
	}

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebRemoteStorageSubsystem::GetPublishedFileDetails(const TArray<FString>& PublishedFileIds, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ISteamRemoteStorage"), TEXT("GetPublishedFileDetails"), 1, EESteamWebVerb::Post, false, /*bRequiresApiKey*/ false);

	Request.AddParam(TEXT("itemcount"), PublishedFileIds.Num());
	for (int32 Index = 0; Index < PublishedFileIds.Num(); ++Index)
	{
		Request.AddParam(FString::Printf(TEXT("publishedfileids[%d]"), Index), PublishedFileIds[Index]);
	}

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebRemoteStorageSubsystem::GetUGCFileDetails(FString SteamId, FString UgcId, int32 AppId, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ISteamRemoteStorage"), TEXT("GetUGCFileDetails"), 1);

	if (!SteamId.IsEmpty())
	{
		Request.AddParam(TEXT("steamid"), SteamId);
	}
	Request.AddParam(TEXT("ugcid"), UgcId);
	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebRemoteStorageSubsystem::SetUGCUsedByGC(FString SteamId, FString UgcId, int32 AppId, bool bUsed, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ISteamRemoteStorage"), TEXT("SetUGCUsedByGC"), 1, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("steamid"), ResolveSteamId(SteamId));
	Request.AddParam(TEXT("ugcid"), UgcId);
	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	Request.AddParam(TEXT("used"), bUsed);

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebRemoteStorageSubsystem::SubscribePublishedFile(FString SteamId, FString PublishedFileId, int32 AppId, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ISteamRemoteStorage"), TEXT("SubscribePublishedFile"), 1, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("steamid"), ResolveSteamId(SteamId));
	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	Request.AddParam(TEXT("publishedfileid"), PublishedFileId);

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebRemoteStorageSubsystem::UnsubscribePublishedFile(FString SteamId, FString PublishedFileId, int32 AppId, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ISteamRemoteStorage"), TEXT("UnsubscribePublishedFile"), 1, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("steamid"), ResolveSteamId(SteamId));
	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	Request.AddParam(TEXT("publishedfileid"), PublishedFileId);

	SendWebRequest(MoveTemp(Request), OnResponse);
}
