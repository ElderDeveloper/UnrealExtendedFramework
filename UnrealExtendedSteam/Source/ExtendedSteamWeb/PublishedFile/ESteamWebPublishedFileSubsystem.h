// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/ESteamWebSubsystem.h"
#include "ESteamWebPublishedFileSubsystem.generated.h"

/**
 * IPublishedFileService — Workshop/UGC file queries and publisher-side moderation.
 * QueryFiles works with a regular Web API key on the public host; the moderation methods
 * (SetDeveloperMetadata, UpdateAppUGCBan, UpdateBanStatus, UpdateIncompatibleStatus) require a
 * PUBLISHER key and use the partner host — trusted-server use only.
 */
UCLASS()
class EXTENDEDSTEAMWEB_API UESteamWebPublishedFileSubsystem : public UESteamWebSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * IPublishedFileService/QueryFiles/v1 — flexible Workshop query (Web API key).
	 * QueryType matches EPublishedFileQueryType (0 RankedByVote, 1 RankedByPublicationDate, ...);
	 * either Page (1-based) or Cursor ("*" for the first page) drives paging — values <= 0 / empty
	 * are omitted, as are NumPerPage <= 0, empty tag CSVs and an empty Language. The bReturn*
	 * flags are only sent when true (endpoint defaults are false).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|PublishedFile", meta = (AutoCreateRefTerm = "OnResponse"))
	void QueryFiles(int32 QueryType, int32 Page, FString Cursor, int32 NumPerPage, int32 AppId, FString RequiredTagsCsv, FString ExcludedTagsCsv, bool bReturnVoteData, bool bReturnTags, bool bReturnKvTags, bool bReturnPreviews, bool bReturnChildren, bool bReturnShortDescription, bool bReturnMetadata, FString Language, const FOnSteamWebResponse& OnResponse);

	/**
	 * IPublishedFileService/SetDeveloperMetadata/v1 (POST) — sets the developer metadata string
	 * on a published file (publisher key).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|PublishedFile", meta = (AutoCreateRefTerm = "OnResponse"))
	void SetDeveloperMetadata(int32 AppId, FString PublishedFileId, FString Metadata, const FOnSteamWebResponse& OnResponse);

	/**
	 * IPublishedFileService/UpdateAppUGCBan/v1 (POST) — bans a user from the app's Workshop
	 * (publisher key). ExpirationTime is a unix time; 0 means a permanent ban, so it is always
	 * sent. An empty Reason is omitted.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|PublishedFile", meta = (AutoCreateRefTerm = "OnResponse"))
	void UpdateAppUGCBan(int32 AppId, FString SteamId, int32 ExpirationTime, FString Reason, const FOnSteamWebResponse& OnResponse);

	/**
	 * IPublishedFileService/UpdateBanStatus/v1 (POST) — bans or unbans a published file
	 * (publisher key).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|PublishedFile", meta = (AutoCreateRefTerm = "OnResponse"))
	void UpdateBanStatus(int32 AppId, FString PublishedFileId, bool bBanned, FString Reason, const FOnSteamWebResponse& OnResponse);

	/**
	 * IPublishedFileService/UpdateIncompatibleStatus/v1 (POST) — marks a published file as
	 * incompatible with the app, or clears the flag (publisher key).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|PublishedFile", meta = (AutoCreateRefTerm = "OnResponse"))
	void UpdateIncompatibleStatus(int32 AppId, FString PublishedFileId, bool bIncompatible, const FOnSteamWebResponse& OnResponse);

	/**
	 * IPublishedFileService/UpdateTags/v1 (POST) — adds and/or removes developer tags on a published
	 * file (publisher key). AddTags/RemoveTags are sent as the service-interface indexed arrays
	 * add_tags[0].. / remove_tags[0].. — either may be empty. AppId <= 0 falls back to the configured AppId.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|PublishedFile", meta = (AutoCreateRefTerm = "OnResponse"))
	void UpdateTags(int32 AppId, FString PublishedFileId, const TArray<FString>& AddTags, const TArray<FString>& RemoveTags, const FOnSteamWebResponse& OnResponse);
};
