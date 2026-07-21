// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "PublishedItemVoting/ESteamWebPublishedItemVotingSubsystem.h"
#include "Core/ESteamWebSettings.h"

void UESteamWebPublishedItemVotingSubsystem::ItemVoteSummary(FString SteamId, int32 AppId, const TArray<FString>& PublishedFileIds, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ISteamPublishedItemVoting"), TEXT("ItemVoteSummary"), 1, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("steamid"), ResolveSteamId(SteamId));
	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	Request.AddParam(TEXT("count"), PublishedFileIds.Num());
	for (int32 Index = 0; Index < PublishedFileIds.Num(); ++Index)
	{
		Request.AddParam(FString::Printf(TEXT("publishedfileid[%d]"), Index), PublishedFileIds[Index]);
	}

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebPublishedItemVotingSubsystem::UserVoteSummary(FString SteamId, const TArray<FString>& PublishedFileIds, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ISteamPublishedItemVoting"), TEXT("UserVoteSummary"), 1, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("steamid"), ResolveSteamId(SteamId));
	Request.AddParam(TEXT("count"), PublishedFileIds.Num());
	for (int32 Index = 0; Index < PublishedFileIds.Num(); ++Index)
	{
		Request.AddParam(FString::Printf(TEXT("publishedfileid[%d]"), Index), PublishedFileIds[Index]);
	}

	SendWebRequest(MoveTemp(Request), OnResponse);
}
