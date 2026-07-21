// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/ESteamSubsystem.h"
#include "Shared/ESteamTypes.h"
#include "ESteamUGCSubsystem.generated.h"

/** Workshop item type created via CreateItem (mirrors Steamworks EWorkshopFileType subset). */
UENUM(BlueprintType)
enum class EESteamWorkshopFileType : uint8
{
	/** Normal Workshop item that users can subscribe to. */
	Community,
	/** Workshop item meant to be voted on for the purpose of selling in-game. */
	Microtransaction
};

/** Visibility of a Workshop item (mirrors ERemoteStoragePublishedFileVisibility). */
UENUM(BlueprintType)
enum class EESteamUGCVisibility : uint8
{
	Public,
	FriendsOnly,
	Private,
	Unlisted
};

/** Sorting/filtering for QueryAllItems (mirrors Steamworks EUGCQuery subset). */
UENUM(BlueprintType)
enum class EESteamUGCQueryType : uint8
{
	RankedByVote,
	RankedByPublicationDate,
	AcceptedForGameRankedByAcceptanceDate,
	RankedByTrend,
	FavoritedByFriendsRankedByPublicationDate,
	CreatedByFriendsRankedByPublicationDate,
	RankedByTotalUniqueSubscriptions,
	RankedByTextSearch
};

/** Which kind of UGC a query matches (mirrors Steamworks EUGCMatchingUGCType subset). */
UENUM(BlueprintType)
enum class EESteamUGCMatchingType : uint8
{
	/** Both microtransaction items and ready-to-use items. */
	Items,
	Collections,
	Artwork,
	Videos,
	Screenshots,
	/** Ready-to-use items and integrated guides. */
	UsableInGame,
	ControllerBindings,
	/** Items managed completely by the game, not the user. */
	GameManagedItems
};

/** Which of a user's published/interacted UGC lists to query (mirrors Steamworks EUserUGCList). */
UENUM(BlueprintType)
enum class EESteamUserUGCList : uint8
{
	Published,
	VotedOn,
	VotedUp,
	VotedDown,
	WillVoteLater,
	Favorited,
	Subscribed,
	UsedOrPlayed,
	Followed
};

/** Sort order for a user UGC list query (mirrors Steamworks EUserUGCListSortOrder). */
UENUM(BlueprintType)
enum class EESteamUserUGCListSortOrder : uint8
{
	CreationOrderDesc,
	CreationOrderAsc,
	TitleAsc,
	LastUpdatedDesc,
	SubscriptionDateDesc,
	VoteScoreDesc,
	ForModeration
};

/** A single aggregate statistic of a Workshop item (mirrors Steamworks EItemStatistic). */
UENUM(BlueprintType)
enum class EESteamItemStatistic : uint8
{
	NumSubscriptions,
	NumFavorites,
	NumFollowers,
	NumUniqueSubscriptions,
	NumUniqueFavorites,
	NumUniqueFollowers,
	NumUniqueWebsiteViews,
	ReportScore,
	NumSecondsPlayed,
	NumPlaytimeSessions,
	NumComments,
	NumSecondsPlayedDuringTimePeriod,
	NumPlaytimeSessionsDuringTimePeriod
};

/** Kind of an item preview (mirrors Steamworks EItemPreviewType). */
UENUM(BlueprintType)
enum class EESteamItemPreviewType : uint8
{
	/** Standard image file (jpg, png, gif...). */
	Image,
	/** A YouTube video id is stored. */
	YouTubeVideo,
	/** A Sketchfab model id is stored. */
	Sketchfab,
	/** Standard image file laid out as a horizontal-cross cube map. */
	EnvironmentMap_HorizontalCross,
	/** Standard image file laid out as a lat-long environment map. */
	EnvironmentMap_LatLong,
	/** A clip id is stored. */
	Clip,
	/** Values at/above this are game-defined. */
	ReservedMax
};

/** Progress state of a running item update (mirrors Steamworks EItemUpdateStatus). */
UENUM(BlueprintType)
enum class EESteamItemUpdateStatus : uint8
{
	/** Handle invalid or the update already finished — listen for the submit result. */
	Invalid,
	PreparingConfig,
	PreparingContent,
	UploadingContent,
	UploadingPreviewFile,
	CommittingChanges
};

/** A single key/value tag on a Workshop item (a key may map to multiple values). */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDSTEAM_API FESteamKeyValueTag
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Steam|UGC")
	FString Key;

	UPROPERTY(BlueprintReadWrite, Category = "Steam|UGC")
	FString Value;

	FESteamKeyValueTag() = default;

	FESteamKeyValueTag(const FString& InKey, const FString& InValue)
		: Key(InKey)
		, Value(InValue)
	{
	}
};

/** One additional preview (image/video/model) attached to a Workshop item. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDSTEAM_API FESteamUGCAdditionalPreview
{
	GENERATED_BODY()

	/** For images this is a URL; for videos/models this is the id (YouTube/Sketchfab/clip). */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|UGC")
	FString UrlOrVideoId;

	/** Original local file name of the preview, when known. */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|UGC")
	FString OriginalFileName;

	UPROPERTY(BlueprintReadOnly, Category = "Steam|UGC")
	EESteamItemPreviewType PreviewType = EESteamItemPreviewType::Image;
};

/** Local client state of a Workshop item, decoded from the k_EItemState* bitmask. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDSTEAM_API FESteamUGCItemState
{
	GENERATED_BODY()

	/** Current user is subscribed to this item (not just cached). */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|UGC")
	bool bSubscribed = false;

	/** Item is installed and usable (but maybe out of date). */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|UGC")
	bool bInstalled = false;

	/** Item needs an update: not installed yet, or the creator updated its content. */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|UGC")
	bool bNeedsUpdate = false;

	/** Item update is currently downloading. */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|UGC")
	bool bDownloading = false;

	/** DownloadItem was called; content is unavailable until OnItemDownloaded fires. */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|UGC")
	bool bDownloadPending = false;

	/** Item is disabled locally, so it is not considered subscribed. */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|UGC")
	bool bDisabledLocally = false;
};

/** Details of one published Workshop item, converted from SteamUGCDetails_t. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDSTEAM_API FESteamUGCDetails
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Steam|UGC")
	int64 PublishedFileId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Steam|UGC")
	FString Title;

	UPROPERTY(BlueprintReadOnly, Category = "Steam|UGC")
	FString Description;

	/** Steam id of the user who created this content. */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|UGC")
	FESteamId Owner;

	/** App that created this item. */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|UGC")
	int32 CreatorAppId = 0;

	/** App that consumes this item. */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|UGC")
	int32 ConsumerAppId = 0;

	/** Unix time the item was created. */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|UGC")
	int64 TimeCreated = 0;

	/** Unix time the item was last updated. */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|UGC")
	int64 TimeUpdated = 0;

	/** Calculated vote score in [0, 1]. */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|UGC")
	float Score = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Steam|UGC")
	int32 VotesUp = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Steam|UGC")
	int32 VotesDown = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Steam|UGC")
	bool bBanned = false;

	/** Developer flagged this item as accepted in the Workshop. */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|UGC")
	bool bAcceptedForUse = false;

	/** The tag list was too long to be fully returned (Tags/TagList are truncated). */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|UGC")
	bool bTagsTruncated = false;

	/** Comma separated list of all tags associated with this item. */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|UGC")
	FString Tags;

	/** The item's tags parsed into individual entries. */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|UGC")
	TArray<FString> TagList;

	/** Total content size in bytes (falls back to the legacy single-file size for old items). */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|UGC")
	int64 FileSize = 0;

	/** Cloud file name of the primary file. */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|UGC")
	FString FileName;

	/** URL of the preview image (empty when the item has none). */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|UGC")
	FString PreviewUrl;

	/** Developer metadata (only populated when the query requested metadata). */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|UGC")
	FString Metadata;

	/** Key/value tags (only populated when the query requested key-value tags). */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|UGC")
	TArray<FESteamKeyValueTag> KeyValueTags;

	/** Number of child items (collections); Children is only filled when the query requested children. */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|UGC")
	int32 NumChildren = 0;

	/** Child published file ids (only populated when the query requested children). */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|UGC")
	TArray<int64> Children;

	/** Aggregate statistics keyed by type (values queried opportunistically per result). */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|UGC")
	TMap<EESteamItemStatistic, int64> Statistics;

	/** Additional previews (only populated when the query requested additional previews). */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|UGC")
	TArray<FESteamUGCAdditionalPreview> AdditionalPreviews;

	UPROPERTY(BlueprintReadOnly, Category = "Steam|UGC")
	EESteamUGCVisibility Visibility = EESteamUGCVisibility::Private;
};

/**
 * Options applied to a UGC query before it is sent. Every field is optional: defaults leave the
 * query at Steam's defaults except that long descriptions are returned (preserving the historical
 * QueryAllItems behavior). Tag/search/date/trend options that the SDK documents as "all UGC only"
 * are silently ignored by user queries.
 */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDSTEAM_API FESteamUGCQueryConfig
{
	GENERATED_BODY()

	/** Only items carrying all of these tags match (or any of them when bMatchAnyTag is set). */
	UPROPERTY(BlueprintReadWrite, Category = "Steam|UGC")
	TArray<FString> RequiredTags;

	/** Items carrying any of these tags are excluded. */
	UPROPERTY(BlueprintReadWrite, Category = "Steam|UGC")
	TArray<FString> ExcludedTags;

	/** Only items carrying all of these key/value tags match. */
	UPROPERTY(BlueprintReadWrite, Category = "Steam|UGC")
	TArray<FESteamKeyValueTag> RequiredKeyValueTags;

	/** Match items that have any of the required tags rather than all of them (all-UGC queries). */
	UPROPERTY(BlueprintReadWrite, Category = "Steam|UGC")
	bool bMatchAnyTag = false;

	/** Full-text search string (all-UGC queries; use with RankedByTextSearch). */
	UPROPERTY(BlueprintReadWrite, Category = "Steam|UGC")
	FString SearchText;

	/** ISO language code to fetch the title/description in (empty = default). */
	UPROPERTY(BlueprintReadWrite, Category = "Steam|UGC")
	FString Language;

	/** Accept a cached response up to this many seconds old (0 = do not request cached data). */
	UPROPERTY(BlueprintReadWrite, Category = "Steam|UGC")
	int32 AllowCachedResponseMaxAgeSeconds = 0;

	/** Trend window in days for RankedByTrend queries (0 = leave at default). */
	UPROPERTY(BlueprintReadWrite, Category = "Steam|UGC")
	int32 RankedByTrendDays = 0;

	/** Only match items created in [Start, End] (unix time; both must be > 0 to apply). */
	UPROPERTY(BlueprintReadWrite, Category = "Steam|UGC")
	int64 TimeCreatedStart = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Steam|UGC")
	int64 TimeCreatedEnd = 0;

	/** Only match items updated in [Start, End] (unix time; both must be > 0 to apply). */
	UPROPERTY(BlueprintReadWrite, Category = "Steam|UGC")
	int64 TimeUpdatedStart = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Steam|UGC")
	int64 TimeUpdatedEnd = 0;

	/** Return the full (long) description rather than a truncated one. */
	UPROPERTY(BlueprintReadWrite, Category = "Steam|UGC")
	bool bReturnLongDescription = true;

	/** Return developer metadata for each result. */
	UPROPERTY(BlueprintReadWrite, Category = "Steam|UGC")
	bool bReturnMetadata = false;

	/** Return key/value tags for each result. */
	UPROPERTY(BlueprintReadWrite, Category = "Steam|UGC")
	bool bReturnKeyValueTags = false;

	/** Return child items (for collections). */
	UPROPERTY(BlueprintReadWrite, Category = "Steam|UGC")
	bool bReturnChildren = false;

	/** Return additional previews for each result. */
	UPROPERTY(BlueprintReadWrite, Category = "Steam|UGC")
	bool bReturnAdditionalPreviews = false;

	/** Return only the total match count (no per-item data). */
	UPROPERTY(BlueprintReadWrite, Category = "Steam|UGC")
	bool bReturnTotalOnly = false;

	/** Return only published file ids (no other per-item data). */
	UPROPERTY(BlueprintReadWrite, Category = "Steam|UGC")
	bool bReturnOnlyIds = false;

	/** Return playtime statistics aggregated over this many days (0 = off). */
	UPROPERTY(BlueprintReadWrite, Category = "Steam|UGC")
	int32 ReturnPlaytimeStatsDays = 0;
};

/** Fired when a CreateItem request completed. bNeedsLegalAgreement means the user must accept the Workshop legal agreement before the item is visible. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSteamUGCItemCreated, bool, bSuccess, int64, PublishedFileId, bool, bNeedsLegalAgreement);

/** Fired when a SubmitItemUpdate request completed. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSteamUGCItemSubmitted, bool, bSuccess, bool, bNeedsLegalAgreement);

/** Fired when a SubscribeItem request completed. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSteamUGCSubscribed, bool, bSuccess, int64, PublishedFileId);

/** Fired when an UnsubscribeItem request completed. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSteamUGCUnsubscribed, bool, bSuccess, int64, PublishedFileId);

/** Fired when a QueryAllItems / QueryItemsByIds / QueryUserItems request completed. TotalMatchingResults is the full match count across all pages. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSteamUGCQueryCompleted, bool, bSuccess, const TArray<FESteamUGCDetails>&, Results, int32, TotalMatchingResults);

/** Fired when a DownloadItem request finished (files on disk are safe to use again). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSteamUGCItemDownloaded, bool, bSuccess, int64, PublishedFileId);

/** Fired when a Workshop item finished installing or updating on this client. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSteamUGCItemInstalled, int64, PublishedFileId);

/** Fired when an AddItemToFavorites / RemoveItemFromFavorites request completed. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSteamUGCFavoriteChanged, bool, bSuccess, int64, PublishedFileId, bool, bWasAddRequest);

/** Fired when a SetUserItemVote request completed. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSteamUGCUserItemVoteSet, bool, bSuccess, int64, PublishedFileId, bool, bVoteUp);

/** Fired when a GetUserItemVote request completed. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FOnSteamUGCUserItemVote, bool, bSuccess, int64, PublishedFileId, bool, bVotedUp, bool, bVotedDown, bool, bVoteSkipped);

/** Fired when a DeleteItem request completed. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSteamUGCItemDeleted, bool, bSuccess, int64, PublishedFileId);

/** Fired when an AddDependency / RemoveDependency request completed. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSteamUGCDependencyResult, bool, bSuccess, int64, ParentPublishedFileId, int64, ChildPublishedFileId);

/** Fired when an AddAppDependency / RemoveAppDependency request completed. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSteamUGCAppDependencyChanged, bool, bSuccess, int64, PublishedFileId, int32, AppId);

/** Fired when a GetAppDependencies request completed (may fire more than once for large lists). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSteamUGCAppDependencies, bool, bSuccess, int64, PublishedFileId, const TArray<int32>&, AppIds);

/** Fired when a Start/Stop playtime tracking request completed. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSteamUGCPlaytimeTracking, bool, bSuccess);

/** Fired when the local set of subscribed items changed (SDK 1.51+). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSteamUGCSubscribedItemsChanged, int32, AppId);

/** Fired when a GetWorkshopEULAStatus request completed (SDK 1.53+). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnSteamUGCWorkshopEULAStatus, bool, bSuccess, bool, bAccepted, bool, bNeedsAction, int32, Version);

/**
 * Wraps ISteamUGC: Workshop item creation/update, subscriptions, downloads, queries, favorites,
 * voting, dependencies and playtime tracking.
 *
 * PublishedFileId_t and UGCUpdateHandle_t are uint64 in Steamworks but travel through
 * Blueprint as int64 (Blueprint has no unsigned 64-bit type); the values are reinterpreted
 * with static_cast in both directions. Treat them as opaque handles and only feed back
 * values received from this subsystem. 0 is the invalid value for both.
 *
 * Concurrency: each async operation type owns a single Steam call-result, so same-type requests
 * are serialized via an internal FIFO queue; they complete in order, none are dropped. Starting a
 * new request of the same type before the previous one completed enqueues it, and it is issued
 * automatically when the previous request of that type completes. Operations that share a Steam
 * result struct (all three query entry points; favorites add/remove; playtime stop/stop-all) share
 * one queue.
 */
UCLASS()
class UNREALEXTENDEDSTEAM_API UESteamUGCSubsystem : public UESteamSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	// ---- Item creation / update ----

	/** Creates a new Workshop item with no content attached yet. Result arrives on OnItemCreated. */
	UFUNCTION(BlueprintCallable, Category = "Steam|UGC")
	bool CreateItem(EESteamWorkshopFileType FileType);

	/**
	 * Starts an update of an existing item and returns the update handle (0 on failure).
	 * Set the changed properties with the SetItem* functions, then call SubmitItemUpdate.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|UGC")
	int64 StartItemUpdate(int64 PublishedFileId);

	UFUNCTION(BlueprintCallable, Category = "Steam|UGC")
	bool SetItemTitle(int64 UpdateHandle, const FString& Title);

	UFUNCTION(BlueprintCallable, Category = "Steam|UGC")
	bool SetItemDescription(int64 UpdateHandle, const FString& Description);

	UFUNCTION(BlueprintCallable, Category = "Steam|UGC")
	bool SetItemVisibility(int64 UpdateHandle, EESteamUGCVisibility Visibility);

	/** Replaces the item's tags with the given list. */
	UFUNCTION(BlueprintCallable, Category = "Steam|UGC")
	bool SetItemTags(int64 UpdateHandle, const TArray<FString>& Tags);

	/** Sets the item content from a local folder (absolute path); the whole folder is uploaded. */
	UFUNCTION(BlueprintCallable, Category = "Steam|UGC")
	bool SetItemContent(int64 UpdateHandle, const FString& AbsoluteFolder);

	/** Sets the preview image from a local file (absolute path, must be under 1MB). */
	UFUNCTION(BlueprintCallable, Category = "Steam|UGC")
	bool SetItemPreview(int64 UpdateHandle, const FString& AbsoluteImagePath);

	/** Sets the item's developer metadata (max 5000 bytes). */
	UFUNCTION(BlueprintCallable, Category = "Steam|UGC")
	bool SetItemMetadata(int64 UpdateHandle, const FString& Metadata);

	/** Sets the language of the title/description being set on this update (ISO code). */
	UFUNCTION(BlueprintCallable, Category = "Steam|UGC")
	bool SetItemUpdateLanguage(int64 UpdateHandle, const FString& Language);

	/** Adds a key/value tag to the item (a key may hold multiple values). */
	UFUNCTION(BlueprintCallable, Category = "Steam|UGC")
	bool AddItemKeyValueTag(int64 UpdateHandle, const FString& Key, const FString& Value);

	/** Removes any existing key/value tags with the given key. */
	UFUNCTION(BlueprintCallable, Category = "Steam|UGC")
	bool RemoveItemKeyValueTags(int64 UpdateHandle, const FString& Key);

	/** Removes all existing key/value tags from the item. */
	UFUNCTION(BlueprintCallable, Category = "Steam|UGC")
	bool RemoveAllItemKeyValueTags(int64 UpdateHandle);

	/** Adds an additional preview file (absolute path, under 1MB) of the given type. */
	UFUNCTION(BlueprintCallable, Category = "Steam|UGC")
	bool AddItemPreviewFile(int64 UpdateHandle, const FString& AbsolutePreviewFile, EESteamItemPreviewType PreviewType);

	/** Adds an additional preview video by id (e.g. a YouTube video id). */
	UFUNCTION(BlueprintCallable, Category = "Steam|UGC")
	bool AddItemPreviewVideo(int64 UpdateHandle, const FString& VideoId);

	/** Replaces an existing preview file at the given index (absolute path, under 1MB). */
	UFUNCTION(BlueprintCallable, Category = "Steam|UGC")
	bool UpdateItemPreviewFile(int64 UpdateHandle, int32 Index, const FString& AbsolutePreviewFile);

	/** Replaces an existing preview video id at the given index. */
	UFUNCTION(BlueprintCallable, Category = "Steam|UGC")
	bool UpdateItemPreviewVideo(int64 UpdateHandle, int32 Index, const FString& VideoId);

	/** Removes the preview at the given index (previews are re-sorted afterwards). */
	UFUNCTION(BlueprintCallable, Category = "Steam|UGC")
	bool RemoveItemPreview(int64 UpdateHandle, int32 Index);

	/** Allows uploading a single small file via the legacy path (content must be one file < 10MB). */
	UFUNCTION(BlueprintCallable, Category = "Steam|UGC")
	bool SetAllowLegacyUpload(int64 UpdateHandle, bool bAllowLegacyUpload);

	/** Commits the update started with StartItemUpdate. Result arrives on OnItemSubmitted. */
	UFUNCTION(BlueprintCallable, Category = "Steam|UGC")
	bool SubmitItemUpdate(int64 UpdateHandle, const FString& ChangeNote);

	/** Progress of a running SubmitItemUpdate (byte counts are only meaningful while uploading). */
	UFUNCTION(BlueprintCallable, Category = "Steam|UGC")
	EESteamItemUpdateStatus GetItemUpdateProgress(int64 UpdateHandle, int64& BytesProcessed, int64& BytesTotal) const;

	// ---- Subscriptions / installation ----

	/** Subscribes to an item; it will be downloaded and installed ASAP. Result arrives on OnSubscribed. */
	UFUNCTION(BlueprintCallable, Category = "Steam|UGC")
	bool SubscribeItem(int64 PublishedFileId);

	/** Unsubscribes from an item; it is uninstalled after the game quits. Result arrives on OnUnsubscribed. */
	UFUNCTION(BlueprintCallable, Category = "Steam|UGC")
	bool UnsubscribeItem(int64 PublishedFileId);

	/**
	 * Number of items the current user is subscribed to (0 when unavailable).
	 * @param bIncludeLocallyDisabled Also count items disabled locally (SDK 1.51+; ignored on older SDKs).
	 */
	UFUNCTION(BlueprintPure, Category = "Steam|UGC")
	int32 GetNumSubscribedItems(bool bIncludeLocallyDisabled = false) const;

	/**
	 * All published file ids the current user is subscribed to.
	 * @param bIncludeLocallyDisabled Also include items disabled locally (SDK 1.51+; ignored on older SDKs).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|UGC")
	void GetSubscribedItems(TArray<int64>& OutPublishedFileIds, bool bIncludeLocallyDisabled = false) const;

	/** Local client state of an item (all false when unavailable or untracked). */
	UFUNCTION(BlueprintPure, Category = "Steam|UGC")
	FESteamUGCItemState GetItemState(int64 PublishedFileId) const;

	/** Install location, size on disk and last-update unix time of an installed item. */
	UFUNCTION(BlueprintCallable, Category = "Steam|UGC")
	bool GetItemInstallInfo(int64 PublishedFileId, FString& OutFolder, int64& OutSizeOnDisk, int64& OutTimestamp) const;

	/** Progress of a pending download/update: byte counts of an item with a needs-update state. */
	UFUNCTION(BlueprintCallable, Category = "Steam|UGC")
	bool GetItemDownloadInfo(int64 PublishedFileId, int64& BytesDownloaded, int64& BytesTotal) const;

	/**
	 * Downloads or updates an item immediately. Completion arrives on OnItemDownloaded;
	 * do not touch the item's files on disk until then. bHighPriority suspends all other
	 * Workshop downloads in favor of this one.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|UGC")
	bool DownloadItem(int64 PublishedFileId, bool bHighPriority);

	/** Suspends (or resumes) all Workshop downloads until resumed or the game ends. */
	UFUNCTION(BlueprintCallable, Category = "Steam|UGC")
	void SuspendDownloads(bool bSuspend);

	// ---- Queries ----

	/**
	 * Queries all Workshop items of this app, sorted by QueryType. Page starts at 1;
	 * Steam returns up to 50 results per page. Result arrives on OnQueryCompleted.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|UGC")
	bool QueryAllItems(EESteamUGCQueryType QueryType, EESteamUGCMatchingType MatchingType, int32 Page);

	/**
	 * Queries all Workshop items of this app with fine-grained filtering/return options.
	 * Page starts at 1. Result arrives on OnQueryCompleted.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|UGC")
	bool QueryAllItemsAdvanced(EESteamUGCQueryType QueryType, EESteamUGCMatchingType MatchingType, int32 Page, const FESteamUGCQueryConfig& Config);

	/** Queries the details of specific items by id. Result arrives on OnQueryCompleted. */
	UFUNCTION(BlueprintCallable, Category = "Steam|UGC")
	bool QueryItemsByIds(const TArray<int64>& PublishedFileIds);

	/**
	 * Queries one of a user's UGC lists (published, subscribed, favorited...) with filtering/return
	 * options. Page starts at 1. Result arrives on OnQueryCompleted.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|UGC")
	bool QueryUserItems(FESteamId User, EESteamUserUGCList ListType, EESteamUserUGCListSortOrder SortOrder, EESteamUGCMatchingType MatchingType, int32 Page, const FESteamUGCQueryConfig& Config);

	// ---- Favorites / voting ----

	/** Adds an item to the current user's favorites. Result arrives on OnItemFavoriteChanged. */
	UFUNCTION(BlueprintCallable, Category = "Steam|UGC")
	bool AddItemToFavorites(int32 AppId, int64 PublishedFileId);

	/** Removes an item from the current user's favorites. Result arrives on OnItemFavoriteChanged. */
	UFUNCTION(BlueprintCallable, Category = "Steam|UGC")
	bool RemoveItemFromFavorites(int32 AppId, int64 PublishedFileId);

	/** Casts the current user's up/down vote on an item. Result arrives on OnUserItemVoteSet. */
	UFUNCTION(BlueprintCallable, Category = "Steam|UGC")
	bool SetUserItemVote(int64 PublishedFileId, bool bVoteUp);

	/** Requests the current user's existing vote on an item. Result arrives on OnUserItemVote. */
	UFUNCTION(BlueprintCallable, Category = "Steam|UGC")
	bool GetUserItemVote(int64 PublishedFileId);

	/** Deletes an item the current user owns without prompting. Result arrives on OnItemDeleted. */
	UFUNCTION(BlueprintCallable, Category = "Steam|UGC")
	bool DeleteItem(int64 PublishedFileId);

	// ---- Dependencies ----

	/** Adds a child item as a dependency of a parent (collection). Result arrives on OnDependencyAdded. */
	UFUNCTION(BlueprintCallable, Category = "Steam|UGC")
	bool AddDependency(int64 ParentPublishedFileId, int64 ChildPublishedFileId);

	/** Removes a child dependency from a parent. Result arrives on OnDependencyRemoved. */
	UFUNCTION(BlueprintCallable, Category = "Steam|UGC")
	bool RemoveDependency(int64 ParentPublishedFileId, int64 ChildPublishedFileId);

	/** Adds an app (usually DLC) as a required dependency of an item. Result arrives on OnAppDependencyAdded. */
	UFUNCTION(BlueprintCallable, Category = "Steam|UGC")
	bool AddAppDependency(int64 PublishedFileId, int32 AppId);

	/** Removes an app dependency from an item. Result arrives on OnAppDependencyRemoved. */
	UFUNCTION(BlueprintCallable, Category = "Steam|UGC")
	bool RemoveAppDependency(int64 PublishedFileId, int32 AppId);

	/** Requests the app dependencies of an item. Result(s) arrive on OnAppDependenciesReceived. */
	UFUNCTION(BlueprintCallable, Category = "Steam|UGC")
	bool GetAppDependencies(int64 PublishedFileId);

	// ---- Playtime tracking ----

	/** Starts tracking playtime for the given items. Result arrives on OnPlaytimeTrackingStarted. */
	UFUNCTION(BlueprintCallable, Category = "Steam|UGC")
	bool StartPlaytimeTracking(const TArray<int64>& PublishedFileIds);

	/** Stops tracking playtime for the given items. Result arrives on OnPlaytimeTrackingStopped. */
	UFUNCTION(BlueprintCallable, Category = "Steam|UGC")
	bool StopPlaytimeTracking(const TArray<int64>& PublishedFileIds);

	/** Stops tracking playtime for all items. Result arrives on OnPlaytimeTrackingStopped. */
	UFUNCTION(BlueprintCallable, Category = "Steam|UGC")
	bool StopPlaytimeTrackingForAllItems();

	// ---- Game server / EULA ----

	/**
	 * Points the game server UGC API at a specific Workshop folder before any UGC commands, so
	 * several servers can run from one install. Returns false on older SDKs or when unavailable.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|UGC")
	bool BInitWorkshopForGameServer(int32 WorkshopDepotId, const FString& Folder);

	/** Shows the app's latest Workshop EULA in the overlay (SDK 1.53+). Returns false on older SDKs. */
	UFUNCTION(BlueprintCallable, Category = "Steam|UGC")
	bool ShowWorkshopEULA();

	/**
	 * Requests the user's acceptance status of the app's Workshop EULA (SDK 1.53+).
	 * Result arrives on OnWorkshopEULAStatus. Returns false on older SDKs.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|UGC")
	bool GetWorkshopEULAStatus();

	// ---- Events ----

	UPROPERTY(BlueprintAssignable, Category = "Steam|UGC")
	FOnSteamUGCItemCreated OnItemCreated;

	UPROPERTY(BlueprintAssignable, Category = "Steam|UGC")
	FOnSteamUGCItemSubmitted OnItemSubmitted;

	UPROPERTY(BlueprintAssignable, Category = "Steam|UGC")
	FOnSteamUGCSubscribed OnSubscribed;

	UPROPERTY(BlueprintAssignable, Category = "Steam|UGC")
	FOnSteamUGCUnsubscribed OnUnsubscribed;

	UPROPERTY(BlueprintAssignable, Category = "Steam|UGC")
	FOnSteamUGCQueryCompleted OnQueryCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Steam|UGC")
	FOnSteamUGCItemDownloaded OnItemDownloaded;

	UPROPERTY(BlueprintAssignable, Category = "Steam|UGC")
	FOnSteamUGCItemInstalled OnItemInstalled;

	UPROPERTY(BlueprintAssignable, Category = "Steam|UGC")
	FOnSteamUGCFavoriteChanged OnItemFavoriteChanged;

	UPROPERTY(BlueprintAssignable, Category = "Steam|UGC")
	FOnSteamUGCUserItemVoteSet OnUserItemVoteSet;

	UPROPERTY(BlueprintAssignable, Category = "Steam|UGC")
	FOnSteamUGCUserItemVote OnUserItemVote;

	UPROPERTY(BlueprintAssignable, Category = "Steam|UGC")
	FOnSteamUGCItemDeleted OnItemDeleted;

	UPROPERTY(BlueprintAssignable, Category = "Steam|UGC")
	FOnSteamUGCDependencyResult OnDependencyAdded;

	UPROPERTY(BlueprintAssignable, Category = "Steam|UGC")
	FOnSteamUGCDependencyResult OnDependencyRemoved;

	UPROPERTY(BlueprintAssignable, Category = "Steam|UGC")
	FOnSteamUGCAppDependencyChanged OnAppDependencyAdded;

	UPROPERTY(BlueprintAssignable, Category = "Steam|UGC")
	FOnSteamUGCAppDependencyChanged OnAppDependencyRemoved;

	UPROPERTY(BlueprintAssignable, Category = "Steam|UGC")
	FOnSteamUGCAppDependencies OnAppDependenciesReceived;

	UPROPERTY(BlueprintAssignable, Category = "Steam|UGC")
	FOnSteamUGCPlaytimeTracking OnPlaytimeTrackingStarted;

	UPROPERTY(BlueprintAssignable, Category = "Steam|UGC")
	FOnSteamUGCPlaytimeTracking OnPlaytimeTrackingStopped;

	/** Fires when the local subscribed-items list changes (SDK 1.51+). */
	UPROPERTY(BlueprintAssignable, Category = "Steam|UGC")
	FOnSteamUGCSubscribedItemsChanged OnSubscribedItemsChanged;

	/** Fires with the result of GetWorkshopEULAStatus (SDK 1.53+). */
	UPROPERTY(BlueprintAssignable, Category = "Steam|UGC")
	FOnSteamUGCWorkshopEULAStatus OnWorkshopEULAStatus;

protected:
	virtual void HandleSteamClientInitialized() override;
	virtual void HandleSteamClientShutdown() override;

private:
	friend class FESteamUGCCallbacks;
	TSharedPtr<class FESteamUGCCallbacks> Callbacks;
};
