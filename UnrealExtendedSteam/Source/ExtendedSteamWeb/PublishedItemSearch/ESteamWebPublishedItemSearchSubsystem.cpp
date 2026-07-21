// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "PublishedItemSearch/ESteamWebPublishedItemSearchSubsystem.h"
#include "Core/ESteamWebSettings.h"

FESteamWebRequest UESteamWebPublishedItemSearchSubsystem::MakeSearchRequest(const FString& Method, const FString& SteamId, int32 AppId) const
{
	FESteamWebRequest Request = MakeRequest(TEXT("ISteamPublishedItemSearch"), Method, 1, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("steamid"), ResolveSteamId(SteamId));
	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	return Request;
}

void UESteamWebPublishedItemSearchSubsystem::AppendTagParams(FESteamWebRequest& Request, const TArray<FString>& Tags, const TArray<FString>& UserTags, bool bHasAppAdminAccess, int32 FileType)
{
	Request.AddParam(TEXT("tagcount"), Tags.Num());
	for (int32 Index = 0; Index < Tags.Num(); ++Index)
	{
		Request.AddParam(FString::Printf(TEXT("tag[%d]"), Index), Tags[Index]);
	}
	Request.AddParam(TEXT("usertagcount"), UserTags.Num());
	for (int32 Index = 0; Index < UserTags.Num(); ++Index)
	{
		Request.AddParam(FString::Printf(TEXT("usertag[%d]"), Index), UserTags[Index]);
	}
	if (bHasAppAdminAccess)
	{
		Request.AddParam(TEXT("hasappadminaccess"), bHasAppAdminAccess);
	}
	if (FileType > 0)
	{
		Request.AddParam(TEXT("fileType"), FileType);
	}
}

void UESteamWebPublishedItemSearchSubsystem::RankedByPublicationOrder(FString SteamId, int32 AppId, int32 StartIdx, int32 Count, const TArray<FString>& Tags, const TArray<FString>& UserTags, bool bHasAppAdminAccess, int32 FileType, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeSearchRequest(TEXT("RankedByPublicationOrder"), SteamId, AppId);

	Request.AddParam(TEXT("startidx"), StartIdx > 0 ? StartIdx : 0);
	Request.AddParam(TEXT("count"), Count);
	AppendTagParams(Request, Tags, UserTags, bHasAppAdminAccess, FileType);

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebPublishedItemSearchSubsystem::RankedByTrend(FString SteamId, int32 AppId, int32 StartIdx, int32 Count, int32 Days, const TArray<FString>& Tags, const TArray<FString>& UserTags, bool bHasAppAdminAccess, int32 FileType, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeSearchRequest(TEXT("RankedByTrend"), SteamId, AppId);

	Request.AddParam(TEXT("startidx"), StartIdx > 0 ? StartIdx : 0);
	Request.AddParam(TEXT("count"), Count);
	if (Days > 0)
	{
		Request.AddParam(TEXT("days"), Days);
	}
	AppendTagParams(Request, Tags, UserTags, bHasAppAdminAccess, FileType);

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebPublishedItemSearchSubsystem::RankedByVote(FString SteamId, int32 AppId, int32 StartIdx, int32 Count, const TArray<FString>& Tags, const TArray<FString>& UserTags, bool bHasAppAdminAccess, int32 FileType, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeSearchRequest(TEXT("RankedByVote"), SteamId, AppId);

	Request.AddParam(TEXT("startidx"), StartIdx > 0 ? StartIdx : 0);
	Request.AddParam(TEXT("count"), Count);
	AppendTagParams(Request, Tags, UserTags, bHasAppAdminAccess, FileType);

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebPublishedItemSearchSubsystem::ResultSetSummary(FString SteamId, int32 AppId, const TArray<FString>& Tags, const TArray<FString>& UserTags, bool bHasAppAdminAccess, int32 FileType, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeSearchRequest(TEXT("ResultSetSummary"), SteamId, AppId);

	AppendTagParams(Request, Tags, UserTags, bHasAppAdminAccess, FileType);

	SendWebRequest(MoveTemp(Request), OnResponse);
}
