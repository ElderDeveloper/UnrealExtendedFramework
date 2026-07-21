// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Leaderboards/OnlineLeaderboardsExtendedSteam.h"
#include "Core/OnlineSubsystemExtendedSteam.h"
#include "ExtendedSteamSharedModule.h"
#include "Shared/ESteamSDK.h"

#if WITH_EXTENDEDSTEAM_SDK
THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
THIRD_PARTY_INCLUDES_END
#endif

namespace ExtendedSteamLeaderboards
{
	/** Rows fetched by a ReadLeaderboards call that names no players (global top-of-table read). */
	static constexpr int32 DefaultGlobalReadCount = 100;

	/** Steam caps DownloadLeaderboardEntriesForUsers at 100 SteamIDs per call. */
	static constexpr int32 MaxUsersPerDownload = 100;

	/** True while the shared module has the Steam client API up. */
	static bool IsSteamClientUp()
	{
#if WITH_EXTENDEDSTEAM_SDK
		return FExtendedSteamSharedModule::IsModuleAvailable()
			&& FExtendedSteamSharedModule::Get().IsSteamClientInitialized();
#else
		return false;
#endif
	}

	/** Extracts an int32 leaderboard score from a stat value (Steam scores are int32 only). */
	static bool ToInt32Score(const FVariantData& Value, int32& OutScore)
	{
		switch (Value.GetType())
		{
		case EOnlineKeyValuePairDataType::Int32:
		{
			Value.GetValue(OutScore);
			return true;
		}
		case EOnlineKeyValuePairDataType::UInt32:
		{
			uint32 UIntValue = 0;
			Value.GetValue(UIntValue);
			OutScore = static_cast<int32>(FMath::Min<uint32>(UIntValue, static_cast<uint32>(MAX_int32)));
			return true;
		}
		case EOnlineKeyValuePairDataType::Int64:
		{
			int64 Int64Value = 0;
			Value.GetValue(Int64Value);
			OutScore = static_cast<int32>(FMath::Clamp<int64>(Int64Value, MIN_int32, MAX_int32));
			return true;
		}
		case EOnlineKeyValuePairDataType::Float:
		{
			float FloatValue = 0.0f;
			Value.GetValue(FloatValue);
			OutScore = static_cast<int32>(FloatValue);
			return true;
		}
		case EOnlineKeyValuePairDataType::Double:
		{
			double DoubleValue = 0.0;
			Value.GetValue(DoubleValue);
			OutScore = static_cast<int32>(DoubleValue);
			return true;
		}
		default:
			return false;
		}
	}

#if WITH_EXTENDEDSTEAM_SDK
	static ELeaderboardSortMethod ToSteamSortMethod(ELeaderboardSort::Type SortMethod)
	{
		switch (SortMethod)
		{
		case ELeaderboardSort::Ascending:  return k_ELeaderboardSortMethodAscending;
		case ELeaderboardSort::Descending: return k_ELeaderboardSortMethodDescending;
		default:                           return k_ELeaderboardSortMethodNone;
		}
	}

	static ELeaderboardDisplayType ToSteamDisplayType(ELeaderboardFormat::Type DisplayFormat)
	{
		switch (DisplayFormat)
		{
		case ELeaderboardFormat::Seconds:      return k_ELeaderboardDisplayTypeTimeSeconds;
		case ELeaderboardFormat::Milliseconds: return k_ELeaderboardDisplayTypeTimeMilliSeconds;
		default:                               return k_ELeaderboardDisplayTypeNumeric;
		}
	}

	static ELeaderboardUploadScoreMethod ToSteamUploadMethod(ELeaderboardUpdateMethod::Type UpdateMethod)
	{
		return UpdateMethod == ELeaderboardUpdateMethod::Force
			? k_ELeaderboardUploadScoreMethodForceUpdate
			: k_ELeaderboardUploadScoreMethodKeepBest;
	}
#endif // WITH_EXTENDEDSTEAM_SDK
}

#if WITH_EXTENDEDSTEAM_SDK
/**
 * Native Steam call-result listeners for the leaderboards interface. Separate CCallResults for
 * the read pipeline (find + download) and the write pipeline (find + upload) so a read and a
 * write may run concurrently; each pipeline itself is single-flight (re-tracking would drop the
 * older pending result, which the owner prevents).
 */
class FOnlineLeaderboardsExtendedSteamCallbacks
{
public:
	explicit FOnlineLeaderboardsExtendedSteamCallbacks(FOnlineLeaderboardsExtendedSteam* InOwner)
		: Owner(InOwner)
	{
	}

	void TrackReadFind(SteamAPICall_t Call)
	{
		ReadFindResult.Set(Call, this, &FOnlineLeaderboardsExtendedSteamCallbacks::OnReadLeaderboardFound);
	}

	void TrackReadDownload(SteamAPICall_t Call)
	{
		ReadDownloadResult.Set(Call, this, &FOnlineLeaderboardsExtendedSteamCallbacks::OnReadScoresDownloaded);
	}

	void TrackWriteFind(SteamAPICall_t Call)
	{
		WriteFindResult.Set(Call, this, &FOnlineLeaderboardsExtendedSteamCallbacks::OnWriteLeaderboardFound);
	}

	void TrackWriteUpload(SteamAPICall_t Call)
	{
		WriteUploadResult.Set(Call, this, &FOnlineLeaderboardsExtendedSteamCallbacks::OnScoreUploaded);
	}

private:
	void OnReadLeaderboardFound(LeaderboardFindResult_t* Data, bool bIOFailure)
	{
		if (Data == nullptr)
		{
			return;
		}

		Owner->HandleReadLeaderboardFound(Data->m_hSteamLeaderboard, !bIOFailure && Data->m_bLeaderboardFound != 0);
	}

	void OnReadScoresDownloaded(LeaderboardScoresDownloaded_t* Data, bool bIOFailure)
	{
		if (Data == nullptr)
		{
			return;
		}

		Owner->HandleReadScoresDownloaded(Data->m_hSteamLeaderboardEntries, Data->m_cEntryCount, bIOFailure);
	}

	void OnWriteLeaderboardFound(LeaderboardFindResult_t* Data, bool bIOFailure)
	{
		if (Data == nullptr)
		{
			return;
		}

		Owner->HandleWriteLeaderboardFound(Data->m_hSteamLeaderboard, !bIOFailure && Data->m_bLeaderboardFound != 0);
	}

	void OnScoreUploaded(LeaderboardScoreUploaded_t* Data, bool bIOFailure)
	{
		if (Data == nullptr)
		{
			return;
		}

		Owner->HandleScoreUploaded(!bIOFailure && Data->m_bSuccess != 0, Data->m_nScore, Data->m_nGlobalRankNew);
	}

	/** Owner owns this holder, so a raw back pointer is safe. */
	FOnlineLeaderboardsExtendedSteam* Owner = nullptr;

	CCallResult<FOnlineLeaderboardsExtendedSteamCallbacks, LeaderboardFindResult_t> ReadFindResult;
	CCallResult<FOnlineLeaderboardsExtendedSteamCallbacks, LeaderboardScoresDownloaded_t> ReadDownloadResult;
	CCallResult<FOnlineLeaderboardsExtendedSteamCallbacks, LeaderboardFindResult_t> WriteFindResult;
	CCallResult<FOnlineLeaderboardsExtendedSteamCallbacks, LeaderboardScoreUploaded_t> WriteUploadResult;
};
#else
class FOnlineLeaderboardsExtendedSteamCallbacks
{
};
#endif // WITH_EXTENDEDSTEAM_SDK

FOnlineLeaderboardsExtendedSteam::FOnlineLeaderboardsExtendedSteam(FOnlineSubsystemExtendedSteam* InSubsystem)
	: Subsystem(InSubsystem)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (ExtendedSteamLeaderboards::IsSteamClientUp())
	{
		Callbacks = MakeShared<FOnlineLeaderboardsExtendedSteamCallbacks>(this);
	}
#endif
}

FOnlineLeaderboardsExtendedSteam::~FOnlineLeaderboardsExtendedSteam() = default;

uint64 FOnlineLeaderboardsExtendedSteam::GetLocalSteamId64() const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (ExtendedSteamLeaderboards::IsSteamClientUp() && SteamUser() != nullptr)
	{
		return SteamUser()->GetSteamID().ConvertToUint64();
	}
#endif
	return 0;
}

// -------------------------------------------------------------------------------------------
// Reads
// -------------------------------------------------------------------------------------------

bool FOnlineLeaderboardsExtendedSteam::StartRead(FOnlineLeaderboardReadRef& ReadObject, EReadMode Mode, int32 RangeStart, int32 RangeEnd, TArray<uint64>&& UserIds)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (CurrentRead.IsValid())
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("ReadLeaderboards: a read is already in flight ('%s', subsystem instance %s); sequence reads through OnLeaderboardReadComplete"),
			*CurrentRead->LeaderboardName, Subsystem != nullptr ? *Subsystem->GetInstanceName().ToString() : TEXT("<none>"));
		ReadObject->ReadState = EOnlineAsyncTaskState::Failed;
		return false;
	}

	ISteamUserStats* Stats = ExtendedSteamLeaderboards::IsSteamClientUp() && Callbacks.IsValid() ? SteamUserStats() : nullptr;
	if (Stats == nullptr || ReadObject->LeaderboardName.IsEmpty())
	{
		ReadObject->ReadState = EOnlineAsyncTaskState::Failed;
		return false;
	}

	// Steam is asked for the handle first (never creating here — reads of a leaderboard that
	// does not exist must fail); the download is issued from HandleReadLeaderboardFound.
	const SteamAPICall_t Call = Stats->FindLeaderboard(TCHAR_TO_UTF8(*ReadObject->LeaderboardName));
	if (Call == k_uAPICallInvalid)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("ReadLeaderboards: FindLeaderboard('%s') failed"), *ReadObject->LeaderboardName);
		ReadObject->ReadState = EOnlineAsyncTaskState::Failed;
		return false;
	}

	ReadObject->Rows.Empty();
	ReadObject->ReadState = EOnlineAsyncTaskState::InProgress;

	CurrentRead = ReadObject;
	CurrentReadMode = Mode;
	CurrentRangeStart = RangeStart;
	CurrentRangeEnd = RangeEnd;
	CurrentReadUserIds = MoveTemp(UserIds);

	Callbacks->TrackReadFind(Call);
	return true;
#else
	ReadObject->ReadState = EOnlineAsyncTaskState::Failed;
	return false;
#endif
}

bool FOnlineLeaderboardsExtendedSteam::ReadLeaderboards(const TArray<FUniqueNetIdRef>& Players, FOnlineLeaderboardReadRef& ReadObject)
{
	if (Players.Num() == 0)
	{
		// No explicit players: read the top of the global table (first DefaultGlobalReadCount rows).
		return StartRead(ReadObject, EReadMode::Global, 1, ExtendedSteamLeaderboards::DefaultGlobalReadCount, TArray<uint64>());
	}

	TArray<uint64> UserIds;
	UserIds.Reserve(Players.Num());
	for (const FUniqueNetIdRef& Player : Players)
	{
		const FUniqueNetIdExtendedSteamPtr SteamId = FUniqueNetIdExtendedSteam::Create(Player->ToString());
		if (SteamId.IsValid() && SteamId->IsValid())
		{
			UserIds.Add(SteamId->GetSteamId64());
		}
		else
		{
			UE_LOG(LogExtendedSteam, Warning, TEXT("ReadLeaderboards: skipping non-Steam player id %s"), *Player->ToDebugString());
		}
	}

	if (UserIds.Num() == 0)
	{
		ReadObject->ReadState = EOnlineAsyncTaskState::Failed;
		return false;
	}

	if (UserIds.Num() > ExtendedSteamLeaderboards::MaxUsersPerDownload)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("ReadLeaderboards: Steam caps per-user downloads at %d players; truncating %d requested"),
			ExtendedSteamLeaderboards::MaxUsersPerDownload, UserIds.Num());
		UserIds.SetNum(ExtendedSteamLeaderboards::MaxUsersPerDownload);
	}

	return StartRead(ReadObject, EReadMode::Users, 0, 0, MoveTemp(UserIds));
}

bool FOnlineLeaderboardsExtendedSteam::ReadLeaderboardsForFriends(int32 LocalUserNum, FOnlineLeaderboardReadRef& ReadObject)
{
	if (LocalUserNum != 0)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("ReadLeaderboardsForFriends: Steam supports a single local user; invalid LocalUserNum %d"), LocalUserNum);
		ReadObject->ReadState = EOnlineAsyncTaskState::Failed;
		return false;
	}

	// Friends requests return every friend row; the range parameters are ignored by Steam.
	return StartRead(ReadObject, EReadMode::Friends, 0, 0, TArray<uint64>());
}

bool FOnlineLeaderboardsExtendedSteam::ReadLeaderboardsAroundRank(int32 Rank, uint32 Range, FOnlineLeaderboardReadRef& ReadObject)
{
	const int32 RangeStart = FMath::Max(1, Rank - static_cast<int32>(Range));
	const int32 RangeEnd = Rank + static_cast<int32>(Range);
	return StartRead(ReadObject, EReadMode::Global, RangeStart, RangeEnd, TArray<uint64>());
}

bool FOnlineLeaderboardsExtendedSteam::ReadLeaderboardsAroundUser(FUniqueNetIdRef Player, uint32 Range, FOnlineLeaderboardReadRef& ReadObject)
{
	const FUniqueNetIdExtendedSteamPtr SteamId = FUniqueNetIdExtendedSteam::Create(Player->ToString());
	if (!SteamId.IsValid() || !SteamId->IsValid())
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("ReadLeaderboardsAroundUser: invalid player id %s"), *Player->ToDebugString());
		ReadObject->ReadState = EOnlineAsyncTaskState::Failed;
		return false;
	}

	if (SteamId->GetSteamId64() == GetLocalSteamId64())
	{
		// k_ELeaderboardDataRequestGlobalAroundUser: negative start, positive end around the local user.
		return StartRead(ReadObject, EReadMode::GlobalAroundLocalUser, -static_cast<int32>(Range), static_cast<int32>(Range), TArray<uint64>());
	}

	// Steam can only window around the CURRENT user; for anyone else degrade to fetching just
	// that player's row via the per-user download (rank is still returned, surrounding rows are not).
	UE_LOG(LogExtendedSteam, Log, TEXT("ReadLeaderboardsAroundUser: %s is not the local user; returning only that player's row (Steam cannot window around other users)"),
		*Player->ToDebugString());
	TArray<uint64> UserIds;
	UserIds.Add(SteamId->GetSteamId64());
	return StartRead(ReadObject, EReadMode::Users, 0, 0, MoveTemp(UserIds));
}

void FOnlineLeaderboardsExtendedSteam::HandleReadLeaderboardFound(uint64 LeaderboardHandle, bool bFound)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!CurrentRead.IsValid())
	{
		return;
	}

	ISteamUserStats* Stats = ExtendedSteamLeaderboards::IsSteamClientUp() ? SteamUserStats() : nullptr;
	if (!bFound || LeaderboardHandle == 0 || Stats == nullptr)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("ReadLeaderboards: leaderboard '%s' not found"), *CurrentRead->LeaderboardName);
		CompleteRead(false);
		return;
	}

	SteamAPICall_t Call = k_uAPICallInvalid;
	switch (CurrentReadMode)
	{
	case EReadMode::Global:
		Call = Stats->DownloadLeaderboardEntries(LeaderboardHandle, k_ELeaderboardDataRequestGlobal, CurrentRangeStart, CurrentRangeEnd);
		break;
	case EReadMode::Friends:
		Call = Stats->DownloadLeaderboardEntries(LeaderboardHandle, k_ELeaderboardDataRequestFriends, 0, 0);
		break;
	case EReadMode::GlobalAroundLocalUser:
		Call = Stats->DownloadLeaderboardEntries(LeaderboardHandle, k_ELeaderboardDataRequestGlobalAroundUser, CurrentRangeStart, CurrentRangeEnd);
		break;
	case EReadMode::Users:
	{
		TArray<CSteamID> SteamIds;
		SteamIds.Reserve(CurrentReadUserIds.Num());
		for (const uint64 UserId : CurrentReadUserIds)
		{
			SteamIds.Emplace(UserId);
		}
		Call = Stats->DownloadLeaderboardEntriesForUsers(LeaderboardHandle, SteamIds.GetData(), SteamIds.Num());
		break;
	}
	}

	if (Call == k_uAPICallInvalid)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("ReadLeaderboards: DownloadLeaderboardEntries failed for '%s'"), *CurrentRead->LeaderboardName);
		CompleteRead(false);
		return;
	}

	Callbacks->TrackReadDownload(Call);
#endif
}

void FOnlineLeaderboardsExtendedSteam::HandleReadScoresDownloaded(uint64 EntriesHandle, int32 EntryCount, bool bIOFailure)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!CurrentRead.IsValid())
	{
		return;
	}

	ISteamUserStats* Stats = ExtendedSteamLeaderboards::IsSteamClientUp() ? SteamUserStats() : nullptr;
	if (bIOFailure || Stats == nullptr)
	{
		CompleteRead(false);
		return;
	}

	// Steam leaderboards carry a single int32 score; expose it under the read object's sorted
	// column name so FindPlayerRecord/column lookups on the caller side keep working.
	const FString ColumnName = CurrentRead->SortedColumn.IsEmpty() ? FString(TEXT("Score")) : FString(CurrentRead->SortedColumn);

	CurrentRead->Rows.Reserve(EntryCount);
	for (int32 Index = 0; Index < EntryCount; ++Index)
	{
		LeaderboardEntry_t Entry;
		if (!Stats->GetDownloadedLeaderboardEntry(EntriesHandle, Index, &Entry, nullptr, 0))
		{
			continue;
		}

		const FString Nickname = SteamFriends() != nullptr
			? FString(UTF8_TO_TCHAR(SteamFriends()->GetFriendPersonaName(Entry.m_steamIDUser)))
			: FString();

		FOnlineStatsRow Row(Nickname, FUniqueNetIdExtendedSteam::Create(Entry.m_steamIDUser.ConvertToUint64()));
		Row.Rank = Entry.m_nGlobalRank;
		Row.Columns.Add(ColumnName, FVariantData(Entry.m_nScore));
		CurrentRead->Rows.Add(MoveTemp(Row));
	}

	CompleteRead(true);
#endif
}

void FOnlineLeaderboardsExtendedSteam::CompleteRead(bool bWasSuccessful)
{
	if (CurrentRead.IsValid())
	{
		CurrentRead->ReadState = bWasSuccessful ? EOnlineAsyncTaskState::Done : EOnlineAsyncTaskState::Failed;
	}

	CurrentRead.Reset();
	CurrentReadUserIds.Reset();

	TriggerOnLeaderboardReadCompleteDelegates(bWasSuccessful);
}

void FOnlineLeaderboardsExtendedSteam::FreeStats(FOnlineLeaderboardRead& ReadObject)
{
	// Nothing platform-specific to release: Steam frees downloaded entry data after the
	// GetDownloadedLeaderboardEntry pass in HandleReadScoresDownloaded, and rows own plain UE types.
}

// -------------------------------------------------------------------------------------------
// Writes
// -------------------------------------------------------------------------------------------

bool FOnlineLeaderboardsExtendedSteam::WriteLeaderboards(const FName& SessionName, const FUniqueNetId& Player, FOnlineLeaderboardWrite& WriteObject)
{
	// Steam can only upload scores for the signed-in user.
	const uint64 LocalSteamId64 = GetLocalSteamId64();
	if (LocalSteamId64 == 0 || !Player.IsValid() || Player != *FUniqueNetIdExtendedSteam::Create(LocalSteamId64))
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("WriteLeaderboards: only the local Steam user is supported (got %s)"), *Player.ToDebugString());
		return false;
	}

	int32 NumQueued = 0;

	if (WriteObject.LeaderboardNames.Num() > 0)
	{
		// Engine convention: the leaderboards listed on the write object are rated by RatedStat
		// (falling back to the only stat when RatedStat is unset or missing).
		const FVariantData* RatedValue = WriteObject.FindStatByName(WriteObject.RatedStat);
		if (RatedValue == nullptr && WriteObject.Properties.Num() == 1)
		{
			for (const TPair<FString, FVariantData>& Stat : WriteObject.Properties)
			{
				RatedValue = &Stat.Value;
			}
		}

		int32 Score = 0;
		if (RatedValue == nullptr || !ExtendedSteamLeaderboards::ToInt32Score(*RatedValue, Score))
		{
			UE_LOG(LogExtendedSteam, Warning, TEXT("WriteLeaderboards: no usable int score for rated stat '%s'"), *WriteObject.RatedStat);
			return false;
		}

		for (const FString& LeaderboardName : WriteObject.LeaderboardNames)
		{
			FPendingWrite& Pending = WriteQueue.AddDefaulted_GetRef();
			Pending.LeaderboardName = LeaderboardName;
			Pending.SortMethod = WriteObject.SortMethod;
			Pending.DisplayFormat = WriteObject.DisplayFormat;
			Pending.UpdateMethod = WriteObject.UpdateMethod;
			Pending.Score = Score;
			++NumQueued;
		}
	}
	else
	{
		// No leaderboard names: treat each stat as its own leaderboard (stat name = board name).
		for (const TPair<FString, FVariantData>& Stat : WriteObject.Properties)
		{
			int32 Score = 0;
			if (!ExtendedSteamLeaderboards::ToInt32Score(Stat.Value, Score))
			{
				UE_LOG(LogExtendedSteam, Warning, TEXT("WriteLeaderboards: stat '%s' has no int-convertible value; skipped"), *Stat.Key);
				continue;
			}

			FPendingWrite& Pending = WriteQueue.AddDefaulted_GetRef();
			Pending.LeaderboardName = Stat.Key;
			Pending.SortMethod = WriteObject.SortMethod;
			Pending.DisplayFormat = WriteObject.DisplayFormat;
			Pending.UpdateMethod = WriteObject.UpdateMethod;
			Pending.Score = Score;
			++NumQueued;
		}
	}

	if (NumQueued == 0)
	{
		return false;
	}

	StartNextWrite();
	return true;
}

void FOnlineLeaderboardsExtendedSteam::StartNextWrite()
{
#if WITH_EXTENDEDSTEAM_SDK
	if (bWriteInFlight || WriteQueue.Num() == 0)
	{
		return;
	}

	ISteamUserStats* Stats = ExtendedSteamLeaderboards::IsSteamClientUp() && Callbacks.IsValid() ? SteamUserStats() : nullptr;
	if (Stats == nullptr)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("WriteLeaderboards: Steam unavailable; dropping %d queued upload(s)"), WriteQueue.Num());
		WriteQueue.Reset();
		return;
	}

	CurrentWrite = WriteQueue[0];
	WriteQueue.RemoveAt(0);
	bWriteInFlight = true;

	// FindOrCreateLeaderboard so first-time writes lazily create the board with the write
	// object's sort/display settings (existing boards keep their configured settings).
	const SteamAPICall_t Call = Stats->FindOrCreateLeaderboard(
		TCHAR_TO_UTF8(*CurrentWrite.LeaderboardName),
		ExtendedSteamLeaderboards::ToSteamSortMethod(CurrentWrite.SortMethod),
		ExtendedSteamLeaderboards::ToSteamDisplayType(CurrentWrite.DisplayFormat));
	if (Call == k_uAPICallInvalid)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("WriteLeaderboards: FindOrCreateLeaderboard('%s') failed"), *CurrentWrite.LeaderboardName);
		bWriteInFlight = false;
		StartNextWrite();
		return;
	}

	Callbacks->TrackWriteFind(Call);
#else
	WriteQueue.Reset();
#endif
}

void FOnlineLeaderboardsExtendedSteam::HandleWriteLeaderboardFound(uint64 LeaderboardHandle, bool bFound)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!bWriteInFlight)
	{
		return;
	}

	ISteamUserStats* Stats = ExtendedSteamLeaderboards::IsSteamClientUp() ? SteamUserStats() : nullptr;
	if (!bFound || LeaderboardHandle == 0 || Stats == nullptr)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("WriteLeaderboards: could not find or create leaderboard '%s'"), *CurrentWrite.LeaderboardName);
		bWriteInFlight = false;
		StartNextWrite();
		return;
	}

	const SteamAPICall_t Call = Stats->UploadLeaderboardScore(
		LeaderboardHandle,
		ExtendedSteamLeaderboards::ToSteamUploadMethod(CurrentWrite.UpdateMethod),
		CurrentWrite.Score,
		nullptr, 0);
	if (Call == k_uAPICallInvalid)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("WriteLeaderboards: UploadLeaderboardScore('%s') failed"), *CurrentWrite.LeaderboardName);
		bWriteInFlight = false;
		StartNextWrite();
		return;
	}

	Callbacks->TrackWriteUpload(Call);
#endif
}

void FOnlineLeaderboardsExtendedSteam::HandleScoreUploaded(bool bSuccess, int32 Score, int32 NewGlobalRank)
{
	if (!bWriteInFlight)
	{
		return;
	}

	if (bSuccess)
	{
		UE_LOG(LogExtendedSteam, Verbose, TEXT("WriteLeaderboards: uploaded %d to '%s' (new global rank %d)"),
			Score, *CurrentWrite.LeaderboardName, NewGlobalRank);
	}
	else
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("WriteLeaderboards: upload of %d to '%s' failed"), Score, *CurrentWrite.LeaderboardName);
	}

	bWriteInFlight = false;
	StartNextWrite();
}

bool FOnlineLeaderboardsExtendedSteam::FlushLeaderboards(const FName& SessionName)
{
	// Steam commits every UploadLeaderboardScore immediately — there is no client-side cache to
	// flush, so this reports success synchronously. Queued uploads still in the sequential write
	// queue commit on their own as the queue drains.
	TriggerOnLeaderboardFlushCompleteDelegates(SessionName, true);
	return true;
}

bool FOnlineLeaderboardsExtendedSteam::WriteOnlinePlayerRatings(const FName& SessionName, int32 LeaderboardId, const TArray<FOnlinePlayerScore>& PlayerScores)
{
	// Not supported: Steam has no skill-rating write API, and the engine's FOnlinePlayerScore is
	// itself an empty placeholder type.
	UE_LOG(LogExtendedSteam, Verbose, TEXT("WriteOnlinePlayerRatings: not supported on Steam"));
	return false;
}
