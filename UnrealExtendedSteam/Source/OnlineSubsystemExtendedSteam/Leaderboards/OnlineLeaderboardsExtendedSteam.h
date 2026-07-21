// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/OnlineLeaderboardInterface.h"
#include "OnlineStats.h"
#include "Core/OnlineSubsystemExtendedSteamPackage.h"
#include "Identity/OnlineIdentityExtendedSteam.h"

class FOnlineSubsystemExtendedSteam;
class FOnlineLeaderboardsExtendedSteamCallbacks;

/**
 * Leaderboards interface backed by ISteamUserStats leaderboards.
 *
 * Read model (ONE read in flight): every ReadLeaderboards* variant resolves the leaderboard
 * handle with FindLeaderboard (LeaderboardFindResult_t), then downloads rows with
 * DownloadLeaderboardEntries / DownloadLeaderboardEntriesForUsers (LeaderboardScoresDownloaded_t)
 * using the data-request mode matching the call variant. Each row becomes an FOnlineStatsRow
 * with the entry's SteamID64 as PlayerId, the Steam global rank, and a single int32 column named
 * after ReadObject->SortedColumn (falling back to "Score") holding the Steam score — Steam
 * leaderboards store exactly one int32 per entry, so extra ColumnMetadata entries are not
 * populated. Starting a read while one is pending returns false without touching the pending read.
 *
 * Write model: WriteLeaderboards enqueues one upload per target leaderboard and returns true if
 * anything was queued. Uploads drain strictly sequentially (Steam allows one outstanding
 * leaderboard operation): FindOrCreateLeaderboard with sort/display derived from the write
 * object, then UploadLeaderboardScore with the update method mapped from
 * ELeaderboardUpdateMethod (KeepBest/Force). Steam commits each upload immediately, so
 * FlushLeaderboards has nothing to flush and completes with success synchronously.
 */
class FOnlineLeaderboardsExtendedSteam : public IOnlineLeaderboards
{
public:
	explicit FOnlineLeaderboardsExtendedSteam(FOnlineSubsystemExtendedSteam* InSubsystem);
	virtual ~FOnlineLeaderboardsExtendedSteam();

	//~ Begin IOnlineLeaderboards
	virtual bool ReadLeaderboards(const TArray<FUniqueNetIdRef>& Players, FOnlineLeaderboardReadRef& ReadObject) override;
	virtual bool ReadLeaderboardsForFriends(int32 LocalUserNum, FOnlineLeaderboardReadRef& ReadObject) override;
	virtual bool ReadLeaderboardsAroundRank(int32 Rank, uint32 Range, FOnlineLeaderboardReadRef& ReadObject) override;
	virtual bool ReadLeaderboardsAroundUser(FUniqueNetIdRef Player, uint32 Range, FOnlineLeaderboardReadRef& ReadObject) override;
	virtual void FreeStats(FOnlineLeaderboardRead& ReadObject) override;
	virtual bool WriteLeaderboards(const FName& SessionName, const FUniqueNetId& Player, FOnlineLeaderboardWrite& WriteObject) override;
	virtual bool FlushLeaderboards(const FName& SessionName) override;
	virtual bool WriteOnlinePlayerRatings(const FName& SessionName, int32 LeaderboardId, const TArray<FOnlinePlayerScore>& PlayerScores) override;
	//~ End IOnlineLeaderboards

private:
	friend class FOnlineLeaderboardsExtendedSteamCallbacks;

	/** Which DownloadLeaderboardEntries mode a pending read maps to. */
	enum class EReadMode : uint8
	{
		/** Global rows for a rank window (ReadLeaderboardsAroundRank, and ReadLeaderboards with no players). */
		Global,
		/** All rows belonging to friends of the local user. */
		Friends,
		/** Rows around the local user's own rank. */
		GlobalAroundLocalUser,
		/** Rows for an explicit set of SteamIDs (DownloadLeaderboardEntriesForUsers). */
		Users
	};

	/** A single queued leaderboard score upload. */
	struct FPendingWrite
	{
		FString LeaderboardName;
		ELeaderboardSort::Type SortMethod = ELeaderboardSort::None;
		ELeaderboardFormat::Type DisplayFormat = ELeaderboardFormat::Number;
		ELeaderboardUpdateMethod::Type UpdateMethod = ELeaderboardUpdateMethod::KeepBest;
		int32 Score = 0;
	};

	/** Shared entry point of all read variants; starts FindLeaderboard when no read is in flight. */
	bool StartRead(FOnlineLeaderboardReadRef& ReadObject, EReadMode Mode, int32 RangeStart, int32 RangeEnd, TArray<uint64>&& UserIds);

	/** Finishes the in-flight read: sets ReadState, clears state, fires OnLeaderboardReadComplete. */
	void CompleteRead(bool bWasSuccessful);

	/** LeaderboardFindResult_t for the in-flight read (forwarded by the callback holder). */
	void HandleReadLeaderboardFound(uint64 LeaderboardHandle, bool bFound);

	/** LeaderboardScoresDownloaded_t for the in-flight read (forwarded by the callback holder). */
	void HandleReadScoresDownloaded(uint64 EntriesHandle, int32 EntryCount, bool bIOFailure);

	/** Starts the next queued write when none is in flight. */
	void StartNextWrite();

	/** LeaderboardFindResult_t for the in-flight write (forwarded by the callback holder). */
	void HandleWriteLeaderboardFound(uint64 LeaderboardHandle, bool bFound);

	/** LeaderboardScoreUploaded_t for the in-flight write (forwarded by the callback holder). */
	void HandleScoreUploaded(bool bSuccess, int32 Score, int32 NewGlobalRank);

	/** SteamID64 of the local user; 0 when the Steam client API is unavailable. */
	uint64 GetLocalSteamId64() const;

	/** Owning subsystem (owns this object; outlives it). */
	FOnlineSubsystemExtendedSteam* Subsystem = nullptr;

	/** Native Steam call-result listeners (SDK-gated, defined in the cpp). */
	TSharedPtr<FOnlineLeaderboardsExtendedSteamCallbacks> Callbacks;

	// ---- In-flight read state (one read at a time) ----
	FOnlineLeaderboardReadPtr CurrentRead;
	EReadMode CurrentReadMode = EReadMode::Global;
	int32 CurrentRangeStart = 0;
	int32 CurrentRangeEnd = 0;
	TArray<uint64> CurrentReadUserIds;

	// ---- Sequential write queue (one upload at a time) ----
	TArray<FPendingWrite> WriteQueue;
	bool bWriteInFlight = false;
	FPendingWrite CurrentWrite;
};

typedef TSharedPtr<FOnlineLeaderboardsExtendedSteam, ESPMode::ThreadSafe> FOnlineLeaderboardsExtendedSteamPtr;
