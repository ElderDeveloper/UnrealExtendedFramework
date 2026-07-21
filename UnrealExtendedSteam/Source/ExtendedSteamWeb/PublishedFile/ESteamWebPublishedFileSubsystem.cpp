// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "PublishedFile/ESteamWebPublishedFileSubsystem.h"
#include "Core/ESteamWebSettings.h"

void UESteamWebPublishedFileSubsystem::QueryFiles(int32 QueryType, int32 Page, FString Cursor, int32 NumPerPage, int32 AppId, FString RequiredTagsCsv, FString ExcludedTagsCsv, bool bReturnVoteData, bool bReturnTags, bool bReturnKvTags, bool bReturnPreviews, bool bReturnChildren, bool bReturnShortDescription, bool bReturnMetadata, FString Language, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("IPublishedFileService"), TEXT("QueryFiles"), 1);

	Request.AddParam(TEXT("query_type"), QueryType);
	if (Page > 0)
	{
		Request.AddParam(TEXT("page"), Page);
	}
	if (!Cursor.IsEmpty())
	{
		Request.AddParam(TEXT("cursor"), Cursor);
	}
	if (NumPerPage > 0)
	{
		Request.AddParam(TEXT("numperpage"), NumPerPage);
	}
	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	if (!RequiredTagsCsv.IsEmpty())
	{
		Request.AddParam(TEXT("requiredtags"), RequiredTagsCsv);
	}
	if (!ExcludedTagsCsv.IsEmpty())
	{
		Request.AddParam(TEXT("excludedtags"), ExcludedTagsCsv);
	}
	if (bReturnVoteData)
	{
		Request.AddParam(TEXT("return_vote_data"), bReturnVoteData);
	}
	if (bReturnTags)
	{
		Request.AddParam(TEXT("return_tags"), bReturnTags);
	}
	if (bReturnKvTags)
	{
		Request.AddParam(TEXT("return_kv_tags"), bReturnKvTags);
	}
	if (bReturnPreviews)
	{
		Request.AddParam(TEXT("return_previews"), bReturnPreviews);
	}
	if (bReturnChildren)
	{
		Request.AddParam(TEXT("return_children"), bReturnChildren);
	}
	if (bReturnShortDescription)
	{
		Request.AddParam(TEXT("return_short_description"), bReturnShortDescription);
	}
	if (bReturnMetadata)
	{
		Request.AddParam(TEXT("return_metadata"), bReturnMetadata);
	}
	if (!Language.IsEmpty())
	{
		Request.AddParam(TEXT("language"), Language);
	}

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebPublishedFileSubsystem::SetDeveloperMetadata(int32 AppId, FString PublishedFileId, FString Metadata, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("IPublishedFileService"), TEXT("SetDeveloperMetadata"), 1, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("publishedfileid"), PublishedFileId);
	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	Request.AddParam(TEXT("metadata"), Metadata);

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebPublishedFileSubsystem::UpdateAppUGCBan(int32 AppId, FString SteamId, int32 ExpirationTime, FString Reason, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("IPublishedFileService"), TEXT("UpdateAppUGCBan"), 1, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("steamid"), SteamId);
	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	Request.AddParam(TEXT("expiration_time"), ExpirationTime > 0 ? ExpirationTime : 0);
	if (!Reason.IsEmpty())
	{
		Request.AddParam(TEXT("reason"), Reason);
	}

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebPublishedFileSubsystem::UpdateBanStatus(int32 AppId, FString PublishedFileId, bool bBanned, FString Reason, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("IPublishedFileService"), TEXT("UpdateBanStatus"), 1, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("publishedfileid"), PublishedFileId);
	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	Request.AddParam(TEXT("banned"), bBanned);
	Request.AddParam(TEXT("reason"), Reason);

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebPublishedFileSubsystem::UpdateIncompatibleStatus(int32 AppId, FString PublishedFileId, bool bIncompatible, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("IPublishedFileService"), TEXT("UpdateIncompatibleStatus"), 1, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("publishedfileid"), PublishedFileId);
	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	Request.AddParam(TEXT("incompatible"), bIncompatible);

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebPublishedFileSubsystem::UpdateTags(int32 AppId, FString PublishedFileId, const TArray<FString>& AddTags, const TArray<FString>& RemoveTags, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("IPublishedFileService"), TEXT("UpdateTags"), 1, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("publishedfileid"), PublishedFileId);
	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	// Service-interface indexed arrays -> add_tags[N] / remove_tags[N].
	for (int32 Index = 0; Index < AddTags.Num(); ++Index)
	{
		Request.AddParam(FString::Printf(TEXT("add_tags[%d]"), Index), AddTags[Index]);
	}
	for (int32 Index = 0; Index < RemoveTags.Num(); ++Index)
	{
		Request.AddParam(FString::Printf(TEXT("remove_tags[%d]"), Index), RemoveTags[Index]);
	}

	SendWebRequest(MoveTemp(Request), OnResponse);
}
