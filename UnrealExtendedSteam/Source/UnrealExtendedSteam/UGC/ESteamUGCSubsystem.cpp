// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "UGC/ESteamUGCSubsystem.h"
#include "Shared/ESteamLog.h"
#include "Shared/ESteamSDK.h"
#include "Containers/Queue.h"

#if WITH_EXTENDEDSTEAM_SDK
THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
THIRD_PARTY_INCLUDES_END

namespace
{
	EWorkshopFileType ToSteamWorkshopFileType(EESteamWorkshopFileType FileType)
	{
		switch (FileType)
		{
		case EESteamWorkshopFileType::Microtransaction: return k_EWorkshopFileTypeMicrotransaction;
		default:                                        return k_EWorkshopFileTypeCommunity;
		}
	}

	ERemoteStoragePublishedFileVisibility ToSteamVisibility(EESteamUGCVisibility Visibility)
	{
		switch (Visibility)
		{
		case EESteamUGCVisibility::Public:      return k_ERemoteStoragePublishedFileVisibilityPublic;
		case EESteamUGCVisibility::FriendsOnly: return k_ERemoteStoragePublishedFileVisibilityFriendsOnly;
		case EESteamUGCVisibility::Unlisted:    return k_ERemoteStoragePublishedFileVisibilityUnlisted;
		default:                                return k_ERemoteStoragePublishedFileVisibilityPrivate;
		}
	}

	EESteamUGCVisibility FromSteamVisibility(ERemoteStoragePublishedFileVisibility Visibility)
	{
		switch (Visibility)
		{
		case k_ERemoteStoragePublishedFileVisibilityPublic:      return EESteamUGCVisibility::Public;
		case k_ERemoteStoragePublishedFileVisibilityFriendsOnly: return EESteamUGCVisibility::FriendsOnly;
		case k_ERemoteStoragePublishedFileVisibilityUnlisted:    return EESteamUGCVisibility::Unlisted;
		default:                                                 return EESteamUGCVisibility::Private;
		}
	}

	EUGCQuery ToSteamUGCQuery(EESteamUGCQueryType QueryType)
	{
		switch (QueryType)
		{
		case EESteamUGCQueryType::RankedByVote:                              return k_EUGCQuery_RankedByVote;
		case EESteamUGCQueryType::RankedByPublicationDate:                   return k_EUGCQuery_RankedByPublicationDate;
		case EESteamUGCQueryType::AcceptedForGameRankedByAcceptanceDate:     return k_EUGCQuery_AcceptedForGameRankedByAcceptanceDate;
		case EESteamUGCQueryType::RankedByTrend:                             return k_EUGCQuery_RankedByTrend;
		case EESteamUGCQueryType::FavoritedByFriendsRankedByPublicationDate: return k_EUGCQuery_FavoritedByFriendsRankedByPublicationDate;
		case EESteamUGCQueryType::CreatedByFriendsRankedByPublicationDate:   return k_EUGCQuery_CreatedByFriendsRankedByPublicationDate;
		case EESteamUGCQueryType::RankedByTotalUniqueSubscriptions:          return k_EUGCQuery_RankedByTotalUniqueSubscriptions;
		case EESteamUGCQueryType::RankedByTextSearch:                        return k_EUGCQuery_RankedByTextSearch;
		default:                                                             return k_EUGCQuery_RankedByPublicationDate;
		}
	}

	EUGCMatchingUGCType ToSteamMatchingType(EESteamUGCMatchingType MatchingType)
	{
		switch (MatchingType)
		{
		case EESteamUGCMatchingType::Collections:        return k_EUGCMatchingUGCType_Collections;
		case EESteamUGCMatchingType::Artwork:            return k_EUGCMatchingUGCType_Artwork;
		case EESteamUGCMatchingType::Videos:             return k_EUGCMatchingUGCType_Videos;
		case EESteamUGCMatchingType::Screenshots:        return k_EUGCMatchingUGCType_Screenshots;
		case EESteamUGCMatchingType::UsableInGame:       return k_EUGCMatchingUGCType_UsableInGame;
		case EESteamUGCMatchingType::ControllerBindings: return k_EUGCMatchingUGCType_ControllerBindings;
		case EESteamUGCMatchingType::GameManagedItems:   return k_EUGCMatchingUGCType_GameManagedItems;
		default:                                         return k_EUGCMatchingUGCType_Items;
		}
	}

	EUserUGCList ToSteamUserUGCList(EESteamUserUGCList List)
	{
		switch (List)
		{
		case EESteamUserUGCList::VotedOn:       return k_EUserUGCList_VotedOn;
		case EESteamUserUGCList::VotedUp:       return k_EUserUGCList_VotedUp;
		case EESteamUserUGCList::VotedDown:     return k_EUserUGCList_VotedDown;
		case EESteamUserUGCList::WillVoteLater: return k_EUserUGCList_WillVoteLater;
		case EESteamUserUGCList::Favorited:     return k_EUserUGCList_Favorited;
		case EESteamUserUGCList::Subscribed:    return k_EUserUGCList_Subscribed;
		case EESteamUserUGCList::UsedOrPlayed:  return k_EUserUGCList_UsedOrPlayed;
		case EESteamUserUGCList::Followed:      return k_EUserUGCList_Followed;
		default:                                return k_EUserUGCList_Published;
		}
	}

	EUserUGCListSortOrder ToSteamUserUGCListSortOrder(EESteamUserUGCListSortOrder Order)
	{
		switch (Order)
		{
		case EESteamUserUGCListSortOrder::CreationOrderAsc:     return k_EUserUGCListSortOrder_CreationOrderAsc;
		case EESteamUserUGCListSortOrder::TitleAsc:             return k_EUserUGCListSortOrder_TitleAsc;
		case EESteamUserUGCListSortOrder::LastUpdatedDesc:      return k_EUserUGCListSortOrder_LastUpdatedDesc;
		case EESteamUserUGCListSortOrder::SubscriptionDateDesc: return k_EUserUGCListSortOrder_SubscriptionDateDesc;
		case EESteamUserUGCListSortOrder::VoteScoreDesc:        return k_EUserUGCListSortOrder_VoteScoreDesc;
		case EESteamUserUGCListSortOrder::ForModeration:        return k_EUserUGCListSortOrder_ForModeration;
		default:                                                return k_EUserUGCListSortOrder_CreationOrderDesc;
		}
	}

	EItemPreviewType ToSteamItemPreviewType(EESteamItemPreviewType Type)
	{
		switch (Type)
		{
		case EESteamItemPreviewType::YouTubeVideo:                   return k_EItemPreviewType_YouTubeVideo;
		case EESteamItemPreviewType::Sketchfab:                      return k_EItemPreviewType_Sketchfab;
		case EESteamItemPreviewType::EnvironmentMap_HorizontalCross: return k_EItemPreviewType_EnvironmentMap_HorizontalCross;
		case EESteamItemPreviewType::EnvironmentMap_LatLong:         return k_EItemPreviewType_EnvironmentMap_LatLong;
		case EESteamItemPreviewType::Clip:                          return k_EItemPreviewType_Clip;
		case EESteamItemPreviewType::ReservedMax:                    return k_EItemPreviewType_ReservedMax;
		default:                                                     return k_EItemPreviewType_Image;
		}
	}

	EESteamItemPreviewType FromSteamItemPreviewType(EItemPreviewType Type)
	{
		switch (Type)
		{
		case k_EItemPreviewType_YouTubeVideo:                   return EESteamItemPreviewType::YouTubeVideo;
		case k_EItemPreviewType_Sketchfab:                      return EESteamItemPreviewType::Sketchfab;
		case k_EItemPreviewType_EnvironmentMap_HorizontalCross: return EESteamItemPreviewType::EnvironmentMap_HorizontalCross;
		case k_EItemPreviewType_EnvironmentMap_LatLong:         return EESteamItemPreviewType::EnvironmentMap_LatLong;
		case k_EItemPreviewType_Clip:                          return EESteamItemPreviewType::Clip;
		case k_EItemPreviewType_Image:                          return EESteamItemPreviewType::Image;
		default:                                                return EESteamItemPreviewType::ReservedMax;
		}
	}

	/** Applies all set filter/return options of a query config to a created (not yet sent) query handle. */
	void ApplyUGCQueryConfig(UGCQueryHandle_t Handle, const FESteamUGCQueryConfig& Config)
	{
		ISteamUGC* UGC = SteamUGC();
		if (!UGC)
		{
			return;
		}

		UGC->SetReturnLongDescription(Handle, Config.bReturnLongDescription);
		UGC->SetReturnMetadata(Handle, Config.bReturnMetadata);
		UGC->SetReturnKeyValueTags(Handle, Config.bReturnKeyValueTags);
		UGC->SetReturnChildren(Handle, Config.bReturnChildren);
		UGC->SetReturnAdditionalPreviews(Handle, Config.bReturnAdditionalPreviews);
		UGC->SetReturnTotalOnly(Handle, Config.bReturnTotalOnly);
		UGC->SetReturnOnlyIDs(Handle, Config.bReturnOnlyIds);
		if (Config.ReturnPlaytimeStatsDays > 0)
		{
			UGC->SetReturnPlaytimeStats(Handle, static_cast<uint32>(Config.ReturnPlaytimeStatsDays));
		}

		for (const FString& Tag : Config.RequiredTags)
		{
			UGC->AddRequiredTag(Handle, TCHAR_TO_UTF8(*Tag));
		}
		for (const FString& Tag : Config.ExcludedTags)
		{
			UGC->AddExcludedTag(Handle, TCHAR_TO_UTF8(*Tag));
		}
		for (const FESteamKeyValueTag& KeyValue : Config.RequiredKeyValueTags)
		{
			UGC->AddRequiredKeyValueTag(Handle, TCHAR_TO_UTF8(*KeyValue.Key), TCHAR_TO_UTF8(*KeyValue.Value));
		}

		if (Config.bMatchAnyTag)
		{
			UGC->SetMatchAnyTag(Handle, true);
		}
		if (!Config.SearchText.IsEmpty())
		{
			UGC->SetSearchText(Handle, TCHAR_TO_UTF8(*Config.SearchText));
		}
		if (!Config.Language.IsEmpty())
		{
			UGC->SetLanguage(Handle, TCHAR_TO_UTF8(*Config.Language));
		}
		if (Config.AllowCachedResponseMaxAgeSeconds > 0)
		{
			UGC->SetAllowCachedResponse(Handle, static_cast<uint32>(Config.AllowCachedResponseMaxAgeSeconds));
		}
		if (Config.RankedByTrendDays > 0)
		{
			UGC->SetRankedByTrendDays(Handle, static_cast<uint32>(Config.RankedByTrendDays));
		}
		if (Config.TimeCreatedStart > 0 && Config.TimeCreatedEnd > 0)
		{
			UGC->SetTimeCreatedDateRange(Handle, static_cast<RTime32>(Config.TimeCreatedStart), static_cast<RTime32>(Config.TimeCreatedEnd));
		}
		if (Config.TimeUpdatedStart > 0 && Config.TimeUpdatedEnd > 0)
		{
			UGC->SetTimeUpdatedDateRange(Handle, static_cast<RTime32>(Config.TimeUpdatedStart), static_cast<RTime32>(Config.TimeUpdatedEnd));
		}
	}

	/** Converts one query result (and its opportunistic per-index getters) into an FESteamUGCDetails. */
	void ParseUGCDetails(UGCQueryHandle_t Handle, uint32 Index, const SteamUGCDetails_t& Details, FESteamUGCDetails& Out)
	{
		Out.PublishedFileId = static_cast<int64>(Details.m_nPublishedFileId);
		Out.Title = FString(UTF8_TO_TCHAR(Details.m_rgchTitle));
		Out.Description = FString(UTF8_TO_TCHAR(Details.m_rgchDescription));
		Out.Owner = FESteamId(Details.m_ulSteamIDOwner);
		Out.CreatorAppId = static_cast<int32>(Details.m_nCreatorAppID);
		Out.ConsumerAppId = static_cast<int32>(Details.m_nConsumerAppID);
		Out.TimeCreated = static_cast<int64>(Details.m_rtimeCreated);
		Out.TimeUpdated = static_cast<int64>(Details.m_rtimeUpdated);
		Out.Score = Details.m_flScore;
		Out.VotesUp = static_cast<int32>(Details.m_unVotesUp);
		Out.VotesDown = static_cast<int32>(Details.m_unVotesDown);
		Out.bBanned = Details.m_bBanned;
		Out.bAcceptedForUse = Details.m_bAcceptedForUse;
		Out.bTagsTruncated = Details.m_bTagsTruncated;
		Out.Tags = FString(UTF8_TO_TCHAR(Details.m_rgchTags));
		Out.Tags.ParseIntoArray(Out.TagList, TEXT(","), true);
		// Non-legacy items report their full content size in m_ulTotalFilesSize;
		// legacy single-file items only fill m_nFileSize.
		Out.FileSize = Details.m_ulTotalFilesSize > 0
			? static_cast<int64>(Details.m_ulTotalFilesSize)
			: static_cast<int64>(Details.m_nFileSize);
		Out.FileName = FString(UTF8_TO_TCHAR(Details.m_pchFileName));
		Out.NumChildren = static_cast<int32>(Details.m_unNumChildren);
		Out.Visibility = FromSteamVisibility(Details.m_eVisibility);

		ISteamUGC* UGC = SteamUGC();
		if (!UGC)
		{
			return;
		}

		char PreviewUrl[k_cchPublishedFileURLMax] = {};
		if (UGC->GetQueryUGCPreviewURL(Handle, Index, PreviewUrl, sizeof(PreviewUrl)))
		{
			Out.PreviewUrl = FString(UTF8_TO_TCHAR(PreviewUrl));
		}

		// Only populated when the query requested metadata.
		char Metadata[k_cchDeveloperMetadataMax] = {};
		if (UGC->GetQueryUGCMetadata(Handle, Index, Metadata, sizeof(Metadata)))
		{
			Out.Metadata = FString(UTF8_TO_TCHAR(Metadata));
		}

		// Key/value tags — only populated when the query requested them.
		const uint32 NumKeyValueTags = UGC->GetQueryUGCNumKeyValueTags(Handle, Index);
		Out.KeyValueTags.Reserve(NumKeyValueTags);
		for (uint32 KeyValueIndex = 0; KeyValueIndex < NumKeyValueTags; ++KeyValueIndex)
		{
			char Key[256] = {};
			char Value[256] = {};
			if (UGC->GetQueryUGCKeyValueTag(Handle, Index, KeyValueIndex, Key, sizeof(Key), Value, sizeof(Value)))
			{
				Out.KeyValueTags.Emplace(FString(UTF8_TO_TCHAR(Key)), FString(UTF8_TO_TCHAR(Value)));
			}
		}

		// Children — only populated when the query requested children.
		if (Out.NumChildren > 0)
		{
			TArray<PublishedFileId_t> ChildIds;
			ChildIds.SetNumZeroed(Out.NumChildren);
			if (UGC->GetQueryUGCChildren(Handle, Index, ChildIds.GetData(), static_cast<uint32>(ChildIds.Num())))
			{
				Out.Children.Reserve(ChildIds.Num());
				for (const PublishedFileId_t ChildId : ChildIds)
				{
					Out.Children.Add(static_cast<int64>(ChildId));
				}
			}
		}

		// Statistics — queried opportunistically; unavailable stats simply return false.
		static const EItemStatistic StatTypes[] =
		{
			k_EItemStatistic_NumSubscriptions,
			k_EItemStatistic_NumFavorites,
			k_EItemStatistic_NumFollowers,
			k_EItemStatistic_NumUniqueSubscriptions,
			k_EItemStatistic_NumUniqueFavorites,
			k_EItemStatistic_NumUniqueFollowers,
			k_EItemStatistic_NumUniqueWebsiteViews,
			k_EItemStatistic_ReportScore,
			k_EItemStatistic_NumSecondsPlayed,
			k_EItemStatistic_NumPlaytimeSessions,
			k_EItemStatistic_NumComments,
			k_EItemStatistic_NumSecondsPlayedDuringTimePeriod,
			k_EItemStatistic_NumPlaytimeSessionsDuringTimePeriod
		};
		static const EESteamItemStatistic StatEnums[] =
		{
			EESteamItemStatistic::NumSubscriptions,
			EESteamItemStatistic::NumFavorites,
			EESteamItemStatistic::NumFollowers,
			EESteamItemStatistic::NumUniqueSubscriptions,
			EESteamItemStatistic::NumUniqueFavorites,
			EESteamItemStatistic::NumUniqueFollowers,
			EESteamItemStatistic::NumUniqueWebsiteViews,
			EESteamItemStatistic::ReportScore,
			EESteamItemStatistic::NumSecondsPlayed,
			EESteamItemStatistic::NumPlaytimeSessions,
			EESteamItemStatistic::NumComments,
			EESteamItemStatistic::NumSecondsPlayedDuringTimePeriod,
			EESteamItemStatistic::NumPlaytimeSessionsDuringTimePeriod
		};
		static_assert(UE_ARRAY_COUNT(StatTypes) == UE_ARRAY_COUNT(StatEnums), "Statistic mapping tables must stay in sync");
		for (int32 StatIndex = 0; StatIndex < static_cast<int32>(UE_ARRAY_COUNT(StatTypes)); ++StatIndex)
		{
			uint64 StatValue = 0;
			if (UGC->GetQueryUGCStatistic(Handle, Index, StatTypes[StatIndex], &StatValue))
			{
				Out.Statistics.Add(StatEnums[StatIndex], static_cast<int64>(StatValue));
			}
		}

		// Additional previews — only populated when the query requested them.
		const uint32 NumAdditionalPreviews = UGC->GetQueryUGCNumAdditionalPreviews(Handle, Index);
		Out.AdditionalPreviews.Reserve(NumAdditionalPreviews);
		for (uint32 PreviewIndex = 0; PreviewIndex < NumAdditionalPreviews; ++PreviewIndex)
		{
			char UrlOrVideoId[k_cchPublishedFileURLMax] = {};
			char OriginalFileName[k_cchFilenameMax] = {};
			EItemPreviewType PreviewType = k_EItemPreviewType_Image;
			if (UGC->GetQueryUGCAdditionalPreview(Handle, Index, PreviewIndex,
				UrlOrVideoId, sizeof(UrlOrVideoId), OriginalFileName, sizeof(OriginalFileName), &PreviewType))
			{
				FESteamUGCAdditionalPreview& Preview = Out.AdditionalPreviews.AddDefaulted_GetRef();
				Preview.UrlOrVideoId = FString(UTF8_TO_TCHAR(UrlOrVideoId));
				Preview.OriginalFileName = FString(UTF8_TO_TCHAR(OriginalFileName));
				Preview.PreviewType = FromSteamItemPreviewType(PreviewType);
			}
		}
	}
}

/** Parameters captured to (re-)issue a queued CreateItem call. */
struct FESteamPendingUGCCreate
{
	EWorkshopFileType FileType = k_EWorkshopFileTypeCommunity;
};

/** Parameters captured to (re-)issue a queued SubmitItemUpdate call. */
struct FESteamPendingUGCSubmit
{
	UGCUpdateHandle_t Handle = k_UGCUpdateHandleInvalid;
	FString ChangeNote;
};

/** Parameters captured to (re-)issue a queued SubscribeItem / UnsubscribeItem call. */
struct FESteamPendingUGCSubscription
{
	PublishedFileId_t FileId = 0;
};

/** Which create call a queued query uses; a query handle is only allocated when the query is issued. */
enum class EESteamUGCQueryKind : uint8
{
	AllItems,
	ByIds,
	UserItems
};

/** Parameters captured to (re-)issue a queued query (all-items, by-ids or user-items). */
struct FESteamPendingUGCQuery
{
	EESteamUGCQueryKind Kind = EESteamUGCQueryKind::AllItems;
	// All-items / user query parameters (already converted to SDK enums):
	EUGCQuery QueryType = k_EUGCQuery_RankedByPublicationDate;
	EUGCMatchingUGCType MatchingType = k_EUGCMatchingUGCType_Items;
	uint32 Page = 1;
	// By-ids query parameters:
	TArray<PublishedFileId_t> FileIds;
	// User query parameters:
	AccountID_t AccountId = 0;
	EUserUGCList UserList = k_EUserUGCList_Published;
	EUserUGCListSortOrder SortOrder = k_EUserUGCListSortOrder_CreationOrderDesc;
	// Optional customization applied before the query is sent.
	bool bHasConfig = false;
	FESteamUGCQueryConfig Config;
};

/** Parameters captured to (re-)issue a queued AddItemToFavorites / RemoveItemFromFavorites call. */
struct FESteamPendingUGCFavorite
{
	AppId_t AppId = 0;
	PublishedFileId_t FileId = 0;
	bool bAdd = true;
};

/** Parameters captured to (re-)issue a queued SetUserItemVote call. */
struct FESteamPendingUGCVote
{
	PublishedFileId_t FileId = 0;
	bool bVoteUp = true;
};

/** Parameters captured to (re-)issue a queued single-id call (GetUserItemVote / DeleteItem / GetAppDependencies). */
struct FESteamPendingUGCFileId
{
	PublishedFileId_t FileId = 0;
};

/** Parameters captured to (re-)issue a queued AddDependency / RemoveDependency call. */
struct FESteamPendingUGCDependency
{
	PublishedFileId_t Parent = 0;
	PublishedFileId_t Child = 0;
};

/** Parameters captured to (re-)issue a queued AddAppDependency / RemoveAppDependency call. */
struct FESteamPendingUGCAppDependency
{
	PublishedFileId_t FileId = 0;
	AppId_t AppId = 0;
};

/** Parameters captured to (re-)issue a queued StartPlaytimeTracking call. */
struct FESteamPendingUGCPlaytime
{
	TArray<PublishedFileId_t> FileIds;
};

/** Parameters captured to (re-)issue a queued StopPlaytimeTracking / StopPlaytimeTrackingForAllItems call. */
struct FESteamPendingUGCPlaytimeStop
{
	TArray<PublishedFileId_t> FileIds;
	bool bAllItems = false;
};

/** Marker captured to (re-)issue a queued GetWorkshopEULAStatus call (no parameters). */
struct FESteamPendingUGCEula
{
};

/**
 * Native Steam callback listeners; alive only while the Steam client API is initialized.
 *
 * Each async operation type owns a single CCallResult, so same-type requests are serialized via a
 * per-operation FIFO queue: while a given op's CCallResult is in flight, further requests of that
 * type are enqueued and issued in order as each completion arrives, so none are dropped. Operations
 * that share a Steam result struct share one CCallResult and therefore one queue: the three query
 * entry points (SteamUGCQueryCompleted_t), favorites add/remove (UserFavoriteItemsListChanged_t) and
 * playtime stop/stop-all (StopPlaytimeTrackingResult_t). Queued-but-unissued requests capture only
 * their parameters and hold no Steam handles (a UGC query handle is created only when the query is
 * actually issued), so tearing down this holder (Steam shutdown / Deinitialize) abandons them cleanly
 * with nothing to release.
 */
class FESteamUGCCallbacks
{
public:
	explicit FESteamUGCCallbacks(UESteamUGCSubsystem* InOwner)
		: Owner(InOwner)
		, DownloadItemCallback(this, &FESteamUGCCallbacks::HandleDownloadItemResult)
		, ItemInstalledCallback(this, &FESteamUGCCallbacks::HandleItemInstalled)
#if ESTEAM_SDK_AT_LEAST(151)
		, SubscribedItemsChangedCallback(this, &FESteamUGCCallbacks::HandleSubscribedItemsChanged)
#endif
	{
	}

	// Each Enqueue* issues the Steam call immediately when its operation is idle, otherwise it
	// queues the parameters. Returns true when the request was issued or queued; false only on the
	// immediate path when the Steam call could not be issued (preserves the public methods'
	// historical "return false when the request could not be issued" contract).

	bool EnqueueCreateItem(const FESteamPendingUGCCreate& Request)
	{
		if (bCreateItemBusy)
		{
			CreateItemQueue.Enqueue(Request);
			return true;
		}
		return IssueCreateItem(Request);
	}

	bool EnqueueSubmitItemUpdate(const FESteamPendingUGCSubmit& Request)
	{
		if (bSubmitBusy)
		{
			SubmitQueue.Enqueue(Request);
			return true;
		}
		return IssueSubmitItemUpdate(Request);
	}

	bool EnqueueSubscribe(const FESteamPendingUGCSubscription& Request)
	{
		if (bSubscribeBusy)
		{
			SubscribeQueue.Enqueue(Request);
			return true;
		}
		return IssueSubscribe(Request);
	}

	bool EnqueueUnsubscribe(const FESteamPendingUGCSubscription& Request)
	{
		if (bUnsubscribeBusy)
		{
			UnsubscribeQueue.Enqueue(Request);
			return true;
		}
		return IssueUnsubscribe(Request);
	}

	// QueryAllItems, QueryItemsByIds and QueryUserItems share QueryCompletedResult, so they share one queue.
	bool EnqueueQuery(const FESteamPendingUGCQuery& Request)
	{
		if (bQueryBusy)
		{
			QueryQueue.Enqueue(Request);
			return true;
		}
		return IssueQuery(Request);
	}

	bool EnqueueFavorite(const FESteamPendingUGCFavorite& Request)
	{
		if (bFavoriteBusy)
		{
			FavoriteQueue.Enqueue(Request);
			return true;
		}
		return IssueFavorite(Request);
	}

	bool EnqueueVoteSet(const FESteamPendingUGCVote& Request)
	{
		if (bVoteSetBusy)
		{
			VoteSetQueue.Enqueue(Request);
			return true;
		}
		return IssueVoteSet(Request);
	}

	bool EnqueueVoteGet(const FESteamPendingUGCFileId& Request)
	{
		if (bVoteGetBusy)
		{
			VoteGetQueue.Enqueue(Request);
			return true;
		}
		return IssueVoteGet(Request);
	}

	bool EnqueueDelete(const FESteamPendingUGCFileId& Request)
	{
		if (bDeleteBusy)
		{
			DeleteQueue.Enqueue(Request);
			return true;
		}
		return IssueDelete(Request);
	}

	bool EnqueueAddDependency(const FESteamPendingUGCDependency& Request)
	{
		if (bAddDependencyBusy)
		{
			AddDependencyQueue.Enqueue(Request);
			return true;
		}
		return IssueAddDependency(Request);
	}

	bool EnqueueRemoveDependency(const FESteamPendingUGCDependency& Request)
	{
		if (bRemoveDependencyBusy)
		{
			RemoveDependencyQueue.Enqueue(Request);
			return true;
		}
		return IssueRemoveDependency(Request);
	}

	bool EnqueueAddAppDependency(const FESteamPendingUGCAppDependency& Request)
	{
		if (bAddAppDependencyBusy)
		{
			AddAppDependencyQueue.Enqueue(Request);
			return true;
		}
		return IssueAddAppDependency(Request);
	}

	bool EnqueueRemoveAppDependency(const FESteamPendingUGCAppDependency& Request)
	{
		if (bRemoveAppDependencyBusy)
		{
			RemoveAppDependencyQueue.Enqueue(Request);
			return true;
		}
		return IssueRemoveAppDependency(Request);
	}

	bool EnqueueGetAppDependencies(const FESteamPendingUGCFileId& Request)
	{
		if (bGetAppDependenciesBusy)
		{
			GetAppDependenciesQueue.Enqueue(Request);
			return true;
		}
		return IssueGetAppDependencies(Request);
	}

	bool EnqueueStartPlaytime(const FESteamPendingUGCPlaytime& Request)
	{
		if (bStartPlaytimeBusy)
		{
			StartPlaytimeQueue.Enqueue(Request);
			return true;
		}
		return IssueStartPlaytime(Request);
	}

	// StopPlaytimeTracking and StopPlaytimeTrackingForAllItems share StopPlaytimeResult, so they share one queue.
	bool EnqueueStopPlaytime(const FESteamPendingUGCPlaytimeStop& Request)
	{
		if (bStopPlaytimeBusy)
		{
			StopPlaytimeQueue.Enqueue(Request);
			return true;
		}
		return IssueStopPlaytime(Request);
	}

#if ESTEAM_SDK_AT_LEAST(153)
	bool EnqueueEula(const FESteamPendingUGCEula& Request)
	{
		if (bEulaBusy)
		{
			EulaQueue.Enqueue(Request);
			return true;
		}
		return IssueEula(Request);
	}
#endif

private:
	static bool IsForThisApp(AppId_t AppId)
	{
		return SteamUtils() && AppId == SteamUtils()->GetAppID();
	}

	// ---- CreateItem (serialized) ----

	bool IssueCreateItem(const FESteamPendingUGCCreate& Request)
	{
		UESteamUGCSubsystem* Subsystem = Owner.Get();
		if (!Subsystem || !Subsystem->IsSteamAvailable() || !SteamUGC() || !SteamUtils())
		{
			return false;
		}

		const SteamAPICall_t Call = SteamUGC()->CreateItem(SteamUtils()->GetAppID(), Request.FileType);
		if (Call == k_uAPICallInvalid)
		{
			return false;
		}
		CreateItemResult.Set(Call, this, &FESteamUGCCallbacks::HandleCreateItemResult);
		bCreateItemBusy = true;
		return true;
	}

	void DrainCreateItemQueue()
	{
		while (!bCreateItemBusy)
		{
			FESteamPendingUGCCreate Request;
			if (!CreateItemQueue.Dequeue(Request))
			{
				break;
			}
			if (!IssueCreateItem(Request))
			{
				// Steam went away while draining: fail this queued request instead of dropping it.
				if (UESteamUGCSubsystem* Subsystem = Owner.Get())
				{
					Subsystem->OnItemCreated.Broadcast(false, 0, false);
				}
			}
		}
	}

	// ---- SubmitItemUpdate (serialized) ----

	bool IssueSubmitItemUpdate(const FESteamPendingUGCSubmit& Request)
	{
		UESteamUGCSubsystem* Subsystem = Owner.Get();
		if (!Subsystem || !Subsystem->IsSteamAvailable() || !SteamUGC())
		{
			return false;
		}

		const SteamAPICall_t Call = SteamUGC()->SubmitItemUpdate(
			Request.Handle, Request.ChangeNote.IsEmpty() ? nullptr : TCHAR_TO_UTF8(*Request.ChangeNote));
		if (Call == k_uAPICallInvalid)
		{
			return false;
		}
		SubmitItemUpdateResult.Set(Call, this, &FESteamUGCCallbacks::HandleSubmitItemUpdateResult);
		bSubmitBusy = true;
		return true;
	}

	void DrainSubmitQueue()
	{
		while (!bSubmitBusy)
		{
			FESteamPendingUGCSubmit Request;
			if (!SubmitQueue.Dequeue(Request))
			{
				break;
			}
			if (!IssueSubmitItemUpdate(Request))
			{
				if (UESteamUGCSubsystem* Subsystem = Owner.Get())
				{
					Subsystem->OnItemSubmitted.Broadcast(false, false);
				}
			}
		}
	}

	// ---- SubscribeItem (serialized) ----

	bool IssueSubscribe(const FESteamPendingUGCSubscription& Request)
	{
		UESteamUGCSubsystem* Subsystem = Owner.Get();
		if (!Subsystem || !Subsystem->IsSteamAvailable() || !SteamUGC())
		{
			return false;
		}

		const SteamAPICall_t Call = SteamUGC()->SubscribeItem(Request.FileId);
		if (Call == k_uAPICallInvalid)
		{
			return false;
		}
		SubscribeResult.Set(Call, this, &FESteamUGCCallbacks::HandleSubscribeResult);
		bSubscribeBusy = true;
		return true;
	}

	void DrainSubscribeQueue()
	{
		while (!bSubscribeBusy)
		{
			FESteamPendingUGCSubscription Request;
			if (!SubscribeQueue.Dequeue(Request))
			{
				break;
			}
			if (!IssueSubscribe(Request))
			{
				if (UESteamUGCSubsystem* Subsystem = Owner.Get())
				{
					Subsystem->OnSubscribed.Broadcast(false, static_cast<int64>(Request.FileId));
				}
			}
		}
	}

	// ---- UnsubscribeItem (serialized) ----

	bool IssueUnsubscribe(const FESteamPendingUGCSubscription& Request)
	{
		UESteamUGCSubsystem* Subsystem = Owner.Get();
		if (!Subsystem || !Subsystem->IsSteamAvailable() || !SteamUGC())
		{
			return false;
		}

		const SteamAPICall_t Call = SteamUGC()->UnsubscribeItem(Request.FileId);
		if (Call == k_uAPICallInvalid)
		{
			return false;
		}
		UnsubscribeResult.Set(Call, this, &FESteamUGCCallbacks::HandleUnsubscribeResult);
		bUnsubscribeBusy = true;
		return true;
	}

	void DrainUnsubscribeQueue()
	{
		while (!bUnsubscribeBusy)
		{
			FESteamPendingUGCSubscription Request;
			if (!UnsubscribeQueue.Dequeue(Request))
			{
				break;
			}
			if (!IssueUnsubscribe(Request))
			{
				if (UESteamUGCSubsystem* Subsystem = Owner.Get())
				{
					Subsystem->OnUnsubscribed.Broadcast(false, static_cast<int64>(Request.FileId));
				}
			}
		}
	}

	// ---- QueryAllItems / QueryItemsByIds / QueryUserItems (serialized, shared queue) ----

	/**
	 * Issues one query. The UGC query handle is created here — so a queued query only allocates its
	 * handle when it is actually issued — then configured, sent, and on send failure released
	 * immediately. On success HandleQueryCompleted releases the handle.
	 */
	bool IssueQuery(const FESteamPendingUGCQuery& Request)
	{
		UESteamUGCSubsystem* Subsystem = Owner.Get();
		if (!Subsystem || !Subsystem->IsSteamAvailable() || !SteamUGC())
		{
			return false;
		}

		UGCQueryHandle_t Handle = k_UGCQueryHandleInvalid;
		switch (Request.Kind)
		{
		case EESteamUGCQueryKind::ByIds:
		{
			// CreateQueryUGCDetailsRequest takes a mutable id pointer, so copy into a local array.
			TArray<PublishedFileId_t> FileIds = Request.FileIds;
			Handle = SteamUGC()->CreateQueryUGCDetailsRequest(FileIds.GetData(), static_cast<uint32>(FileIds.Num()));
			break;
		}
		case EESteamUGCQueryKind::UserItems:
		{
			if (SteamUtils())
			{
				const AppId_t AppId = SteamUtils()->GetAppID();
				Handle = SteamUGC()->CreateQueryUserUGCRequest(
					Request.AccountId, Request.UserList, Request.MatchingType, Request.SortOrder, AppId, AppId, Request.Page);
			}
			break;
		}
		case EESteamUGCQueryKind::AllItems:
		default:
		{
			if (SteamUtils())
			{
				const AppId_t AppId = SteamUtils()->GetAppID();
				Handle = SteamUGC()->CreateQueryAllUGCRequest(Request.QueryType, Request.MatchingType, AppId, AppId, Request.Page);
			}
			break;
		}
		}

		if (Handle == k_UGCQueryHandleInvalid)
		{
			return false;
		}

		if (Request.bHasConfig)
		{
			ApplyUGCQueryConfig(Handle, Request.Config);
		}
		else
		{
			// Preserve the historical QueryAllItems/QueryItemsByIds behavior.
			SteamUGC()->SetReturnLongDescription(Handle, true);
		}

		const SteamAPICall_t Call = SteamUGC()->SendQueryUGCRequest(Handle);
		if (Call == k_uAPICallInvalid)
		{
			SteamUGC()->ReleaseQueryUGCRequest(Handle);
			return false;
		}
		QueryCompletedResult.Set(Call, this, &FESteamUGCCallbacks::HandleQueryCompleted);
		bQueryBusy = true;
		return true;
	}

	void DrainQueryQueue()
	{
		while (!bQueryBusy)
		{
			FESteamPendingUGCQuery Request;
			if (!QueryQueue.Dequeue(Request))
			{
				break;
			}
			if (!IssueQuery(Request))
			{
				if (UESteamUGCSubsystem* Subsystem = Owner.Get())
				{
					const TArray<FESteamUGCDetails> EmptyResults;
					Subsystem->OnQueryCompleted.Broadcast(false, EmptyResults, 0);
				}
			}
		}
	}

	// ---- Favorites (serialized, shared add/remove queue) ----

	bool IssueFavorite(const FESteamPendingUGCFavorite& Request)
	{
		UESteamUGCSubsystem* Subsystem = Owner.Get();
		if (!Subsystem || !Subsystem->IsSteamAvailable() || !SteamUGC())
		{
			return false;
		}

		const SteamAPICall_t Call = Request.bAdd
			? SteamUGC()->AddItemToFavorites(Request.AppId, Request.FileId)
			: SteamUGC()->RemoveItemFromFavorites(Request.AppId, Request.FileId);
		if (Call == k_uAPICallInvalid)
		{
			return false;
		}
		FavoriteResult.Set(Call, this, &FESteamUGCCallbacks::HandleFavoriteResult);
		bFavoriteBusy = true;
		return true;
	}

	void DrainFavoriteQueue()
	{
		while (!bFavoriteBusy)
		{
			FESteamPendingUGCFavorite Request;
			if (!FavoriteQueue.Dequeue(Request))
			{
				break;
			}
			if (!IssueFavorite(Request))
			{
				if (UESteamUGCSubsystem* Subsystem = Owner.Get())
				{
					Subsystem->OnItemFavoriteChanged.Broadcast(false, static_cast<int64>(Request.FileId), Request.bAdd);
				}
			}
		}
	}

	// ---- SetUserItemVote (serialized) ----

	bool IssueVoteSet(const FESteamPendingUGCVote& Request)
	{
		UESteamUGCSubsystem* Subsystem = Owner.Get();
		if (!Subsystem || !Subsystem->IsSteamAvailable() || !SteamUGC())
		{
			return false;
		}

		const SteamAPICall_t Call = SteamUGC()->SetUserItemVote(Request.FileId, Request.bVoteUp);
		if (Call == k_uAPICallInvalid)
		{
			return false;
		}
		VoteSetResult.Set(Call, this, &FESteamUGCCallbacks::HandleVoteSetResult);
		bVoteSetBusy = true;
		return true;
	}

	void DrainVoteSetQueue()
	{
		while (!bVoteSetBusy)
		{
			FESteamPendingUGCVote Request;
			if (!VoteSetQueue.Dequeue(Request))
			{
				break;
			}
			if (!IssueVoteSet(Request))
			{
				if (UESteamUGCSubsystem* Subsystem = Owner.Get())
				{
					Subsystem->OnUserItemVoteSet.Broadcast(false, static_cast<int64>(Request.FileId), Request.bVoteUp);
				}
			}
		}
	}

	// ---- GetUserItemVote (serialized) ----

	bool IssueVoteGet(const FESteamPendingUGCFileId& Request)
	{
		UESteamUGCSubsystem* Subsystem = Owner.Get();
		if (!Subsystem || !Subsystem->IsSteamAvailable() || !SteamUGC())
		{
			return false;
		}

		const SteamAPICall_t Call = SteamUGC()->GetUserItemVote(Request.FileId);
		if (Call == k_uAPICallInvalid)
		{
			return false;
		}
		VoteGetResult.Set(Call, this, &FESteamUGCCallbacks::HandleVoteGetResult);
		bVoteGetBusy = true;
		return true;
	}

	void DrainVoteGetQueue()
	{
		while (!bVoteGetBusy)
		{
			FESteamPendingUGCFileId Request;
			if (!VoteGetQueue.Dequeue(Request))
			{
				break;
			}
			if (!IssueVoteGet(Request))
			{
				if (UESteamUGCSubsystem* Subsystem = Owner.Get())
				{
					Subsystem->OnUserItemVote.Broadcast(false, static_cast<int64>(Request.FileId), false, false, false);
				}
			}
		}
	}

	// ---- DeleteItem (serialized) ----

	bool IssueDelete(const FESteamPendingUGCFileId& Request)
	{
		UESteamUGCSubsystem* Subsystem = Owner.Get();
		if (!Subsystem || !Subsystem->IsSteamAvailable() || !SteamUGC())
		{
			return false;
		}

		const SteamAPICall_t Call = SteamUGC()->DeleteItem(Request.FileId);
		if (Call == k_uAPICallInvalid)
		{
			return false;
		}
		DeleteResult.Set(Call, this, &FESteamUGCCallbacks::HandleDeleteResult);
		bDeleteBusy = true;
		return true;
	}

	void DrainDeleteQueue()
	{
		while (!bDeleteBusy)
		{
			FESteamPendingUGCFileId Request;
			if (!DeleteQueue.Dequeue(Request))
			{
				break;
			}
			if (!IssueDelete(Request))
			{
				if (UESteamUGCSubsystem* Subsystem = Owner.Get())
				{
					Subsystem->OnItemDeleted.Broadcast(false, static_cast<int64>(Request.FileId));
				}
			}
		}
	}

	// ---- AddDependency (serialized) ----

	bool IssueAddDependency(const FESteamPendingUGCDependency& Request)
	{
		UESteamUGCSubsystem* Subsystem = Owner.Get();
		if (!Subsystem || !Subsystem->IsSteamAvailable() || !SteamUGC())
		{
			return false;
		}

		const SteamAPICall_t Call = SteamUGC()->AddDependency(Request.Parent, Request.Child);
		if (Call == k_uAPICallInvalid)
		{
			return false;
		}
		AddDependencyResult.Set(Call, this, &FESteamUGCCallbacks::HandleAddDependencyResult);
		bAddDependencyBusy = true;
		return true;
	}

	void DrainAddDependencyQueue()
	{
		while (!bAddDependencyBusy)
		{
			FESteamPendingUGCDependency Request;
			if (!AddDependencyQueue.Dequeue(Request))
			{
				break;
			}
			if (!IssueAddDependency(Request))
			{
				if (UESteamUGCSubsystem* Subsystem = Owner.Get())
				{
					Subsystem->OnDependencyAdded.Broadcast(false, static_cast<int64>(Request.Parent), static_cast<int64>(Request.Child));
				}
			}
		}
	}

	// ---- RemoveDependency (serialized) ----

	bool IssueRemoveDependency(const FESteamPendingUGCDependency& Request)
	{
		UESteamUGCSubsystem* Subsystem = Owner.Get();
		if (!Subsystem || !Subsystem->IsSteamAvailable() || !SteamUGC())
		{
			return false;
		}

		const SteamAPICall_t Call = SteamUGC()->RemoveDependency(Request.Parent, Request.Child);
		if (Call == k_uAPICallInvalid)
		{
			return false;
		}
		RemoveDependencyResult.Set(Call, this, &FESteamUGCCallbacks::HandleRemoveDependencyResult);
		bRemoveDependencyBusy = true;
		return true;
	}

	void DrainRemoveDependencyQueue()
	{
		while (!bRemoveDependencyBusy)
		{
			FESteamPendingUGCDependency Request;
			if (!RemoveDependencyQueue.Dequeue(Request))
			{
				break;
			}
			if (!IssueRemoveDependency(Request))
			{
				if (UESteamUGCSubsystem* Subsystem = Owner.Get())
				{
					Subsystem->OnDependencyRemoved.Broadcast(false, static_cast<int64>(Request.Parent), static_cast<int64>(Request.Child));
				}
			}
		}
	}

	// ---- AddAppDependency (serialized) ----

	bool IssueAddAppDependency(const FESteamPendingUGCAppDependency& Request)
	{
		UESteamUGCSubsystem* Subsystem = Owner.Get();
		if (!Subsystem || !Subsystem->IsSteamAvailable() || !SteamUGC())
		{
			return false;
		}

		const SteamAPICall_t Call = SteamUGC()->AddAppDependency(Request.FileId, Request.AppId);
		if (Call == k_uAPICallInvalid)
		{
			return false;
		}
		AddAppDependencyResult.Set(Call, this, &FESteamUGCCallbacks::HandleAddAppDependencyResult);
		bAddAppDependencyBusy = true;
		return true;
	}

	void DrainAddAppDependencyQueue()
	{
		while (!bAddAppDependencyBusy)
		{
			FESteamPendingUGCAppDependency Request;
			if (!AddAppDependencyQueue.Dequeue(Request))
			{
				break;
			}
			if (!IssueAddAppDependency(Request))
			{
				if (UESteamUGCSubsystem* Subsystem = Owner.Get())
				{
					Subsystem->OnAppDependencyAdded.Broadcast(false, static_cast<int64>(Request.FileId), static_cast<int32>(Request.AppId));
				}
			}
		}
	}

	// ---- RemoveAppDependency (serialized) ----

	bool IssueRemoveAppDependency(const FESteamPendingUGCAppDependency& Request)
	{
		UESteamUGCSubsystem* Subsystem = Owner.Get();
		if (!Subsystem || !Subsystem->IsSteamAvailable() || !SteamUGC())
		{
			return false;
		}

		const SteamAPICall_t Call = SteamUGC()->RemoveAppDependency(Request.FileId, Request.AppId);
		if (Call == k_uAPICallInvalid)
		{
			return false;
		}
		RemoveAppDependencyResult.Set(Call, this, &FESteamUGCCallbacks::HandleRemoveAppDependencyResult);
		bRemoveAppDependencyBusy = true;
		return true;
	}

	void DrainRemoveAppDependencyQueue()
	{
		while (!bRemoveAppDependencyBusy)
		{
			FESteamPendingUGCAppDependency Request;
			if (!RemoveAppDependencyQueue.Dequeue(Request))
			{
				break;
			}
			if (!IssueRemoveAppDependency(Request))
			{
				if (UESteamUGCSubsystem* Subsystem = Owner.Get())
				{
					Subsystem->OnAppDependencyRemoved.Broadcast(false, static_cast<int64>(Request.FileId), static_cast<int32>(Request.AppId));
				}
			}
		}
	}

	// ---- GetAppDependencies (serialized) ----

	bool IssueGetAppDependencies(const FESteamPendingUGCFileId& Request)
	{
		UESteamUGCSubsystem* Subsystem = Owner.Get();
		if (!Subsystem || !Subsystem->IsSteamAvailable() || !SteamUGC())
		{
			return false;
		}

		const SteamAPICall_t Call = SteamUGC()->GetAppDependencies(Request.FileId);
		if (Call == k_uAPICallInvalid)
		{
			return false;
		}
		GetAppDependenciesResult.Set(Call, this, &FESteamUGCCallbacks::HandleGetAppDependenciesResult);
		bGetAppDependenciesBusy = true;
		return true;
	}

	void DrainGetAppDependenciesQueue()
	{
		while (!bGetAppDependenciesBusy)
		{
			FESteamPendingUGCFileId Request;
			if (!GetAppDependenciesQueue.Dequeue(Request))
			{
				break;
			}
			if (!IssueGetAppDependencies(Request))
			{
				if (UESteamUGCSubsystem* Subsystem = Owner.Get())
				{
					const TArray<int32> EmptyAppIds;
					Subsystem->OnAppDependenciesReceived.Broadcast(false, static_cast<int64>(Request.FileId), EmptyAppIds);
				}
			}
		}
	}

	// ---- StartPlaytimeTracking (serialized) ----

	bool IssueStartPlaytime(const FESteamPendingUGCPlaytime& Request)
	{
		UESteamUGCSubsystem* Subsystem = Owner.Get();
		if (!Subsystem || !Subsystem->IsSteamAvailable() || !SteamUGC() || Request.FileIds.Num() == 0)
		{
			return false;
		}

		TArray<PublishedFileId_t> FileIds = Request.FileIds;
		const SteamAPICall_t Call = SteamUGC()->StartPlaytimeTracking(FileIds.GetData(), static_cast<uint32>(FileIds.Num()));
		if (Call == k_uAPICallInvalid)
		{
			return false;
		}
		StartPlaytimeResult.Set(Call, this, &FESteamUGCCallbacks::HandleStartPlaytimeResult);
		bStartPlaytimeBusy = true;
		return true;
	}

	void DrainStartPlaytimeQueue()
	{
		while (!bStartPlaytimeBusy)
		{
			FESteamPendingUGCPlaytime Request;
			if (!StartPlaytimeQueue.Dequeue(Request))
			{
				break;
			}
			if (!IssueStartPlaytime(Request))
			{
				if (UESteamUGCSubsystem* Subsystem = Owner.Get())
				{
					Subsystem->OnPlaytimeTrackingStarted.Broadcast(false);
				}
			}
		}
	}

	// ---- StopPlaytimeTracking / StopPlaytimeTrackingForAllItems (serialized, shared queue) ----

	bool IssueStopPlaytime(const FESteamPendingUGCPlaytimeStop& Request)
	{
		UESteamUGCSubsystem* Subsystem = Owner.Get();
		if (!Subsystem || !Subsystem->IsSteamAvailable() || !SteamUGC())
		{
			return false;
		}

		SteamAPICall_t Call = k_uAPICallInvalid;
		if (Request.bAllItems)
		{
			Call = SteamUGC()->StopPlaytimeTrackingForAllItems();
		}
		else
		{
			if (Request.FileIds.Num() == 0)
			{
				return false;
			}
			TArray<PublishedFileId_t> FileIds = Request.FileIds;
			Call = SteamUGC()->StopPlaytimeTracking(FileIds.GetData(), static_cast<uint32>(FileIds.Num()));
		}
		if (Call == k_uAPICallInvalid)
		{
			return false;
		}
		StopPlaytimeResult.Set(Call, this, &FESteamUGCCallbacks::HandleStopPlaytimeResult);
		bStopPlaytimeBusy = true;
		return true;
	}

	void DrainStopPlaytimeQueue()
	{
		while (!bStopPlaytimeBusy)
		{
			FESteamPendingUGCPlaytimeStop Request;
			if (!StopPlaytimeQueue.Dequeue(Request))
			{
				break;
			}
			if (!IssueStopPlaytime(Request))
			{
				if (UESteamUGCSubsystem* Subsystem = Owner.Get())
				{
					Subsystem->OnPlaytimeTrackingStopped.Broadcast(false);
				}
			}
		}
	}

#if ESTEAM_SDK_AT_LEAST(153)
	// ---- GetWorkshopEULAStatus (serialized) ----

	bool IssueEula(const FESteamPendingUGCEula& /*Request*/)
	{
		UESteamUGCSubsystem* Subsystem = Owner.Get();
		if (!Subsystem || !Subsystem->IsSteamAvailable() || !SteamUGC())
		{
			return false;
		}

		const SteamAPICall_t Call = SteamUGC()->GetWorkshopEULAStatus();
		if (Call == k_uAPICallInvalid)
		{
			return false;
		}
		EulaResult.Set(Call, this, &FESteamUGCCallbacks::HandleEulaResult);
		bEulaBusy = true;
		return true;
	}

	void DrainEulaQueue()
	{
		while (!bEulaBusy)
		{
			FESteamPendingUGCEula Request;
			if (!EulaQueue.Dequeue(Request))
			{
				break;
			}
			if (!IssueEula(Request))
			{
				if (UESteamUGCSubsystem* Subsystem = Owner.Get())
				{
					Subsystem->OnWorkshopEULAStatus.Broadcast(false, false, false, 0);
				}
			}
		}
	}
#endif // ESTEAM_SDK_AT_LEAST(153)

	// ---- Completion handlers ----

	void HandleCreateItemResult(CreateItemResult_t* Data, bool bIOFailure)
	{
		if (UESteamUGCSubsystem* Subsystem = Owner.Get())
		{
			const bool bSuccess = !bIOFailure && Data->m_eResult == k_EResultOK;
			Subsystem->OnItemCreated.Broadcast(
				bSuccess,
				static_cast<int64>(Data->m_nPublishedFileId),
				Data->m_bUserNeedsToAcceptWorkshopLegalAgreement);
		}
		bCreateItemBusy = false;
		DrainCreateItemQueue();
	}

	void HandleSubmitItemUpdateResult(SubmitItemUpdateResult_t* Data, bool bIOFailure)
	{
		if (UESteamUGCSubsystem* Subsystem = Owner.Get())
		{
			const bool bSuccess = !bIOFailure && Data->m_eResult == k_EResultOK;
			Subsystem->OnItemSubmitted.Broadcast(bSuccess, Data->m_bUserNeedsToAcceptWorkshopLegalAgreement);
		}
		bSubmitBusy = false;
		DrainSubmitQueue();
	}

	void HandleSubscribeResult(RemoteStorageSubscribePublishedFileResult_t* Data, bool bIOFailure)
	{
		if (UESteamUGCSubsystem* Subsystem = Owner.Get())
		{
			const bool bSuccess = !bIOFailure && Data->m_eResult == k_EResultOK;
			Subsystem->OnSubscribed.Broadcast(bSuccess, static_cast<int64>(Data->m_nPublishedFileId));
		}
		bSubscribeBusy = false;
		DrainSubscribeQueue();
	}

	void HandleUnsubscribeResult(RemoteStorageUnsubscribePublishedFileResult_t* Data, bool bIOFailure)
	{
		if (UESteamUGCSubsystem* Subsystem = Owner.Get())
		{
			const bool bSuccess = !bIOFailure && Data->m_eResult == k_EResultOK;
			Subsystem->OnUnsubscribed.Broadcast(bSuccess, static_cast<int64>(Data->m_nPublishedFileId));
		}
		bUnsubscribeBusy = false;
		DrainUnsubscribeQueue();
	}

	void HandleQueryCompleted(SteamUGCQueryCompleted_t* Data, bool bIOFailure)
	{
		UESteamUGCSubsystem* Subsystem = Owner.Get();
		if (!Subsystem)
		{
			// Subsystem gone: still release the query handle (SDK contract), then drain to fail any
			// queued queries (they hold no handles) so nothing is left dangling.
			if (SteamUGC())
			{
				SteamUGC()->ReleaseQueryUGCRequest(Data->m_handle);
			}
			bQueryBusy = false;
			DrainQueryQueue();
			return;
		}

		TArray<FESteamUGCDetails> Results;
		const bool bSuccess = !bIOFailure && Data->m_eResult == k_EResultOK && SteamUGC() != nullptr;
		if (bSuccess)
		{
			Results.Reserve(Data->m_unNumResultsReturned);
			for (uint32 Index = 0; Index < Data->m_unNumResultsReturned; ++Index)
			{
				SteamUGCDetails_t Details;
				if (!SteamUGC()->GetQueryUGCResult(Data->m_handle, Index, &Details))
				{
					continue;
				}

				FESteamUGCDetails& OutDetails = Results.AddDefaulted_GetRef();
				ParseUGCDetails(Data->m_handle, Index, Details, OutDetails);
			}
		}

		if (SteamUGC())
		{
			SteamUGC()->ReleaseQueryUGCRequest(Data->m_handle);
		}

		Subsystem->OnQueryCompleted.Broadcast(bSuccess, Results, static_cast<int32>(Data->m_unTotalMatchingResults));
		bQueryBusy = false;
		DrainQueryQueue();
	}

	void HandleFavoriteResult(UserFavoriteItemsListChanged_t* Data, bool bIOFailure)
	{
		if (UESteamUGCSubsystem* Subsystem = Owner.Get())
		{
			const bool bSuccess = !bIOFailure && Data->m_eResult == k_EResultOK;
			Subsystem->OnItemFavoriteChanged.Broadcast(
				bSuccess, static_cast<int64>(Data->m_nPublishedFileId), Data->m_bWasAddRequest);
		}
		bFavoriteBusy = false;
		DrainFavoriteQueue();
	}

	void HandleVoteSetResult(SetUserItemVoteResult_t* Data, bool bIOFailure)
	{
		if (UESteamUGCSubsystem* Subsystem = Owner.Get())
		{
			const bool bSuccess = !bIOFailure && Data->m_eResult == k_EResultOK;
			Subsystem->OnUserItemVoteSet.Broadcast(bSuccess, static_cast<int64>(Data->m_nPublishedFileId), Data->m_bVoteUp);
		}
		bVoteSetBusy = false;
		DrainVoteSetQueue();
	}

	void HandleVoteGetResult(GetUserItemVoteResult_t* Data, bool bIOFailure)
	{
		if (UESteamUGCSubsystem* Subsystem = Owner.Get())
		{
			const bool bSuccess = !bIOFailure && Data->m_eResult == k_EResultOK;
			Subsystem->OnUserItemVote.Broadcast(
				bSuccess, static_cast<int64>(Data->m_nPublishedFileId), Data->m_bVotedUp, Data->m_bVotedDown, Data->m_bVoteSkipped);
		}
		bVoteGetBusy = false;
		DrainVoteGetQueue();
	}

	void HandleDeleteResult(DeleteItemResult_t* Data, bool bIOFailure)
	{
		if (UESteamUGCSubsystem* Subsystem = Owner.Get())
		{
			const bool bSuccess = !bIOFailure && Data->m_eResult == k_EResultOK;
			Subsystem->OnItemDeleted.Broadcast(bSuccess, static_cast<int64>(Data->m_nPublishedFileId));
		}
		bDeleteBusy = false;
		DrainDeleteQueue();
	}

	void HandleAddDependencyResult(AddUGCDependencyResult_t* Data, bool bIOFailure)
	{
		if (UESteamUGCSubsystem* Subsystem = Owner.Get())
		{
			const bool bSuccess = !bIOFailure && Data->m_eResult == k_EResultOK;
			Subsystem->OnDependencyAdded.Broadcast(
				bSuccess, static_cast<int64>(Data->m_nPublishedFileId), static_cast<int64>(Data->m_nChildPublishedFileId));
		}
		bAddDependencyBusy = false;
		DrainAddDependencyQueue();
	}

	void HandleRemoveDependencyResult(RemoveUGCDependencyResult_t* Data, bool bIOFailure)
	{
		if (UESteamUGCSubsystem* Subsystem = Owner.Get())
		{
			const bool bSuccess = !bIOFailure && Data->m_eResult == k_EResultOK;
			Subsystem->OnDependencyRemoved.Broadcast(
				bSuccess, static_cast<int64>(Data->m_nPublishedFileId), static_cast<int64>(Data->m_nChildPublishedFileId));
		}
		bRemoveDependencyBusy = false;
		DrainRemoveDependencyQueue();
	}

	void HandleAddAppDependencyResult(AddAppDependencyResult_t* Data, bool bIOFailure)
	{
		if (UESteamUGCSubsystem* Subsystem = Owner.Get())
		{
			const bool bSuccess = !bIOFailure && Data->m_eResult == k_EResultOK;
			Subsystem->OnAppDependencyAdded.Broadcast(
				bSuccess, static_cast<int64>(Data->m_nPublishedFileId), static_cast<int32>(Data->m_nAppID));
		}
		bAddAppDependencyBusy = false;
		DrainAddAppDependencyQueue();
	}

	void HandleRemoveAppDependencyResult(RemoveAppDependencyResult_t* Data, bool bIOFailure)
	{
		if (UESteamUGCSubsystem* Subsystem = Owner.Get())
		{
			const bool bSuccess = !bIOFailure && Data->m_eResult == k_EResultOK;
			Subsystem->OnAppDependencyRemoved.Broadcast(
				bSuccess, static_cast<int64>(Data->m_nPublishedFileId), static_cast<int32>(Data->m_nAppID));
		}
		bRemoveAppDependencyBusy = false;
		DrainRemoveAppDependencyQueue();
	}

	void HandleGetAppDependenciesResult(GetAppDependenciesResult_t* Data, bool bIOFailure)
	{
		if (UESteamUGCSubsystem* Subsystem = Owner.Get())
		{
			const bool bSuccess = !bIOFailure && Data->m_eResult == k_EResultOK;
			TArray<int32> AppIds;
			if (bSuccess)
			{
				const uint32 NumInStruct = FMath::Min<uint32>(Data->m_nNumAppDependencies, UE_ARRAY_COUNT(Data->m_rgAppIDs));
				AppIds.Reserve(NumInStruct);
				for (uint32 AppIndex = 0; AppIndex < NumInStruct; ++AppIndex)
				{
					AppIds.Add(static_cast<int32>(Data->m_rgAppIDs[AppIndex]));
				}
			}
			Subsystem->OnAppDependenciesReceived.Broadcast(bSuccess, static_cast<int64>(Data->m_nPublishedFileId), AppIds);
		}
		bGetAppDependenciesBusy = false;
		DrainGetAppDependenciesQueue();
	}

	void HandleStartPlaytimeResult(StartPlaytimeTrackingResult_t* Data, bool bIOFailure)
	{
		if (UESteamUGCSubsystem* Subsystem = Owner.Get())
		{
			const bool bSuccess = !bIOFailure && Data->m_eResult == k_EResultOK;
			Subsystem->OnPlaytimeTrackingStarted.Broadcast(bSuccess);
		}
		bStartPlaytimeBusy = false;
		DrainStartPlaytimeQueue();
	}

	void HandleStopPlaytimeResult(StopPlaytimeTrackingResult_t* Data, bool bIOFailure)
	{
		if (UESteamUGCSubsystem* Subsystem = Owner.Get())
		{
			const bool bSuccess = !bIOFailure && Data->m_eResult == k_EResultOK;
			Subsystem->OnPlaytimeTrackingStopped.Broadcast(bSuccess);
		}
		bStopPlaytimeBusy = false;
		DrainStopPlaytimeQueue();
	}

#if ESTEAM_SDK_AT_LEAST(153)
	void HandleEulaResult(WorkshopEULAStatus_t* Data, bool bIOFailure)
	{
		if (UESteamUGCSubsystem* Subsystem = Owner.Get())
		{
			const bool bSuccess = !bIOFailure && Data->m_eResult == k_EResultOK;
			Subsystem->OnWorkshopEULAStatus.Broadcast(
				bSuccess, Data->m_bAccepted, Data->m_bNeedsAction, static_cast<int32>(Data->m_unVersion));
		}
		bEulaBusy = false;
		DrainEulaQueue();
	}
#endif // ESTEAM_SDK_AT_LEAST(153)

	void HandleDownloadItemResult(DownloadItemResult_t* Data)
	{
		// This callback fires for any app the local Steam client downloads Workshop content for.
		if (!IsForThisApp(Data->m_unAppID))
		{
			return;
		}
		if (UESteamUGCSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnItemDownloaded.Broadcast(
				Data->m_eResult == k_EResultOK, static_cast<int64>(Data->m_nPublishedFileId));
		}
	}

	void HandleItemInstalled(ItemInstalled_t* Data)
	{
		if (!IsForThisApp(Data->m_unAppID))
		{
			return;
		}
		if (UESteamUGCSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnItemInstalled.Broadcast(static_cast<int64>(Data->m_nPublishedFileId));
		}
	}

#if ESTEAM_SDK_AT_LEAST(151)
	void HandleSubscribedItemsChanged(UserSubscribedItemsListChanged_t* Data)
	{
		if (!IsForThisApp(Data->m_nAppID))
		{
			return;
		}
		if (UESteamUGCSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnSubscribedItemsChanged.Broadcast(static_cast<int32>(Data->m_nAppID));
		}
	}
#endif // ESTEAM_SDK_AT_LEAST(151)

	TWeakObjectPtr<UESteamUGCSubsystem> Owner;
	CCallback<FESteamUGCCallbacks, DownloadItemResult_t> DownloadItemCallback;
	CCallback<FESteamUGCCallbacks, ItemInstalled_t> ItemInstalledCallback;
#if ESTEAM_SDK_AT_LEAST(151)
	CCallback<FESteamUGCCallbacks, UserSubscribedItemsListChanged_t> SubscribedItemsChangedCallback;
#endif

	CCallResult<FESteamUGCCallbacks, CreateItemResult_t> CreateItemResult;
	CCallResult<FESteamUGCCallbacks, SubmitItemUpdateResult_t> SubmitItemUpdateResult;
	CCallResult<FESteamUGCCallbacks, RemoteStorageSubscribePublishedFileResult_t> SubscribeResult;
	CCallResult<FESteamUGCCallbacks, RemoteStorageUnsubscribePublishedFileResult_t> UnsubscribeResult;
	CCallResult<FESteamUGCCallbacks, SteamUGCQueryCompleted_t> QueryCompletedResult;
	CCallResult<FESteamUGCCallbacks, UserFavoriteItemsListChanged_t> FavoriteResult;
	CCallResult<FESteamUGCCallbacks, SetUserItemVoteResult_t> VoteSetResult;
	CCallResult<FESteamUGCCallbacks, GetUserItemVoteResult_t> VoteGetResult;
	CCallResult<FESteamUGCCallbacks, DeleteItemResult_t> DeleteResult;
	CCallResult<FESteamUGCCallbacks, AddUGCDependencyResult_t> AddDependencyResult;
	CCallResult<FESteamUGCCallbacks, RemoveUGCDependencyResult_t> RemoveDependencyResult;
	CCallResult<FESteamUGCCallbacks, AddAppDependencyResult_t> AddAppDependencyResult;
	CCallResult<FESteamUGCCallbacks, RemoveAppDependencyResult_t> RemoveAppDependencyResult;
	CCallResult<FESteamUGCCallbacks, GetAppDependenciesResult_t> GetAppDependenciesResult;
	CCallResult<FESteamUGCCallbacks, StartPlaytimeTrackingResult_t> StartPlaytimeResult;
	CCallResult<FESteamUGCCallbacks, StopPlaytimeTrackingResult_t> StopPlaytimeResult;
#if ESTEAM_SDK_AT_LEAST(153)
	CCallResult<FESteamUGCCallbacks, WorkshopEULAStatus_t> EulaResult;
#endif

	// In-flight flags + FIFO queues, one per serialized operation type (shared where a result struct is shared).
	bool bCreateItemBusy = false;
	bool bSubmitBusy = false;
	bool bSubscribeBusy = false;
	bool bUnsubscribeBusy = false;
	bool bQueryBusy = false;
	bool bFavoriteBusy = false;
	bool bVoteSetBusy = false;
	bool bVoteGetBusy = false;
	bool bDeleteBusy = false;
	bool bAddDependencyBusy = false;
	bool bRemoveDependencyBusy = false;
	bool bAddAppDependencyBusy = false;
	bool bRemoveAppDependencyBusy = false;
	bool bGetAppDependenciesBusy = false;
	bool bStartPlaytimeBusy = false;
	bool bStopPlaytimeBusy = false;
#if ESTEAM_SDK_AT_LEAST(153)
	bool bEulaBusy = false;
#endif

	TQueue<FESteamPendingUGCCreate> CreateItemQueue;
	TQueue<FESteamPendingUGCSubmit> SubmitQueue;
	TQueue<FESteamPendingUGCSubscription> SubscribeQueue;
	TQueue<FESteamPendingUGCSubscription> UnsubscribeQueue;
	TQueue<FESteamPendingUGCQuery> QueryQueue;
	TQueue<FESteamPendingUGCFavorite> FavoriteQueue;
	TQueue<FESteamPendingUGCVote> VoteSetQueue;
	TQueue<FESteamPendingUGCFileId> VoteGetQueue;
	TQueue<FESteamPendingUGCFileId> DeleteQueue;
	TQueue<FESteamPendingUGCDependency> AddDependencyQueue;
	TQueue<FESteamPendingUGCDependency> RemoveDependencyQueue;
	TQueue<FESteamPendingUGCAppDependency> AddAppDependencyQueue;
	TQueue<FESteamPendingUGCAppDependency> RemoveAppDependencyQueue;
	TQueue<FESteamPendingUGCFileId> GetAppDependenciesQueue;
	TQueue<FESteamPendingUGCPlaytime> StartPlaytimeQueue;
	TQueue<FESteamPendingUGCPlaytimeStop> StopPlaytimeQueue;
#if ESTEAM_SDK_AT_LEAST(153)
	TQueue<FESteamPendingUGCEula> EulaQueue;
#endif
};
#else
class FESteamUGCCallbacks
{
};
#endif // WITH_EXTENDEDSTEAM_SDK

void UESteamUGCSubsystem::Deinitialize()
{
	Super::Deinitialize();
	Callbacks.Reset();
}

void UESteamUGCSubsystem::HandleSteamClientInitialized()
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!Callbacks)
	{
		Callbacks = MakeShared<FESteamUGCCallbacks>(this);
	}
#endif
}

void UESteamUGCSubsystem::HandleSteamClientShutdown()
{
	Callbacks.Reset();
}

bool UESteamUGCSubsystem::CreateItem(EESteamWorkshopFileType FileType)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUGC() || !SteamUtils() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("CreateItem"));
		return false;
	}

	// Issued now if idle, otherwise queued behind the in-flight create (never dropped).
	FESteamPendingUGCCreate Request;
	Request.FileType = ToSteamWorkshopFileType(FileType);
	return Callbacks->EnqueueCreateItem(Request);
#else
	return false;
#endif
}

int64 UESteamUGCSubsystem::StartItemUpdate(int64 PublishedFileId)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUGC() || !SteamUtils())
	{
		LogSteamUnavailable(TEXT("StartItemUpdate"));
		return 0;
	}
	if (PublishedFileId == 0)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("StartItemUpdate: invalid published file id"));
		return 0;
	}

	const UGCUpdateHandle_t Handle = SteamUGC()->StartItemUpdate(
		SteamUtils()->GetAppID(), static_cast<PublishedFileId_t>(PublishedFileId));
	if (Handle == k_UGCUpdateHandleInvalid)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("StartItemUpdate failed for item %lld"), PublishedFileId);
		return 0;
	}
	return static_cast<int64>(Handle);
#else
	return 0;
#endif
}

bool UESteamUGCSubsystem::SetItemTitle(int64 UpdateHandle, const FString& Title)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUGC() || UpdateHandle == 0)
	{
		return false;
	}
	return SteamUGC()->SetItemTitle(static_cast<UGCUpdateHandle_t>(UpdateHandle), TCHAR_TO_UTF8(*Title));
#else
	return false;
#endif
}

bool UESteamUGCSubsystem::SetItemDescription(int64 UpdateHandle, const FString& Description)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUGC() || UpdateHandle == 0)
	{
		return false;
	}
	return SteamUGC()->SetItemDescription(static_cast<UGCUpdateHandle_t>(UpdateHandle), TCHAR_TO_UTF8(*Description));
#else
	return false;
#endif
}

bool UESteamUGCSubsystem::SetItemVisibility(int64 UpdateHandle, EESteamUGCVisibility Visibility)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUGC() || UpdateHandle == 0)
	{
		return false;
	}
	return SteamUGC()->SetItemVisibility(static_cast<UGCUpdateHandle_t>(UpdateHandle), ToSteamVisibility(Visibility));
#else
	return false;
#endif
}

bool UESteamUGCSubsystem::SetItemTags(int64 UpdateHandle, const TArray<FString>& Tags)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUGC() || UpdateHandle == 0)
	{
		return false;
	}

	// SteamParamStringArray_t needs stable UTF-8 buffers for the duration of the call.
	TArray<TArray<ANSICHAR>> Utf8Tags;
	Utf8Tags.Reserve(Tags.Num());
	TArray<const char*> TagPointers;
	TagPointers.Reserve(Tags.Num());
	for (const FString& Tag : Tags)
	{
		FTCHARToUTF8 Converter(*Tag);
		TArray<ANSICHAR>& Buffer = Utf8Tags.AddDefaulted_GetRef();
		Buffer.Append(reinterpret_cast<const ANSICHAR*>(Converter.Get()), Converter.Length());
		Buffer.Add('\0');
	}
	for (const TArray<ANSICHAR>& Buffer : Utf8Tags)
	{
		TagPointers.Add(Buffer.GetData());
	}

	SteamParamStringArray_t TagArray;
	TagArray.m_ppStrings = TagPointers.GetData();
	TagArray.m_nNumStrings = TagPointers.Num();
	return SteamUGC()->SetItemTags(static_cast<UGCUpdateHandle_t>(UpdateHandle), &TagArray);
#else
	return false;
#endif
}

bool UESteamUGCSubsystem::SetItemContent(int64 UpdateHandle, const FString& AbsoluteFolder)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUGC() || UpdateHandle == 0)
	{
		return false;
	}
	return SteamUGC()->SetItemContent(static_cast<UGCUpdateHandle_t>(UpdateHandle), TCHAR_TO_UTF8(*AbsoluteFolder));
#else
	return false;
#endif
}

bool UESteamUGCSubsystem::SetItemPreview(int64 UpdateHandle, const FString& AbsoluteImagePath)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUGC() || UpdateHandle == 0)
	{
		return false;
	}
	return SteamUGC()->SetItemPreview(static_cast<UGCUpdateHandle_t>(UpdateHandle), TCHAR_TO_UTF8(*AbsoluteImagePath));
#else
	return false;
#endif
}

bool UESteamUGCSubsystem::SetItemMetadata(int64 UpdateHandle, const FString& Metadata)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUGC() || UpdateHandle == 0)
	{
		return false;
	}
	return SteamUGC()->SetItemMetadata(static_cast<UGCUpdateHandle_t>(UpdateHandle), TCHAR_TO_UTF8(*Metadata));
#else
	return false;
#endif
}

bool UESteamUGCSubsystem::SetItemUpdateLanguage(int64 UpdateHandle, const FString& Language)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUGC() || UpdateHandle == 0)
	{
		return false;
	}
	return SteamUGC()->SetItemUpdateLanguage(static_cast<UGCUpdateHandle_t>(UpdateHandle), TCHAR_TO_UTF8(*Language));
#else
	return false;
#endif
}

bool UESteamUGCSubsystem::AddItemKeyValueTag(int64 UpdateHandle, const FString& Key, const FString& Value)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUGC() || UpdateHandle == 0)
	{
		return false;
	}
	return SteamUGC()->AddItemKeyValueTag(static_cast<UGCUpdateHandle_t>(UpdateHandle), TCHAR_TO_UTF8(*Key), TCHAR_TO_UTF8(*Value));
#else
	return false;
#endif
}

bool UESteamUGCSubsystem::RemoveItemKeyValueTags(int64 UpdateHandle, const FString& Key)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUGC() || UpdateHandle == 0)
	{
		return false;
	}
	return SteamUGC()->RemoveItemKeyValueTags(static_cast<UGCUpdateHandle_t>(UpdateHandle), TCHAR_TO_UTF8(*Key));
#else
	return false;
#endif
}

bool UESteamUGCSubsystem::RemoveAllItemKeyValueTags(int64 UpdateHandle)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUGC() || UpdateHandle == 0)
	{
		return false;
	}
	return SteamUGC()->RemoveAllItemKeyValueTags(static_cast<UGCUpdateHandle_t>(UpdateHandle));
#else
	return false;
#endif
}

bool UESteamUGCSubsystem::AddItemPreviewFile(int64 UpdateHandle, const FString& AbsolutePreviewFile, EESteamItemPreviewType PreviewType)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUGC() || UpdateHandle == 0)
	{
		return false;
	}
	return SteamUGC()->AddItemPreviewFile(
		static_cast<UGCUpdateHandle_t>(UpdateHandle), TCHAR_TO_UTF8(*AbsolutePreviewFile), ToSteamItemPreviewType(PreviewType));
#else
	return false;
#endif
}

bool UESteamUGCSubsystem::AddItemPreviewVideo(int64 UpdateHandle, const FString& VideoId)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUGC() || UpdateHandle == 0)
	{
		return false;
	}
	return SteamUGC()->AddItemPreviewVideo(static_cast<UGCUpdateHandle_t>(UpdateHandle), TCHAR_TO_UTF8(*VideoId));
#else
	return false;
#endif
}

bool UESteamUGCSubsystem::UpdateItemPreviewFile(int64 UpdateHandle, int32 Index, const FString& AbsolutePreviewFile)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUGC() || UpdateHandle == 0 || Index < 0)
	{
		return false;
	}
	return SteamUGC()->UpdateItemPreviewFile(
		static_cast<UGCUpdateHandle_t>(UpdateHandle), static_cast<uint32>(Index), TCHAR_TO_UTF8(*AbsolutePreviewFile));
#else
	return false;
#endif
}

bool UESteamUGCSubsystem::UpdateItemPreviewVideo(int64 UpdateHandle, int32 Index, const FString& VideoId)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUGC() || UpdateHandle == 0 || Index < 0)
	{
		return false;
	}
	return SteamUGC()->UpdateItemPreviewVideo(
		static_cast<UGCUpdateHandle_t>(UpdateHandle), static_cast<uint32>(Index), TCHAR_TO_UTF8(*VideoId));
#else
	return false;
#endif
}

bool UESteamUGCSubsystem::RemoveItemPreview(int64 UpdateHandle, int32 Index)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUGC() || UpdateHandle == 0 || Index < 0)
	{
		return false;
	}
	return SteamUGC()->RemoveItemPreview(static_cast<UGCUpdateHandle_t>(UpdateHandle), static_cast<uint32>(Index));
#else
	return false;
#endif
}

bool UESteamUGCSubsystem::SetAllowLegacyUpload(int64 UpdateHandle, bool bAllowLegacyUpload)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUGC() || UpdateHandle == 0)
	{
		return false;
	}
	return SteamUGC()->SetAllowLegacyUpload(static_cast<UGCUpdateHandle_t>(UpdateHandle), bAllowLegacyUpload);
#else
	return false;
#endif
}

bool UESteamUGCSubsystem::SubmitItemUpdate(int64 UpdateHandle, const FString& ChangeNote)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUGC() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("SubmitItemUpdate"));
		return false;
	}
	if (UpdateHandle == 0)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("SubmitItemUpdate: invalid update handle"));
		return false;
	}

	// Issued now if idle, otherwise queued behind the in-flight submit (never dropped). The update
	// handle and change note are captured so a queued submit re-issues the same call.
	FESteamPendingUGCSubmit Request;
	Request.Handle = static_cast<UGCUpdateHandle_t>(UpdateHandle);
	Request.ChangeNote = ChangeNote;
	return Callbacks->EnqueueSubmitItemUpdate(Request);
#else
	return false;
#endif
}

EESteamItemUpdateStatus UESteamUGCSubsystem::GetItemUpdateProgress(int64 UpdateHandle, int64& BytesProcessed, int64& BytesTotal) const
{
	BytesProcessed = 0;
	BytesTotal = 0;
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamUGC() && UpdateHandle != 0)
	{
		uint64 Processed = 0;
		uint64 Total = 0;
		const EItemUpdateStatus Status = SteamUGC()->GetItemUpdateProgress(
			static_cast<UGCUpdateHandle_t>(UpdateHandle), &Processed, &Total);
		BytesProcessed = static_cast<int64>(Processed);
		BytesTotal = static_cast<int64>(Total);

		switch (Status)
		{
		case k_EItemUpdateStatusPreparingConfig:       return EESteamItemUpdateStatus::PreparingConfig;
		case k_EItemUpdateStatusPreparingContent:      return EESteamItemUpdateStatus::PreparingContent;
		case k_EItemUpdateStatusUploadingContent:      return EESteamItemUpdateStatus::UploadingContent;
		case k_EItemUpdateStatusUploadingPreviewFile:  return EESteamItemUpdateStatus::UploadingPreviewFile;
		case k_EItemUpdateStatusCommittingChanges:     return EESteamItemUpdateStatus::CommittingChanges;
		default:                                       return EESteamItemUpdateStatus::Invalid;
		}
	}
#endif
	return EESteamItemUpdateStatus::Invalid;
}

bool UESteamUGCSubsystem::SubscribeItem(int64 PublishedFileId)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUGC() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("SubscribeItem"));
		return false;
	}
	if (PublishedFileId == 0)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("SubscribeItem: invalid published file id"));
		return false;
	}

	// Issued now if idle, otherwise queued behind the in-flight subscribe (never dropped) — a
	// realistic burst when subscribing to many items in a loop.
	FESteamPendingUGCSubscription Request;
	Request.FileId = static_cast<PublishedFileId_t>(PublishedFileId);
	return Callbacks->EnqueueSubscribe(Request);
#else
	return false;
#endif
}

bool UESteamUGCSubsystem::UnsubscribeItem(int64 PublishedFileId)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUGC() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("UnsubscribeItem"));
		return false;
	}
	if (PublishedFileId == 0)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("UnsubscribeItem: invalid published file id"));
		return false;
	}

	// Issued now if idle, otherwise queued behind the in-flight unsubscribe (never dropped) — a
	// realistic burst when unsubscribing from many items in a loop.
	FESteamPendingUGCSubscription Request;
	Request.FileId = static_cast<PublishedFileId_t>(PublishedFileId);
	return Callbacks->EnqueueUnsubscribe(Request);
#else
	return false;
#endif
}

int32 UESteamUGCSubsystem::GetNumSubscribedItems(bool bIncludeLocallyDisabled) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamUGC())
	{
#if ESTEAM_SDK_AT_LEAST(151)
		return static_cast<int32>(SteamUGC()->GetNumSubscribedItems(bIncludeLocallyDisabled));
#else
		(void)bIncludeLocallyDisabled;
		return static_cast<int32>(SteamUGC()->GetNumSubscribedItems());
#endif
	}
#else
	(void)bIncludeLocallyDisabled;
#endif
	return 0;
}

void UESteamUGCSubsystem::GetSubscribedItems(TArray<int64>& OutPublishedFileIds, bool bIncludeLocallyDisabled) const
{
	OutPublishedFileIds.Reset();
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUGC())
	{
		return;
	}

#if ESTEAM_SDK_AT_LEAST(151)
	const uint32 NumSubscribed = SteamUGC()->GetNumSubscribedItems(bIncludeLocallyDisabled);
#else
	(void)bIncludeLocallyDisabled;
	const uint32 NumSubscribed = SteamUGC()->GetNumSubscribedItems();
#endif
	if (NumSubscribed == 0)
	{
		return;
	}

	TArray<PublishedFileId_t> FileIds;
	FileIds.SetNumUninitialized(NumSubscribed);
#if ESTEAM_SDK_AT_LEAST(151)
	const uint32 NumReturned = SteamUGC()->GetSubscribedItems(FileIds.GetData(), NumSubscribed, bIncludeLocallyDisabled);
#else
	const uint32 NumReturned = SteamUGC()->GetSubscribedItems(FileIds.GetData(), NumSubscribed);
#endif

	OutPublishedFileIds.Reserve(NumReturned);
	for (uint32 Index = 0; Index < NumReturned; ++Index)
	{
		OutPublishedFileIds.Add(static_cast<int64>(FileIds[Index]));
	}
#else
	(void)bIncludeLocallyDisabled;
#endif
}

FESteamUGCItemState UESteamUGCSubsystem::GetItemState(int64 PublishedFileId) const
{
	FESteamUGCItemState State;
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamUGC() && PublishedFileId != 0)
	{
		const uint32 Flags = SteamUGC()->GetItemState(static_cast<PublishedFileId_t>(PublishedFileId));
		State.bSubscribed = (Flags & k_EItemStateSubscribed) != 0;
		State.bInstalled = (Flags & k_EItemStateInstalled) != 0;
		State.bNeedsUpdate = (Flags & k_EItemStateNeedsUpdate) != 0;
		State.bDownloading = (Flags & k_EItemStateDownloading) != 0;
		State.bDownloadPending = (Flags & k_EItemStateDownloadPending) != 0;
#if ESTEAM_SDK_AT_LEAST(151)
		// k_EItemStateDisabledLocally only exists in Steamworks 1.51+.
		State.bDisabledLocally = (Flags & k_EItemStateDisabledLocally) != 0;
#endif
	}
#endif
	return State;
}

bool UESteamUGCSubsystem::GetItemInstallInfo(int64 PublishedFileId, FString& OutFolder, int64& OutSizeOnDisk, int64& OutTimestamp) const
{
	OutFolder.Reset();
	OutSizeOnDisk = 0;
	OutTimestamp = 0;
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamUGC() && PublishedFileId != 0)
	{
		uint64 SizeOnDisk = 0;
		uint32 Timestamp = 0;
		char FolderBuffer[1024] = {};
		if (SteamUGC()->GetItemInstallInfo(
			static_cast<PublishedFileId_t>(PublishedFileId), &SizeOnDisk, FolderBuffer, sizeof(FolderBuffer), &Timestamp))
		{
			OutFolder = FString(UTF8_TO_TCHAR(FolderBuffer));
			OutSizeOnDisk = static_cast<int64>(SizeOnDisk);
			OutTimestamp = static_cast<int64>(Timestamp);
			return true;
		}
	}
#endif
	return false;
}

bool UESteamUGCSubsystem::GetItemDownloadInfo(int64 PublishedFileId, int64& BytesDownloaded, int64& BytesTotal) const
{
	BytesDownloaded = 0;
	BytesTotal = 0;
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamUGC() && PublishedFileId != 0)
	{
		uint64 Downloaded = 0;
		uint64 Total = 0;
		if (SteamUGC()->GetItemDownloadInfo(static_cast<PublishedFileId_t>(PublishedFileId), &Downloaded, &Total))
		{
			BytesDownloaded = static_cast<int64>(Downloaded);
			BytesTotal = static_cast<int64>(Total);
			return true;
		}
	}
#endif
	return false;
}

bool UESteamUGCSubsystem::DownloadItem(int64 PublishedFileId, bool bHighPriority)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUGC())
	{
		LogSteamUnavailable(TEXT("DownloadItem"));
		return false;
	}
	if (PublishedFileId == 0)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("DownloadItem: invalid published file id"));
		return false;
	}
	return SteamUGC()->DownloadItem(static_cast<PublishedFileId_t>(PublishedFileId), bHighPriority);
#else
	return false;
#endif
}

void UESteamUGCSubsystem::SuspendDownloads(bool bSuspend)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamUGC())
	{
		SteamUGC()->SuspendDownloads(bSuspend);
	}
#endif
}

bool UESteamUGCSubsystem::QueryAllItems(EESteamUGCQueryType QueryType, EESteamUGCMatchingType MatchingType, int32 Page)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUGC() || !SteamUtils() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("QueryAllItems"));
		return false;
	}

	// Issued now if idle, otherwise queued behind the in-flight query (never dropped). The query
	// handle (create + set-long-description + send, with release-on-failure) is built when the
	// query is actually issued; on completion HandleQueryCompleted releases it.
	FESteamPendingUGCQuery Request;
	Request.Kind = EESteamUGCQueryKind::AllItems;
	Request.QueryType = ToSteamUGCQuery(QueryType);
	Request.MatchingType = ToSteamMatchingType(MatchingType);
	Request.Page = static_cast<uint32>(FMath::Max(Page, 1));
	return Callbacks->EnqueueQuery(Request);
#else
	return false;
#endif
}

bool UESteamUGCSubsystem::QueryAllItemsAdvanced(EESteamUGCQueryType QueryType, EESteamUGCMatchingType MatchingType, int32 Page, const FESteamUGCQueryConfig& Config)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUGC() || !SteamUtils() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("QueryAllItemsAdvanced"));
		return false;
	}

	FESteamPendingUGCQuery Request;
	Request.Kind = EESteamUGCQueryKind::AllItems;
	Request.QueryType = ToSteamUGCQuery(QueryType);
	Request.MatchingType = ToSteamMatchingType(MatchingType);
	Request.Page = static_cast<uint32>(FMath::Max(Page, 1));
	Request.bHasConfig = true;
	Request.Config = Config;
	return Callbacks->EnqueueQuery(Request);
#else
	return false;
#endif
}

bool UESteamUGCSubsystem::QueryItemsByIds(const TArray<int64>& PublishedFileIds)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUGC() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("QueryItemsByIds"));
		return false;
	}
	if (PublishedFileIds.Num() == 0)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("QueryItemsByIds: no published file ids given"));
		return false;
	}

	// Issued now if idle, otherwise queued behind the in-flight query (never dropped). The id list
	// is captured so a queued query builds its own CreateQueryUGCDetailsRequest handle when issued;
	// on completion HandleQueryCompleted releases it.
	FESteamPendingUGCQuery Request;
	Request.Kind = EESteamUGCQueryKind::ByIds;
	Request.FileIds.Reserve(PublishedFileIds.Num());
	for (const int64 PublishedFileId : PublishedFileIds)
	{
		Request.FileIds.Add(static_cast<PublishedFileId_t>(PublishedFileId));
	}
	return Callbacks->EnqueueQuery(Request);
#else
	return false;
#endif
}

bool UESteamUGCSubsystem::QueryUserItems(FESteamId User, EESteamUserUGCList ListType, EESteamUserUGCListSortOrder SortOrder, EESteamUGCMatchingType MatchingType, int32 Page, const FESteamUGCQueryConfig& Config)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUGC() || !SteamUtils() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("QueryUserItems"));
		return false;
	}

	FESteamPendingUGCQuery Request;
	Request.Kind = EESteamUGCQueryKind::UserItems;
	const CSteamID SteamId(User.Value);
	Request.AccountId = SteamId.GetAccountID();
	Request.UserList = ToSteamUserUGCList(ListType);
	Request.SortOrder = ToSteamUserUGCListSortOrder(SortOrder);
	Request.MatchingType = ToSteamMatchingType(MatchingType);
	Request.Page = static_cast<uint32>(FMath::Max(Page, 1));
	Request.bHasConfig = true;
	Request.Config = Config;
	return Callbacks->EnqueueQuery(Request);
#else
	return false;
#endif
}

bool UESteamUGCSubsystem::AddItemToFavorites(int32 AppId, int64 PublishedFileId)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUGC() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("AddItemToFavorites"));
		return false;
	}
	if (PublishedFileId == 0)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("AddItemToFavorites: invalid published file id"));
		return false;
	}

	FESteamPendingUGCFavorite Request;
	Request.AppId = AppId > 0 ? static_cast<AppId_t>(AppId) : (SteamUtils() ? SteamUtils()->GetAppID() : 0);
	Request.FileId = static_cast<PublishedFileId_t>(PublishedFileId);
	Request.bAdd = true;
	return Callbacks->EnqueueFavorite(Request);
#else
	return false;
#endif
}

bool UESteamUGCSubsystem::RemoveItemFromFavorites(int32 AppId, int64 PublishedFileId)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUGC() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("RemoveItemFromFavorites"));
		return false;
	}
	if (PublishedFileId == 0)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("RemoveItemFromFavorites: invalid published file id"));
		return false;
	}

	FESteamPendingUGCFavorite Request;
	Request.AppId = AppId > 0 ? static_cast<AppId_t>(AppId) : (SteamUtils() ? SteamUtils()->GetAppID() : 0);
	Request.FileId = static_cast<PublishedFileId_t>(PublishedFileId);
	Request.bAdd = false;
	return Callbacks->EnqueueFavorite(Request);
#else
	return false;
#endif
}

bool UESteamUGCSubsystem::SetUserItemVote(int64 PublishedFileId, bool bVoteUp)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUGC() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("SetUserItemVote"));
		return false;
	}
	if (PublishedFileId == 0)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("SetUserItemVote: invalid published file id"));
		return false;
	}

	FESteamPendingUGCVote Request;
	Request.FileId = static_cast<PublishedFileId_t>(PublishedFileId);
	Request.bVoteUp = bVoteUp;
	return Callbacks->EnqueueVoteSet(Request);
#else
	return false;
#endif
}

bool UESteamUGCSubsystem::GetUserItemVote(int64 PublishedFileId)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUGC() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("GetUserItemVote"));
		return false;
	}
	if (PublishedFileId == 0)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("GetUserItemVote: invalid published file id"));
		return false;
	}

	FESteamPendingUGCFileId Request;
	Request.FileId = static_cast<PublishedFileId_t>(PublishedFileId);
	return Callbacks->EnqueueVoteGet(Request);
#else
	return false;
#endif
}

bool UESteamUGCSubsystem::DeleteItem(int64 PublishedFileId)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUGC() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("DeleteItem"));
		return false;
	}
	if (PublishedFileId == 0)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("DeleteItem: invalid published file id"));
		return false;
	}

	FESteamPendingUGCFileId Request;
	Request.FileId = static_cast<PublishedFileId_t>(PublishedFileId);
	return Callbacks->EnqueueDelete(Request);
#else
	return false;
#endif
}

bool UESteamUGCSubsystem::AddDependency(int64 ParentPublishedFileId, int64 ChildPublishedFileId)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUGC() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("AddDependency"));
		return false;
	}
	if (ParentPublishedFileId == 0 || ChildPublishedFileId == 0)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("AddDependency: invalid parent/child published file id"));
		return false;
	}

	FESteamPendingUGCDependency Request;
	Request.Parent = static_cast<PublishedFileId_t>(ParentPublishedFileId);
	Request.Child = static_cast<PublishedFileId_t>(ChildPublishedFileId);
	return Callbacks->EnqueueAddDependency(Request);
#else
	return false;
#endif
}

bool UESteamUGCSubsystem::RemoveDependency(int64 ParentPublishedFileId, int64 ChildPublishedFileId)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUGC() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("RemoveDependency"));
		return false;
	}
	if (ParentPublishedFileId == 0 || ChildPublishedFileId == 0)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("RemoveDependency: invalid parent/child published file id"));
		return false;
	}

	FESteamPendingUGCDependency Request;
	Request.Parent = static_cast<PublishedFileId_t>(ParentPublishedFileId);
	Request.Child = static_cast<PublishedFileId_t>(ChildPublishedFileId);
	return Callbacks->EnqueueRemoveDependency(Request);
#else
	return false;
#endif
}

bool UESteamUGCSubsystem::AddAppDependency(int64 PublishedFileId, int32 AppId)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUGC() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("AddAppDependency"));
		return false;
	}
	if (PublishedFileId == 0 || AppId <= 0)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("AddAppDependency: invalid published file id or app id"));
		return false;
	}

	FESteamPendingUGCAppDependency Request;
	Request.FileId = static_cast<PublishedFileId_t>(PublishedFileId);
	Request.AppId = static_cast<AppId_t>(AppId);
	return Callbacks->EnqueueAddAppDependency(Request);
#else
	return false;
#endif
}

bool UESteamUGCSubsystem::RemoveAppDependency(int64 PublishedFileId, int32 AppId)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUGC() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("RemoveAppDependency"));
		return false;
	}
	if (PublishedFileId == 0 || AppId <= 0)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("RemoveAppDependency: invalid published file id or app id"));
		return false;
	}

	FESteamPendingUGCAppDependency Request;
	Request.FileId = static_cast<PublishedFileId_t>(PublishedFileId);
	Request.AppId = static_cast<AppId_t>(AppId);
	return Callbacks->EnqueueRemoveAppDependency(Request);
#else
	return false;
#endif
}

bool UESteamUGCSubsystem::GetAppDependencies(int64 PublishedFileId)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUGC() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("GetAppDependencies"));
		return false;
	}
	if (PublishedFileId == 0)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("GetAppDependencies: invalid published file id"));
		return false;
	}

	FESteamPendingUGCFileId Request;
	Request.FileId = static_cast<PublishedFileId_t>(PublishedFileId);
	return Callbacks->EnqueueGetAppDependencies(Request);
#else
	return false;
#endif
}

bool UESteamUGCSubsystem::StartPlaytimeTracking(const TArray<int64>& PublishedFileIds)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUGC() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("StartPlaytimeTracking"));
		return false;
	}
	if (PublishedFileIds.Num() == 0)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("StartPlaytimeTracking: no published file ids given"));
		return false;
	}

	FESteamPendingUGCPlaytime Request;
	Request.FileIds.Reserve(PublishedFileIds.Num());
	for (const int64 PublishedFileId : PublishedFileIds)
	{
		Request.FileIds.Add(static_cast<PublishedFileId_t>(PublishedFileId));
	}
	return Callbacks->EnqueueStartPlaytime(Request);
#else
	return false;
#endif
}

bool UESteamUGCSubsystem::StopPlaytimeTracking(const TArray<int64>& PublishedFileIds)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUGC() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("StopPlaytimeTracking"));
		return false;
	}
	if (PublishedFileIds.Num() == 0)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("StopPlaytimeTracking: no published file ids given"));
		return false;
	}

	FESteamPendingUGCPlaytimeStop Request;
	Request.bAllItems = false;
	Request.FileIds.Reserve(PublishedFileIds.Num());
	for (const int64 PublishedFileId : PublishedFileIds)
	{
		Request.FileIds.Add(static_cast<PublishedFileId_t>(PublishedFileId));
	}
	return Callbacks->EnqueueStopPlaytime(Request);
#else
	return false;
#endif
}

bool UESteamUGCSubsystem::StopPlaytimeTrackingForAllItems()
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUGC() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("StopPlaytimeTrackingForAllItems"));
		return false;
	}

	FESteamPendingUGCPlaytimeStop Request;
	Request.bAllItems = true;
	return Callbacks->EnqueueStopPlaytime(Request);
#else
	return false;
#endif
}

bool UESteamUGCSubsystem::BInitWorkshopForGameServer(int32 WorkshopDepotId, const FString& Folder)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUGC())
	{
		LogSteamUnavailable(TEXT("BInitWorkshopForGameServer"));
		return false;
	}
	return SteamUGC()->BInitWorkshopForGameServer(
		static_cast<DepotId_t>(WorkshopDepotId), Folder.IsEmpty() ? nullptr : TCHAR_TO_UTF8(*Folder));
#else
	return false;
#endif
}

bool UESteamUGCSubsystem::ShowWorkshopEULA()
{
#if ESTEAM_SDK_AT_LEAST(153)
	if (!IsSteamAvailable() || !SteamUGC())
	{
		LogSteamUnavailable(TEXT("ShowWorkshopEULA"));
		return false;
	}
	return SteamUGC()->ShowWorkshopEULA();
#else
	UE_LOG(LogExtendedSteam, Warning, TEXT("ShowWorkshopEULA requires Steamworks SDK 1.53 or newer"));
	return false;
#endif
}

bool UESteamUGCSubsystem::GetWorkshopEULAStatus()
{
#if ESTEAM_SDK_AT_LEAST(153)
	if (!IsSteamAvailable() || !SteamUGC() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("GetWorkshopEULAStatus"));
		return false;
	}

	FESteamPendingUGCEula Request;
	return Callbacks->EnqueueEula(Request);
#else
	UE_LOG(LogExtendedSteam, Warning, TEXT("GetWorkshopEULAStatus requires Steamworks SDK 1.53 or newer"));
	return false;
#endif
}
