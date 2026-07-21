// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Stats/ESteamUserStatsSubsystem.h"
#include "Shared/ESteamLog.h"
#include "Shared/ESteamSDK.h"
#include "Containers/Queue.h"

#if WITH_EXTENDEDSTEAM_SDK
THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
THIRD_PARTY_INCLUDES_END

namespace
{
	ELeaderboardSortMethod ToSteamSortMethod(EESteamLeaderboardSortMethod SortMethod)
	{
		switch (SortMethod)
		{
		case EESteamLeaderboardSortMethod::Ascending:  return k_ELeaderboardSortMethodAscending;
		case EESteamLeaderboardSortMethod::Descending: return k_ELeaderboardSortMethodDescending;
		default:                                       return k_ELeaderboardSortMethodNone;
		}
	}

	ELeaderboardDisplayType ToSteamDisplayType(EESteamLeaderboardDisplayType DisplayType)
	{
		switch (DisplayType)
		{
		case EESteamLeaderboardDisplayType::Numeric:          return k_ELeaderboardDisplayTypeNumeric;
		case EESteamLeaderboardDisplayType::TimeSeconds:      return k_ELeaderboardDisplayTypeTimeSeconds;
		case EESteamLeaderboardDisplayType::TimeMilliSeconds: return k_ELeaderboardDisplayTypeTimeMilliSeconds;
		default:                                              return k_ELeaderboardDisplayTypeNone;
		}
	}

	ELeaderboardDataRequest ToSteamDataRequest(EESteamLeaderboardDataRequest RequestType)
	{
		switch (RequestType)
		{
		case EESteamLeaderboardDataRequest::GlobalAroundUser: return k_ELeaderboardDataRequestGlobalAroundUser;
		case EESteamLeaderboardDataRequest::Friends:          return k_ELeaderboardDataRequestFriends;
		case EESteamLeaderboardDataRequest::Users:            return k_ELeaderboardDataRequestUsers;
		default:                                              return k_ELeaderboardDataRequestGlobal;
		}
	}

	EESteamLeaderboardSortMethod FromSteamSortMethod(ELeaderboardSortMethod SortMethod)
	{
		switch (SortMethod)
		{
		case k_ELeaderboardSortMethodAscending:  return EESteamLeaderboardSortMethod::Ascending;
		case k_ELeaderboardSortMethodDescending: return EESteamLeaderboardSortMethod::Descending;
		default:                                 return EESteamLeaderboardSortMethod::None;
		}
	}

	EESteamLeaderboardDisplayType FromSteamDisplayType(ELeaderboardDisplayType DisplayType)
	{
		switch (DisplayType)
		{
		case k_ELeaderboardDisplayTypeNumeric:          return EESteamLeaderboardDisplayType::Numeric;
		case k_ELeaderboardDisplayTypeTimeSeconds:      return EESteamLeaderboardDisplayType::TimeSeconds;
		case k_ELeaderboardDisplayTypeTimeMilliSeconds: return EESteamLeaderboardDisplayType::TimeMilliSeconds;
		default:                                        return EESteamLeaderboardDisplayType::None;
		}
	}
}

/** Parameters captured to (re-)issue a queued ISteamUserStats::RequestUserStats call. */
struct FESteamPendingUserStatsRequest
{
	FESteamId User;
};

/** Parameters captured to (re-)issue a queued Find / FindOrCreateLeaderboard call. */
struct FESteamPendingLeaderboardFind
{
	FString Name;
	ELeaderboardSortMethod SortMethod = k_ELeaderboardSortMethodNone;
	ELeaderboardDisplayType DisplayType = k_ELeaderboardDisplayTypeNone;
	bool bCreate = false;
};

/** Parameters captured to (re-)issue a queued UploadLeaderboardScore call. */
struct FESteamPendingLeaderboardUpload
{
	SteamLeaderboard_t Handle = 0;
	int32 Score = 0;
	ELeaderboardUploadScoreMethod Method = k_ELeaderboardUploadScoreMethodKeepBest;
	/** Optional game-defined score details (empty = none). Capped to k_cLeaderboardDetailsMax. */
	TArray<int32> Details;
};

/**
 * Parameters captured to (re-)issue a queued DownloadLeaderboardEntries or
 * DownloadLeaderboardEntriesForUsers call (both share one CCallResult + queue). When bForUsers is
 * set the Users list is downloaded and RequestType/RangeStart/RangeEnd are ignored.
 */
struct FESteamPendingLeaderboardDownload
{
	SteamLeaderboard_t Handle = 0;
	ELeaderboardDataRequest RequestType = k_ELeaderboardDataRequestGlobal;
	int32 RangeStart = 0;
	int32 RangeEnd = 0;
	bool bForUsers = false;
	TArray<CSteamID> Users;
};

/** Parameters captured to (re-)issue a queued GetNumberOfCurrentPlayers call (no arguments). */
struct FESteamPendingNumberOfCurrentPlayers
{
};

/** Parameters captured to (re-)issue a queued RequestGlobalStats call. */
struct FESteamPendingGlobalStats
{
	int32 HistoryDays = 0;
};

/** Parameters captured to (re-)issue a queued RequestGlobalAchievementPercentages call (no arguments). */
struct FESteamPendingGlobalAchievementPercentages
{
};

/** Parameters captured to (re-)issue a queued AttachLeaderboardUGC call. */
struct FESteamPendingLeaderboardAttachUGC
{
	SteamLeaderboard_t Handle = 0;
	UGCHandle_t UGC = 0;
};

/**
 * Native Steam callback listeners; alive only while the Steam client API is initialized.
 *
 * Same-type async requests are serialized via a per-operation FIFO queue: while a given op's
 * CCallResult is in flight, further requests of that type are enqueued and issued in order as
 * each completion arrives, so none are dropped. Queued-but-unissued requests are abandoned
 * cleanly when the holder is destroyed on Steam shutdown / Deinitialize.
 */
class FESteamUserStatsCallbacks
{
public:
	explicit FESteamUserStatsCallbacks(UESteamUserStatsSubsystem* InOwner)
		: Owner(InOwner)
		, UserStatsReceivedCallback(this, &FESteamUserStatsCallbacks::HandleUserStatsReceived)
		, UserStatsStoredCallback(this, &FESteamUserStatsCallbacks::HandleUserStatsStored)
		, UserAchievementStoredCallback(this, &FESteamUserStatsCallbacks::HandleUserAchievementStored)
		, UserAchievementIconFetchedCallback(this, &FESteamUserStatsCallbacks::HandleUserAchievementIconFetched)
		, UserStatsUnloadedCallback(this, &FESteamUserStatsCallbacks::HandleUserStatsUnloaded)
	{
	}

	// Each Enqueue* issues the Steam call immediately when its operation is idle, otherwise it
	// queues the parameters. Returns true when the request was issued or queued; false only on
	// the immediate path when the Steam call could not be issued (preserves the public methods'
	// historical "return false when the request could not be issued" contract).

	bool EnqueueUserStatsRequest(const FESteamPendingUserStatsRequest& Request)
	{
		if (bUserStatsRequestBusy)
		{
			UserStatsRequestQueue.Enqueue(Request);
			return true;
		}
		return IssueUserStatsRequest(Request);
	}

	bool EnqueueLeaderboardFind(const FESteamPendingLeaderboardFind& Request)
	{
		if (bLeaderboardFindBusy)
		{
			LeaderboardFindQueue.Enqueue(Request);
			return true;
		}
		return IssueLeaderboardFind(Request);
	}

	bool EnqueueLeaderboardUpload(const FESteamPendingLeaderboardUpload& Request)
	{
		if (bLeaderboardUploadBusy)
		{
			LeaderboardUploadQueue.Enqueue(Request);
			return true;
		}
		return IssueLeaderboardUpload(Request);
	}

	bool EnqueueLeaderboardDownload(const FESteamPendingLeaderboardDownload& Request)
	{
		if (bLeaderboardDownloadBusy)
		{
			LeaderboardDownloadQueue.Enqueue(Request);
			return true;
		}
		return IssueLeaderboardDownload(Request);
	}

	bool EnqueueLeaderboardAttachUGC(const FESteamPendingLeaderboardAttachUGC& Request)
	{
		if (bLeaderboardAttachUGCBusy)
		{
			LeaderboardAttachUGCQueue.Enqueue(Request);
			return true;
		}
		return IssueLeaderboardAttachUGC(Request);
	}

	bool EnqueueNumberOfCurrentPlayers(const FESteamPendingNumberOfCurrentPlayers& Request)
	{
		if (bNumberOfCurrentPlayersBusy)
		{
			NumberOfCurrentPlayersQueue.Enqueue(Request);
			return true;
		}
		return IssueNumberOfCurrentPlayers(Request);
	}

	bool EnqueueGlobalStats(const FESteamPendingGlobalStats& Request)
	{
		if (bGlobalStatsBusy)
		{
			GlobalStatsQueue.Enqueue(Request);
			return true;
		}
		return IssueGlobalStats(Request);
	}

	bool EnqueueGlobalAchievementPercentages(const FESteamPendingGlobalAchievementPercentages& Request)
	{
		if (bGlobalAchievementPercentagesBusy)
		{
			GlobalAchievementPercentagesQueue.Enqueue(Request);
			return true;
		}
		return IssueGlobalAchievementPercentages(Request);
	}

private:
	// ---- Continuously-listening callbacks (not request/response) ----

	void HandleUserStatsReceived(UserStatsReceived_t* Data)
	{
		if (UESteamUserStatsSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnUserStatsReceived.Broadcast(
				FESteamId(Data->m_steamIDUser.ConvertToUint64()), Data->m_eResult == k_EResultOK);
		}
	}

	void HandleUserStatsStored(UserStatsStored_t* Data)
	{
		if (UESteamUserStatsSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnUserStatsStored.Broadcast(Data->m_eResult == k_EResultOK);
		}
	}

	void HandleUserAchievementStored(UserAchievementStored_t* Data)
	{
		if (UESteamUserStatsSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnAchievementStored.Broadcast(
				FString(UTF8_TO_TCHAR(Data->m_rgchAchievementName)),
				static_cast<int32>(Data->m_nCurProgress),
				static_cast<int32>(Data->m_nMaxProgress));
		}
	}

	void HandleUserAchievementIconFetched(UserAchievementIconFetched_t* Data)
	{
		if (UESteamUserStatsSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnAchievementIconFetched.Broadcast(
				FString(UTF8_TO_TCHAR(Data->m_rgchAchievementName)), static_cast<int32>(Data->m_nIconHandle));
		}
	}

	void HandleUserStatsUnloaded(UserStatsUnloaded_t* Data)
	{
		if (UESteamUserStatsSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnUserStatsUnloaded.Broadcast(FESteamId(Data->m_steamIDUser.ConvertToUint64()));
		}
	}

	// ---- RequestUserStats (serialized) ----

	bool IssueUserStatsRequest(const FESteamPendingUserStatsRequest& Request)
	{
		UESteamUserStatsSubsystem* Subsystem = Owner.Get();
		if (!Subsystem || !Subsystem->IsSteamAvailable() || !SteamUserStats())
		{
			return false;
		}

		const SteamAPICall_t Call = SteamUserStats()->RequestUserStats(CSteamID(Request.User.Value));
		if (Call == k_uAPICallInvalid)
		{
			return false;
		}
		UserStatsRequestResult.Set(Call, this, &FESteamUserStatsCallbacks::HandleUserStatsRequestResult);
		bUserStatsRequestBusy = true;
		return true;
	}

	void HandleUserStatsRequestResult(UserStatsReceived_t* Data, bool bIOFailure)
	{
		if (UESteamUserStatsSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnUserStatsReceived.Broadcast(
				FESteamId(Data->m_steamIDUser.ConvertToUint64()),
				!bIOFailure && Data->m_eResult == k_EResultOK);
		}
		bUserStatsRequestBusy = false;
		DrainUserStatsRequestQueue();
	}

	void DrainUserStatsRequestQueue()
	{
		while (!bUserStatsRequestBusy)
		{
			FESteamPendingUserStatsRequest Request;
			if (!UserStatsRequestQueue.Dequeue(Request))
			{
				break;
			}
			if (!IssueUserStatsRequest(Request))
			{
				// Steam went away while draining: fail this queued request instead of dropping it.
				if (UESteamUserStatsSubsystem* Subsystem = Owner.Get())
				{
					Subsystem->OnUserStatsReceived.Broadcast(Request.User, false);
				}
			}
		}
	}

	// ---- FindLeaderboard / FindOrCreateLeaderboard (serialized) ----

	bool IssueLeaderboardFind(const FESteamPendingLeaderboardFind& Request)
	{
		UESteamUserStatsSubsystem* Subsystem = Owner.Get();
		if (!Subsystem || !Subsystem->IsSteamAvailable() || !SteamUserStats())
		{
			return false;
		}

		const SteamAPICall_t Call = Request.bCreate
			? SteamUserStats()->FindOrCreateLeaderboard(TCHAR_TO_UTF8(*Request.Name), Request.SortMethod, Request.DisplayType)
			: SteamUserStats()->FindLeaderboard(TCHAR_TO_UTF8(*Request.Name));
		if (Call == k_uAPICallInvalid)
		{
			return false;
		}
		PendingLeaderboardName = Request.Name;
		LeaderboardFindResult.Set(Call, this, &FESteamUserStatsCallbacks::HandleLeaderboardFindResult);
		bLeaderboardFindBusy = true;
		return true;
	}

	void HandleLeaderboardFindResult(LeaderboardFindResult_t* Data, bool bIOFailure)
	{
		const FString LeaderboardName = MoveTemp(PendingLeaderboardName);
		PendingLeaderboardName.Reset();

		if (UESteamUserStatsSubsystem* Subsystem = Owner.Get())
		{
			const bool bSuccess = !bIOFailure && Data->m_bLeaderboardFound != 0;
			Subsystem->OnLeaderboardFound.Broadcast(
				bSuccess, static_cast<int64>(Data->m_hSteamLeaderboard), LeaderboardName);
		}
		bLeaderboardFindBusy = false;
		DrainLeaderboardFindQueue();
	}

	void DrainLeaderboardFindQueue()
	{
		while (!bLeaderboardFindBusy)
		{
			FESteamPendingLeaderboardFind Request;
			if (!LeaderboardFindQueue.Dequeue(Request))
			{
				break;
			}
			if (!IssueLeaderboardFind(Request))
			{
				if (UESteamUserStatsSubsystem* Subsystem = Owner.Get())
				{
					Subsystem->OnLeaderboardFound.Broadcast(false, 0, Request.Name);
				}
			}
		}
	}

	// ---- UploadLeaderboardScore (serialized) ----

	bool IssueLeaderboardUpload(const FESteamPendingLeaderboardUpload& Request)
	{
		UESteamUserStatsSubsystem* Subsystem = Owner.Get();
		if (!Subsystem || !Subsystem->IsSteamAvailable() || !SteamUserStats())
		{
			return false;
		}

		const int32* DetailsPtr = Request.Details.Num() > 0 ? Request.Details.GetData() : nullptr;
		const SteamAPICall_t Call = SteamUserStats()->UploadLeaderboardScore(
			Request.Handle, Request.Method, Request.Score, DetailsPtr, Request.Details.Num());
		if (Call == k_uAPICallInvalid)
		{
			return false;
		}
		LeaderboardUploadResult.Set(Call, this, &FESteamUserStatsCallbacks::HandleLeaderboardUploadResult);
		bLeaderboardUploadBusy = true;
		return true;
	}

	void HandleLeaderboardUploadResult(LeaderboardScoreUploaded_t* Data, bool bIOFailure)
	{
		if (UESteamUserStatsSubsystem* Subsystem = Owner.Get())
		{
			const bool bSuccess = !bIOFailure && Data->m_bSuccess != 0;
			Subsystem->OnLeaderboardScoreUploaded.Broadcast(
				bSuccess, Data->m_nScore, Data->m_bScoreChanged != 0, Data->m_nGlobalRankNew);
		}
		bLeaderboardUploadBusy = false;
		DrainLeaderboardUploadQueue();
	}

	void DrainLeaderboardUploadQueue()
	{
		while (!bLeaderboardUploadBusy)
		{
			FESteamPendingLeaderboardUpload Request;
			if (!LeaderboardUploadQueue.Dequeue(Request))
			{
				break;
			}
			if (!IssueLeaderboardUpload(Request))
			{
				if (UESteamUserStatsSubsystem* Subsystem = Owner.Get())
				{
					Subsystem->OnLeaderboardScoreUploaded.Broadcast(false, Request.Score, false, 0);
				}
			}
		}
	}

	// ---- DownloadLeaderboardEntries (serialized) ----

	bool IssueLeaderboardDownload(const FESteamPendingLeaderboardDownload& Request)
	{
		UESteamUserStatsSubsystem* Subsystem = Owner.Get();
		if (!Subsystem || !Subsystem->IsSteamAvailable() || !SteamUserStats())
		{
			return false;
		}

		SteamAPICall_t Call = k_uAPICallInvalid;
		if (Request.bForUsers)
		{
			// DownloadLeaderboardEntriesForUsers takes a non-const CSteamID array; the SDK does not
			// mutate it. A local copy keeps the queued request immutable across re-issues.
			TArray<CSteamID> Users = Request.Users;
			Call = SteamUserStats()->DownloadLeaderboardEntriesForUsers(
				Request.Handle, Users.GetData(), Users.Num());
		}
		else
		{
			Call = SteamUserStats()->DownloadLeaderboardEntries(
				Request.Handle, Request.RequestType, Request.RangeStart, Request.RangeEnd);
		}
		if (Call == k_uAPICallInvalid)
		{
			return false;
		}
		LeaderboardDownloadResult.Set(Call, this, &FESteamUserStatsCallbacks::HandleLeaderboardDownloadResult);
		bLeaderboardDownloadBusy = true;
		return true;
	}

	void HandleLeaderboardDownloadResult(LeaderboardScoresDownloaded_t* Data, bool bIOFailure)
	{
		if (UESteamUserStatsSubsystem* Subsystem = Owner.Get())
		{
			TArray<FESteamLeaderboardEntry> Entries;
			const bool bSuccess = !bIOFailure && SteamUserStats() != nullptr;
			if (bSuccess)
			{
				Entries.Reserve(Data->m_cEntryCount);
				for (int32 Index = 0; Index < Data->m_cEntryCount; ++Index)
				{
					LeaderboardEntry_t Entry;
					int32 DetailsBuffer[k_cLeaderboardDetailsMax];
					if (SteamUserStats()->GetDownloadedLeaderboardEntry(Data->m_hSteamLeaderboardEntries, Index, &Entry, DetailsBuffer, k_cLeaderboardDetailsMax))
					{
						FESteamLeaderboardEntry& OutEntry = Entries.AddDefaulted_GetRef();
						OutEntry.SteamId = FESteamId(Entry.m_steamIDUser.ConvertToUint64());
						OutEntry.GlobalRank = Entry.m_nGlobalRank;
						OutEntry.Score = Entry.m_nScore;
						OutEntry.UGCHandle = static_cast<int64>(Entry.m_hUGC);
						const int32 DetailsCount = FMath::Clamp(Entry.m_cDetails, 0, static_cast<int32>(k_cLeaderboardDetailsMax));
						OutEntry.Details.Append(DetailsBuffer, DetailsCount);
					}
				}
			}
			Subsystem->OnLeaderboardEntriesDownloaded.Broadcast(bSuccess, Entries);
		}
		bLeaderboardDownloadBusy = false;
		DrainLeaderboardDownloadQueue();
	}

	void DrainLeaderboardDownloadQueue()
	{
		while (!bLeaderboardDownloadBusy)
		{
			FESteamPendingLeaderboardDownload Request;
			if (!LeaderboardDownloadQueue.Dequeue(Request))
			{
				break;
			}
			if (!IssueLeaderboardDownload(Request))
			{
				if (UESteamUserStatsSubsystem* Subsystem = Owner.Get())
				{
					const TArray<FESteamLeaderboardEntry> EmptyEntries;
					Subsystem->OnLeaderboardEntriesDownloaded.Broadcast(false, EmptyEntries);
				}
			}
		}
	}

	// ---- AttachLeaderboardUGC (serialized) ----

	bool IssueLeaderboardAttachUGC(const FESteamPendingLeaderboardAttachUGC& Request)
	{
		UESteamUserStatsSubsystem* Subsystem = Owner.Get();
		if (!Subsystem || !Subsystem->IsSteamAvailable() || !SteamUserStats())
		{
			return false;
		}

		const SteamAPICall_t Call = SteamUserStats()->AttachLeaderboardUGC(Request.Handle, Request.UGC);
		if (Call == k_uAPICallInvalid)
		{
			return false;
		}
		LeaderboardAttachUGCResult.Set(Call, this, &FESteamUserStatsCallbacks::HandleLeaderboardAttachUGCResult);
		bLeaderboardAttachUGCBusy = true;
		return true;
	}

	void HandleLeaderboardAttachUGCResult(LeaderboardUGCSet_t* Data, bool bIOFailure)
	{
		if (UESteamUserStatsSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnLeaderboardUGCSet.Broadcast(!bIOFailure && Data->m_eResult == k_EResultOK);
		}
		bLeaderboardAttachUGCBusy = false;
		DrainLeaderboardAttachUGCQueue();
	}

	void DrainLeaderboardAttachUGCQueue()
	{
		while (!bLeaderboardAttachUGCBusy)
		{
			FESteamPendingLeaderboardAttachUGC Request;
			if (!LeaderboardAttachUGCQueue.Dequeue(Request))
			{
				break;
			}
			if (!IssueLeaderboardAttachUGC(Request))
			{
				if (UESteamUserStatsSubsystem* Subsystem = Owner.Get())
				{
					Subsystem->OnLeaderboardUGCSet.Broadcast(false);
				}
			}
		}
	}

	// ---- GetNumberOfCurrentPlayers (serialized) ----

	bool IssueNumberOfCurrentPlayers(const FESteamPendingNumberOfCurrentPlayers& Request)
	{
		UESteamUserStatsSubsystem* Subsystem = Owner.Get();
		if (!Subsystem || !Subsystem->IsSteamAvailable() || !SteamUserStats())
		{
			return false;
		}

		const SteamAPICall_t Call = SteamUserStats()->GetNumberOfCurrentPlayers();
		if (Call == k_uAPICallInvalid)
		{
			return false;
		}
		NumberOfCurrentPlayersResult.Set(Call, this, &FESteamUserStatsCallbacks::HandleNumberOfCurrentPlayersResult);
		bNumberOfCurrentPlayersBusy = true;
		return true;
	}

	void HandleNumberOfCurrentPlayersResult(NumberOfCurrentPlayers_t* Data, bool bIOFailure)
	{
		if (UESteamUserStatsSubsystem* Subsystem = Owner.Get())
		{
			const bool bSuccess = !bIOFailure && Data->m_bSuccess != 0;
			Subsystem->OnNumberOfCurrentPlayers.Broadcast(bSuccess, bSuccess ? Data->m_cPlayers : 0);
		}
		bNumberOfCurrentPlayersBusy = false;
		DrainNumberOfCurrentPlayersQueue();
	}

	void DrainNumberOfCurrentPlayersQueue()
	{
		while (!bNumberOfCurrentPlayersBusy)
		{
			FESteamPendingNumberOfCurrentPlayers Request;
			if (!NumberOfCurrentPlayersQueue.Dequeue(Request))
			{
				break;
			}
			if (!IssueNumberOfCurrentPlayers(Request))
			{
				if (UESteamUserStatsSubsystem* Subsystem = Owner.Get())
				{
					Subsystem->OnNumberOfCurrentPlayers.Broadcast(false, 0);
				}
			}
		}
	}

	// ---- RequestGlobalStats (serialized) ----

	bool IssueGlobalStats(const FESteamPendingGlobalStats& Request)
	{
		UESteamUserStatsSubsystem* Subsystem = Owner.Get();
		if (!Subsystem || !Subsystem->IsSteamAvailable() || !SteamUserStats())
		{
			return false;
		}

		const SteamAPICall_t Call = SteamUserStats()->RequestGlobalStats(Request.HistoryDays);
		if (Call == k_uAPICallInvalid)
		{
			return false;
		}
		GlobalStatsResult.Set(Call, this, &FESteamUserStatsCallbacks::HandleGlobalStatsResult);
		bGlobalStatsBusy = true;
		return true;
	}

	void HandleGlobalStatsResult(GlobalStatsReceived_t* Data, bool bIOFailure)
	{
		if (UESteamUserStatsSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnGlobalStatsReceived.Broadcast(!bIOFailure && Data->m_eResult == k_EResultOK);
		}
		bGlobalStatsBusy = false;
		DrainGlobalStatsQueue();
	}

	void DrainGlobalStatsQueue()
	{
		while (!bGlobalStatsBusy)
		{
			FESteamPendingGlobalStats Request;
			if (!GlobalStatsQueue.Dequeue(Request))
			{
				break;
			}
			if (!IssueGlobalStats(Request))
			{
				if (UESteamUserStatsSubsystem* Subsystem = Owner.Get())
				{
					Subsystem->OnGlobalStatsReceived.Broadcast(false);
				}
			}
		}
	}

	// ---- RequestGlobalAchievementPercentages (serialized) ----

	bool IssueGlobalAchievementPercentages(const FESteamPendingGlobalAchievementPercentages& Request)
	{
		UESteamUserStatsSubsystem* Subsystem = Owner.Get();
		if (!Subsystem || !Subsystem->IsSteamAvailable() || !SteamUserStats())
		{
			return false;
		}

		const SteamAPICall_t Call = SteamUserStats()->RequestGlobalAchievementPercentages();
		if (Call == k_uAPICallInvalid)
		{
			return false;
		}
		GlobalAchievementPercentagesResult.Set(Call, this, &FESteamUserStatsCallbacks::HandleGlobalAchievementPercentagesResult);
		bGlobalAchievementPercentagesBusy = true;
		return true;
	}

	void HandleGlobalAchievementPercentagesResult(GlobalAchievementPercentagesReady_t* Data, bool bIOFailure)
	{
		if (UESteamUserStatsSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnGlobalAchievementPercentagesReady.Broadcast(!bIOFailure && Data->m_eResult == k_EResultOK);
		}
		bGlobalAchievementPercentagesBusy = false;
		DrainGlobalAchievementPercentagesQueue();
	}

	void DrainGlobalAchievementPercentagesQueue()
	{
		while (!bGlobalAchievementPercentagesBusy)
		{
			FESteamPendingGlobalAchievementPercentages Request;
			if (!GlobalAchievementPercentagesQueue.Dequeue(Request))
			{
				break;
			}
			if (!IssueGlobalAchievementPercentages(Request))
			{
				if (UESteamUserStatsSubsystem* Subsystem = Owner.Get())
				{
					Subsystem->OnGlobalAchievementPercentagesReady.Broadcast(false);
				}
			}
		}
	}

	TWeakObjectPtr<UESteamUserStatsSubsystem> Owner;
	FString PendingLeaderboardName;

	CCallback<FESteamUserStatsCallbacks, UserStatsReceived_t> UserStatsReceivedCallback;
	CCallback<FESteamUserStatsCallbacks, UserStatsStored_t> UserStatsStoredCallback;
	CCallback<FESteamUserStatsCallbacks, UserAchievementStored_t> UserAchievementStoredCallback;
	CCallback<FESteamUserStatsCallbacks, UserAchievementIconFetched_t> UserAchievementIconFetchedCallback;
	CCallback<FESteamUserStatsCallbacks, UserStatsUnloaded_t> UserStatsUnloadedCallback;

	CCallResult<FESteamUserStatsCallbacks, UserStatsReceived_t> UserStatsRequestResult;
	CCallResult<FESteamUserStatsCallbacks, LeaderboardFindResult_t> LeaderboardFindResult;
	CCallResult<FESteamUserStatsCallbacks, LeaderboardScoreUploaded_t> LeaderboardUploadResult;
	CCallResult<FESteamUserStatsCallbacks, LeaderboardScoresDownloaded_t> LeaderboardDownloadResult;
	CCallResult<FESteamUserStatsCallbacks, LeaderboardUGCSet_t> LeaderboardAttachUGCResult;
	CCallResult<FESteamUserStatsCallbacks, NumberOfCurrentPlayers_t> NumberOfCurrentPlayersResult;
	CCallResult<FESteamUserStatsCallbacks, GlobalStatsReceived_t> GlobalStatsResult;
	CCallResult<FESteamUserStatsCallbacks, GlobalAchievementPercentagesReady_t> GlobalAchievementPercentagesResult;

	// In-flight flags + FIFO queues, one per serialized operation type.
	bool bUserStatsRequestBusy = false;
	bool bLeaderboardFindBusy = false;
	bool bLeaderboardUploadBusy = false;
	bool bLeaderboardDownloadBusy = false;
	bool bLeaderboardAttachUGCBusy = false;
	bool bNumberOfCurrentPlayersBusy = false;
	bool bGlobalStatsBusy = false;
	bool bGlobalAchievementPercentagesBusy = false;

	TQueue<FESteamPendingUserStatsRequest> UserStatsRequestQueue;
	TQueue<FESteamPendingLeaderboardFind> LeaderboardFindQueue;
	TQueue<FESteamPendingLeaderboardUpload> LeaderboardUploadQueue;
	TQueue<FESteamPendingLeaderboardDownload> LeaderboardDownloadQueue;
	TQueue<FESteamPendingLeaderboardAttachUGC> LeaderboardAttachUGCQueue;
	TQueue<FESteamPendingNumberOfCurrentPlayers> NumberOfCurrentPlayersQueue;
	TQueue<FESteamPendingGlobalStats> GlobalStatsQueue;
	TQueue<FESteamPendingGlobalAchievementPercentages> GlobalAchievementPercentagesQueue;
};
#else
class FESteamUserStatsCallbacks
{
};
#endif // WITH_EXTENDEDSTEAM_SDK

void UESteamUserStatsSubsystem::Deinitialize()
{
	Super::Deinitialize();
	Callbacks.Reset();
}

void UESteamUserStatsSubsystem::HandleSteamClientInitialized()
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!Callbacks)
	{
		Callbacks = MakeShared<FESteamUserStatsCallbacks>(this);
	}
#endif
}

void UESteamUserStatsSubsystem::HandleSteamClientShutdown()
{
	Callbacks.Reset();
}

bool UESteamUserStatsSubsystem::RequestCurrentStats()
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUserStats())
	{
		LogSteamUnavailable(TEXT("RequestCurrentStats"));
		return false;
	}
#if ESTEAM_SDK_AT_LEAST(161)
	// ISteamUserStats::RequestCurrentStats was removed in Steamworks SDK 1.61: the Steam
	// client now synchronizes the local user's stats/achievements before the game process
	// starts, so there is nothing to request. Kept for API stability; succeeds immediately.
	return true;
#else
	return SteamUserStats()->RequestCurrentStats();
#endif
#else
	return false;
#endif
}

bool UESteamUserStatsSubsystem::StoreStats()
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUserStats())
	{
		LogSteamUnavailable(TEXT("StoreStats"));
		return false;
	}
	return SteamUserStats()->StoreStats();
#else
	return false;
#endif
}

bool UESteamUserStatsSubsystem::ResetAllStats(bool bAchievementsToo)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUserStats())
	{
		LogSteamUnavailable(TEXT("ResetAllStats"));
		return false;
	}
	return SteamUserStats()->ResetAllStats(bAchievementsToo);
#else
	return false;
#endif
}

bool UESteamUserStatsSubsystem::GetStatInt(const FString& Name, int32& OutValue) const
{
	OutValue = 0;
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamUserStats())
	{
		return SteamUserStats()->GetStat(TCHAR_TO_UTF8(*Name), &OutValue);
	}
#endif
	return false;
}

bool UESteamUserStatsSubsystem::GetStatFloat(const FString& Name, float& OutValue) const
{
	OutValue = 0.0f;
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamUserStats())
	{
		return SteamUserStats()->GetStat(TCHAR_TO_UTF8(*Name), &OutValue);
	}
#endif
	return false;
}

bool UESteamUserStatsSubsystem::SetStatInt(const FString& Name, int32 Value)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUserStats())
	{
		LogSteamUnavailable(TEXT("SetStatInt"));
		return false;
	}
	return SteamUserStats()->SetStat(TCHAR_TO_UTF8(*Name), Value);
#else
	return false;
#endif
}

bool UESteamUserStatsSubsystem::SetStatFloat(const FString& Name, float Value)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUserStats())
	{
		LogSteamUnavailable(TEXT("SetStatFloat"));
		return false;
	}
	return SteamUserStats()->SetStat(TCHAR_TO_UTF8(*Name), Value);
#else
	return false;
#endif
}

bool UESteamUserStatsSubsystem::UpdateAvgRateStat(const FString& Name, float CountThisSession, float SessionLengthSeconds)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUserStats())
	{
		LogSteamUnavailable(TEXT("UpdateAvgRateStat"));
		return false;
	}
	return SteamUserStats()->UpdateAvgRateStat(TCHAR_TO_UTF8(*Name), CountThisSession, static_cast<double>(SessionLengthSeconds));
#else
	return false;
#endif
}

bool UESteamUserStatsSubsystem::GetAchievement(const FString& Name, bool& bUnlocked) const
{
	bUnlocked = false;
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamUserStats())
	{
		return SteamUserStats()->GetAchievement(TCHAR_TO_UTF8(*Name), &bUnlocked);
	}
#endif
	return false;
}

bool UESteamUserStatsSubsystem::GetAchievementAndUnlockTime(const FString& Name, bool& bUnlocked, int64& UnlockTime) const
{
	bUnlocked = false;
	UnlockTime = 0;
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamUserStats())
	{
		uint32 UnlockTimeSeconds = 0;
		if (SteamUserStats()->GetAchievementAndUnlockTime(TCHAR_TO_UTF8(*Name), &bUnlocked, &UnlockTimeSeconds))
		{
			UnlockTime = static_cast<int64>(UnlockTimeSeconds);
			return true;
		}
	}
#endif
	return false;
}

bool UESteamUserStatsSubsystem::SetAchievement(const FString& Name)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUserStats())
	{
		LogSteamUnavailable(TEXT("SetAchievement"));
		return false;
	}
	return SteamUserStats()->SetAchievement(TCHAR_TO_UTF8(*Name));
#else
	return false;
#endif
}

bool UESteamUserStatsSubsystem::ClearAchievement(const FString& Name)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUserStats())
	{
		LogSteamUnavailable(TEXT("ClearAchievement"));
		return false;
	}
	return SteamUserStats()->ClearAchievement(TCHAR_TO_UTF8(*Name));
#else
	return false;
#endif
}

int32 UESteamUserStatsSubsystem::GetNumAchievements() const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamUserStats())
	{
		return static_cast<int32>(SteamUserStats()->GetNumAchievements());
	}
#endif
	return 0;
}

FString UESteamUserStatsSubsystem::GetAchievementName(int32 Index) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamUserStats() && Index >= 0)
	{
		return FString(UTF8_TO_TCHAR(SteamUserStats()->GetAchievementName(static_cast<uint32>(Index))));
	}
#endif
	return FString();
}

FString UESteamUserStatsSubsystem::GetAchievementDisplayAttribute(const FString& Name, const FString& Key) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamUserStats())
	{
		return FString(UTF8_TO_TCHAR(SteamUserStats()->GetAchievementDisplayAttribute(TCHAR_TO_UTF8(*Name), TCHAR_TO_UTF8(*Key))));
	}
#endif
	return FString();
}

bool UESteamUserStatsSubsystem::IndicateAchievementProgress(const FString& Name, int32 CurProgress, int32 MaxProgress)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUserStats())
	{
		LogSteamUnavailable(TEXT("IndicateAchievementProgress"));
		return false;
	}
	return SteamUserStats()->IndicateAchievementProgress(
		TCHAR_TO_UTF8(*Name),
		static_cast<uint32>(FMath::Max(CurProgress, 0)),
		static_cast<uint32>(FMath::Max(MaxProgress, 0)));
#else
	return false;
#endif
}

bool UESteamUserStatsSubsystem::GetAchievementProgressLimitsInt(const FString& Name, int32& OutMinProgress, int32& OutMaxProgress) const
{
	OutMinProgress = 0;
	OutMaxProgress = 0;
#if ESTEAM_SDK_AT_LEAST(153)
	if (IsSteamAvailable() && SteamUserStats())
	{
		return SteamUserStats()->GetAchievementProgressLimits(TCHAR_TO_UTF8(*Name), &OutMinProgress, &OutMaxProgress);
	}
#endif
	return false;
}

bool UESteamUserStatsSubsystem::GetAchievementProgressLimitsFloat(const FString& Name, float& OutMinProgress, float& OutMaxProgress) const
{
	OutMinProgress = 0.0f;
	OutMaxProgress = 0.0f;
#if ESTEAM_SDK_AT_LEAST(153)
	if (IsSteamAvailable() && SteamUserStats())
	{
		return SteamUserStats()->GetAchievementProgressLimits(TCHAR_TO_UTF8(*Name), &OutMinProgress, &OutMaxProgress);
	}
#endif
	return false;
}

int32 UESteamUserStatsSubsystem::GetAchievementIcon(const FString& Name) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamUserStats())
	{
		return SteamUserStats()->GetAchievementIcon(TCHAR_TO_UTF8(*Name));
	}
#endif
	return 0;
}

bool UESteamUserStatsSubsystem::RequestGlobalAchievementPercentages()
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUserStats() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("RequestGlobalAchievementPercentages"));
		return false;
	}

	FESteamPendingGlobalAchievementPercentages Request;
	return Callbacks->EnqueueGlobalAchievementPercentages(Request);
#else
	return false;
#endif
}

bool UESteamUserStatsSubsystem::GetAchievementAchievedPercent(const FString& Name, float& OutPercent) const
{
	OutPercent = 0.0f;
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamUserStats())
	{
		return SteamUserStats()->GetAchievementAchievedPercent(TCHAR_TO_UTF8(*Name), &OutPercent);
	}
#endif
	return false;
}

int32 UESteamUserStatsSubsystem::GetMostAchievedAchievementInfo(FString& OutName, float& OutPercent, bool& bAchieved) const
{
	OutName.Reset();
	OutPercent = 0.0f;
	bAchieved = false;
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamUserStats())
	{
		char NameBuffer[k_cchStatNameMax] = {};
		const int32 Iterator = SteamUserStats()->GetMostAchievedAchievementInfo(NameBuffer, sizeof(NameBuffer), &OutPercent, &bAchieved);
		if (Iterator >= 0)
		{
			OutName = FString(UTF8_TO_TCHAR(NameBuffer));
		}
		return Iterator;
	}
#endif
	return -1;
}

int32 UESteamUserStatsSubsystem::GetNextMostAchievedAchievementInfo(int32 PreviousIterator, FString& OutName, float& OutPercent, bool& bAchieved) const
{
	OutName.Reset();
	OutPercent = 0.0f;
	bAchieved = false;
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamUserStats())
	{
		char NameBuffer[k_cchStatNameMax] = {};
		const int32 Iterator = SteamUserStats()->GetNextMostAchievedAchievementInfo(PreviousIterator, NameBuffer, sizeof(NameBuffer), &OutPercent, &bAchieved);
		if (Iterator >= 0)
		{
			OutName = FString(UTF8_TO_TCHAR(NameBuffer));
		}
		return Iterator;
	}
#endif
	return -1;
}

bool UESteamUserStatsSubsystem::RequestUserStats(FESteamId SteamId)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUserStats() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("RequestUserStats"));
		return false;
	}
	if (!SteamId.IsValid())
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("RequestUserStats: invalid steam id"));
		return false;
	}

	FESteamPendingUserStatsRequest Request;
	Request.User = SteamId;
	return Callbacks->EnqueueUserStatsRequest(Request);
#else
	return false;
#endif
}

bool UESteamUserStatsSubsystem::GetUserAchievement(FESteamId SteamId, const FString& Name, bool& bUnlocked) const
{
	bUnlocked = false;
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamUserStats() && SteamId.IsValid())
	{
		return SteamUserStats()->GetUserAchievement(CSteamID(SteamId.Value), TCHAR_TO_UTF8(*Name), &bUnlocked);
	}
#endif
	return false;
}

bool UESteamUserStatsSubsystem::GetUserAchievementAndUnlockTime(FESteamId SteamId, const FString& Name, bool& bUnlocked, int64& UnlockTime) const
{
	bUnlocked = false;
	UnlockTime = 0;
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamUserStats() && SteamId.IsValid())
	{
		uint32 UnlockTimeSeconds = 0;
		if (SteamUserStats()->GetUserAchievementAndUnlockTime(CSteamID(SteamId.Value), TCHAR_TO_UTF8(*Name), &bUnlocked, &UnlockTimeSeconds))
		{
			UnlockTime = static_cast<int64>(UnlockTimeSeconds);
			return true;
		}
	}
#endif
	return false;
}

bool UESteamUserStatsSubsystem::GetUserStatInt(FESteamId SteamId, const FString& Name, int32& OutValue) const
{
	OutValue = 0;
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamUserStats() && SteamId.IsValid())
	{
		return SteamUserStats()->GetUserStat(CSteamID(SteamId.Value), TCHAR_TO_UTF8(*Name), &OutValue);
	}
#endif
	return false;
}

bool UESteamUserStatsSubsystem::GetUserStatFloat(FESteamId SteamId, const FString& Name, float& OutValue) const
{
	OutValue = 0.0f;
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamUserStats() && SteamId.IsValid())
	{
		return SteamUserStats()->GetUserStat(CSteamID(SteamId.Value), TCHAR_TO_UTF8(*Name), &OutValue);
	}
#endif
	return false;
}

bool UESteamUserStatsSubsystem::FindOrCreateLeaderboard(const FString& LeaderboardName, EESteamLeaderboardSortMethod SortMethod, EESteamLeaderboardDisplayType DisplayType)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUserStats() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("FindOrCreateLeaderboard"));
		return false;
	}

	FESteamPendingLeaderboardFind Request;
	Request.Name = LeaderboardName;
	Request.SortMethod = ToSteamSortMethod(SortMethod);
	Request.DisplayType = ToSteamDisplayType(DisplayType);
	Request.bCreate = true;
	return Callbacks->EnqueueLeaderboardFind(Request);
#else
	return false;
#endif
}

bool UESteamUserStatsSubsystem::FindLeaderboard(const FString& LeaderboardName)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUserStats() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("FindLeaderboard"));
		return false;
	}

	FESteamPendingLeaderboardFind Request;
	Request.Name = LeaderboardName;
	Request.bCreate = false;
	return Callbacks->EnqueueLeaderboardFind(Request);
#else
	return false;
#endif
}

bool UESteamUserStatsSubsystem::UploadLeaderboardScore(int64 LeaderboardHandle, int32 Score, bool bForceUpdate)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUserStats() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("UploadLeaderboardScore"));
		return false;
	}
	if (LeaderboardHandle == 0)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("UploadLeaderboardScore: invalid leaderboard handle"));
		return false;
	}

	FESteamPendingLeaderboardUpload Request;
	Request.Handle = static_cast<SteamLeaderboard_t>(LeaderboardHandle);
	Request.Score = Score;
	Request.Method = bForceUpdate ? k_ELeaderboardUploadScoreMethodForceUpdate : k_ELeaderboardUploadScoreMethodKeepBest;
	return Callbacks->EnqueueLeaderboardUpload(Request);
#else
	return false;
#endif
}

bool UESteamUserStatsSubsystem::UploadLeaderboardScoreWithDetails(int64 LeaderboardHandle, int32 Score, bool bForceUpdate, const TArray<int32>& Details)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUserStats() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("UploadLeaderboardScoreWithDetails"));
		return false;
	}
	if (LeaderboardHandle == 0)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("UploadLeaderboardScoreWithDetails: invalid leaderboard handle"));
		return false;
	}

	FESteamPendingLeaderboardUpload Request;
	Request.Handle = static_cast<SteamLeaderboard_t>(LeaderboardHandle);
	Request.Score = Score;
	Request.Method = bForceUpdate ? k_ELeaderboardUploadScoreMethodForceUpdate : k_ELeaderboardUploadScoreMethodKeepBest;
	Request.Details = Details;
	if (Request.Details.Num() > k_cLeaderboardDetailsMax)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("UploadLeaderboardScoreWithDetails: %d details exceeds the SDK max of %d, truncating"),
			Request.Details.Num(), static_cast<int32>(k_cLeaderboardDetailsMax));
		Request.Details.SetNum(k_cLeaderboardDetailsMax);
	}
	return Callbacks->EnqueueLeaderboardUpload(Request);
#else
	return false;
#endif
}

bool UESteamUserStatsSubsystem::DownloadLeaderboardEntries(int64 LeaderboardHandle, EESteamLeaderboardDataRequest RequestType, int32 RangeStart, int32 RangeEnd)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUserStats() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("DownloadLeaderboardEntries"));
		return false;
	}
	if (LeaderboardHandle == 0)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("DownloadLeaderboardEntries: invalid leaderboard handle"));
		return false;
	}

	FESteamPendingLeaderboardDownload Request;
	Request.Handle = static_cast<SteamLeaderboard_t>(LeaderboardHandle);
	Request.RequestType = ToSteamDataRequest(RequestType);
	Request.RangeStart = RangeStart;
	Request.RangeEnd = RangeEnd;
	return Callbacks->EnqueueLeaderboardDownload(Request);
#else
	return false;
#endif
}

bool UESteamUserStatsSubsystem::DownloadLeaderboardEntriesForUsers(int64 LeaderboardHandle, const TArray<FESteamId>& Users)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUserStats() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("DownloadLeaderboardEntriesForUsers"));
		return false;
	}
	if (LeaderboardHandle == 0)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("DownloadLeaderboardEntriesForUsers: invalid leaderboard handle"));
		return false;
	}
	if (Users.Num() == 0)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("DownloadLeaderboardEntriesForUsers: empty user list"));
		return false;
	}

	FESteamPendingLeaderboardDownload Request;
	Request.Handle = static_cast<SteamLeaderboard_t>(LeaderboardHandle);
	Request.RequestType = k_ELeaderboardDataRequestUsers;
	Request.bForUsers = true;
	Request.Users.Reserve(Users.Num());
	for (const FESteamId& User : Users)
	{
		Request.Users.Add(CSteamID(User.Value));
	}
	// The SDK accepts at most 100 users per call; extra ids would be rejected outright.
	if (Request.Users.Num() > 100)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("DownloadLeaderboardEntriesForUsers: %d users exceeds the SDK max of 100, truncating"), Request.Users.Num());
		Request.Users.SetNum(100);
	}
	return Callbacks->EnqueueLeaderboardDownload(Request);
#else
	return false;
#endif
}

bool UESteamUserStatsSubsystem::AttachLeaderboardUGC(int64 LeaderboardHandle, int64 UGCHandle)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUserStats() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("AttachLeaderboardUGC"));
		return false;
	}
	if (LeaderboardHandle == 0)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("AttachLeaderboardUGC: invalid leaderboard handle"));
		return false;
	}

	FESteamPendingLeaderboardAttachUGC Request;
	Request.Handle = static_cast<SteamLeaderboard_t>(LeaderboardHandle);
	Request.UGC = static_cast<UGCHandle_t>(UGCHandle);
	return Callbacks->EnqueueLeaderboardAttachUGC(Request);
#else
	return false;
#endif
}

int32 UESteamUserStatsSubsystem::GetLeaderboardEntryCount(int64 LeaderboardHandle) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamUserStats() && LeaderboardHandle != 0)
	{
		return SteamUserStats()->GetLeaderboardEntryCount(static_cast<SteamLeaderboard_t>(LeaderboardHandle));
	}
#endif
	return 0;
}

FString UESteamUserStatsSubsystem::GetLeaderboardName(int64 LeaderboardHandle) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamUserStats() && LeaderboardHandle != 0)
	{
		return FString(UTF8_TO_TCHAR(SteamUserStats()->GetLeaderboardName(static_cast<SteamLeaderboard_t>(LeaderboardHandle))));
	}
#endif
	return FString();
}

EESteamLeaderboardSortMethod UESteamUserStatsSubsystem::GetLeaderboardSortMethod(int64 LeaderboardHandle) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamUserStats() && LeaderboardHandle != 0)
	{
		return FromSteamSortMethod(SteamUserStats()->GetLeaderboardSortMethod(static_cast<SteamLeaderboard_t>(LeaderboardHandle)));
	}
#endif
	return EESteamLeaderboardSortMethod::None;
}

EESteamLeaderboardDisplayType UESteamUserStatsSubsystem::GetLeaderboardDisplayType(int64 LeaderboardHandle) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamUserStats() && LeaderboardHandle != 0)
	{
		return FromSteamDisplayType(SteamUserStats()->GetLeaderboardDisplayType(static_cast<SteamLeaderboard_t>(LeaderboardHandle)));
	}
#endif
	return EESteamLeaderboardDisplayType::None;
}

bool UESteamUserStatsSubsystem::GetNumberOfCurrentPlayers()
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUserStats() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("GetNumberOfCurrentPlayers"));
		return false;
	}

	FESteamPendingNumberOfCurrentPlayers Request;
	return Callbacks->EnqueueNumberOfCurrentPlayers(Request);
#else
	return false;
#endif
}

bool UESteamUserStatsSubsystem::RequestGlobalStats(int32 HistoryDays)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUserStats() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("RequestGlobalStats"));
		return false;
	}

	FESteamPendingGlobalStats Request;
	// The SDK caps day-by-day history at 60 days; clamp so the request is never rejected.
	Request.HistoryDays = FMath::Clamp(HistoryDays, 0, 60);
	return Callbacks->EnqueueGlobalStats(Request);
#else
	return false;
#endif
}

bool UESteamUserStatsSubsystem::GetGlobalStatInt(const FString& Name, int64& OutValue) const
{
	OutValue = 0;
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamUserStats())
	{
		return SteamUserStats()->GetGlobalStat(TCHAR_TO_UTF8(*Name), &OutValue);
	}
#endif
	return false;
}

bool UESteamUserStatsSubsystem::GetGlobalStatFloat(const FString& Name, double& OutValue) const
{
	OutValue = 0.0;
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamUserStats())
	{
		return SteamUserStats()->GetGlobalStat(TCHAR_TO_UTF8(*Name), &OutValue);
	}
#endif
	return false;
}

void UESteamUserStatsSubsystem::GetGlobalStatHistoryInt(const FString& Name, TArray<int64>& OutValues, int32 Days) const
{
	OutValues.Reset();
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamUserStats() && Days > 0)
	{
		TArray<int64> Buffer;
		Buffer.SetNumZeroed(Days);
		const int32 Count = SteamUserStats()->GetGlobalStatHistory(
			TCHAR_TO_UTF8(*Name), Buffer.GetData(), static_cast<uint32>(Days) * sizeof(int64));
		Buffer.SetNum(FMath::Clamp(Count, 0, Days));
		OutValues = MoveTemp(Buffer);
	}
#endif
}

void UESteamUserStatsSubsystem::GetGlobalStatHistoryFloat(const FString& Name, TArray<double>& OutValues, int32 Days) const
{
	OutValues.Reset();
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamUserStats() && Days > 0)
	{
		TArray<double> Buffer;
		Buffer.SetNumZeroed(Days);
		const int32 Count = SteamUserStats()->GetGlobalStatHistory(
			TCHAR_TO_UTF8(*Name), Buffer.GetData(), static_cast<uint32>(Days) * sizeof(double));
		Buffer.SetNum(FMath::Clamp(Count, 0, Days));
		OutValues = MoveTemp(Buffer);
	}
#endif
}
