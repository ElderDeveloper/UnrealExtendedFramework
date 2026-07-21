// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/ESteamWebSubsystem.h"
#include "ESteamWebRemoteStorageSubsystem.generated.h"

/**
 * ISteamRemoteStorage — UGC / Workshop published-file access. A mixed-key interface:
 * GetCollectionDetails and GetPublishedFileDetails need NO key, GetUGCFileDetails needs a
 * regular Web API key, and the remaining methods need a PUBLISHER key on the partner host
 * (trusted-server use only).
 */
UCLASS()
class EXTENDEDSTEAMWEB_API UESteamWebRemoteStorageSubsystem : public UESteamWebSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * ISteamRemoteStorage/EnumerateUserSubscribedFiles/v1 (POST) — files the user subscribed to
	 * (publisher key). ListType matches EUCMListType (1 subscriptions, 2 voted on, ...) and is
	 * omitted when <= 0. An empty SteamId falls back to the configured DevSteamId.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|RemoteStorage", meta = (AutoCreateRefTerm = "OnResponse"))
	void EnumerateUserSubscribedFiles(FString SteamId, int32 AppId, int32 ListType, const FOnSteamWebResponse& OnResponse);

	/**
	 * ISteamRemoteStorage/EnumerateUserPublishedFiles/v1 (POST) — files the user published for
	 * the app (publisher key). An empty SteamId falls back to the configured DevSteamId.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|RemoteStorage", meta = (AutoCreateRefTerm = "OnResponse"))
	void EnumerateUserPublishedFiles(FString SteamId, int32 AppId, const FOnSteamWebResponse& OnResponse);

	/**
	 * ISteamRemoteStorage/GetCollectionDetails/v1 (POST) — details for Workshop collections
	 * (NO key required). CollectionIds expand to collectioncount + publishedfileids[N].
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|RemoteStorage", meta = (AutoCreateRefTerm = "CollectionIds,OnResponse"))
	void GetCollectionDetails(const TArray<FString>& CollectionIds, const FOnSteamWebResponse& OnResponse);

	/**
	 * ISteamRemoteStorage/GetPublishedFileDetails/v1 (POST) — details for published files
	 * (NO key required). PublishedFileIds expand to itemcount + publishedfileids[N].
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|RemoteStorage", meta = (AutoCreateRefTerm = "PublishedFileIds,OnResponse"))
	void GetPublishedFileDetails(const TArray<FString>& PublishedFileIds, const FOnSteamWebResponse& OnResponse);

	/**
	 * ISteamRemoteStorage/GetUGCFileDetails/v1 — download metadata for a raw UGC handle.
	 * Per the docs this DOES require a (regular) Web API key; SteamId is optional and omitted
	 * when empty.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|RemoteStorage", meta = (AutoCreateRefTerm = "OnResponse"))
	void GetUGCFileDetails(FString SteamId, FString UgcId, int32 AppId, const FOnSteamWebResponse& OnResponse);

	/**
	 * ISteamRemoteStorage/SetUGCUsedByGC/v1 (POST) — marks a UGC handle as (not) referenced by
	 * the game coordinator, pinning it against deletion (publisher key).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|RemoteStorage", meta = (AutoCreateRefTerm = "OnResponse"))
	void SetUGCUsedByGC(FString SteamId, FString UgcId, int32 AppId, bool bUsed, const FOnSteamWebResponse& OnResponse);

	/**
	 * ISteamRemoteStorage/SubscribePublishedFile/v1 (POST) — subscribes the user to a published
	 * file (publisher key). An empty SteamId falls back to the configured DevSteamId.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|RemoteStorage", meta = (AutoCreateRefTerm = "OnResponse"))
	void SubscribePublishedFile(FString SteamId, FString PublishedFileId, int32 AppId, const FOnSteamWebResponse& OnResponse);

	/**
	 * ISteamRemoteStorage/UnsubscribePublishedFile/v1 (POST) — unsubscribes the user from a
	 * published file (publisher key). An empty SteamId falls back to the configured DevSteamId.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|RemoteStorage", meta = (AutoCreateRefTerm = "OnResponse"))
	void UnsubscribePublishedFile(FString SteamId, FString PublishedFileId, int32 AppId, const FOnSteamWebResponse& OnResponse);
};
