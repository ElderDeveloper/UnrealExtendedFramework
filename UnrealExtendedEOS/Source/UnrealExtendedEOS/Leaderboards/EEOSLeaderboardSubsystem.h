// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EEOSSubsystem.h"
#include "Interfaces/OnlineLeaderboardInterface.h"
#include "EEOSLeaderboardSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSLeaderboardQueried, bool, bSuccess, const TArray<FEEOSLeaderboardEntry>&, Entries);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSScoreUploaded, bool, bSuccess, const FString&, LeaderboardId);

/**
 * Manages EOS leaderboard queries — global, friends, and range-based.
 * Supports score uploads and pagination.
 */
UCLASS()
class UNREALEXTENDEDEOS_API UEEOSLeaderboardSubsystem : public UEEOSSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Actions ──────────────────────────────────────────────────────────────
	// All async actions return true when the request was started (the result arrives on
	// the corresponding delegate) and false when it could not be. Pre-flight failures
	// broadcast a failure; a query rejected because another leaderboard query is already
	// in flight is log-only — NO broadcast, so the in-flight query's waiters never see a
	// foreign failure on the shared OnLeaderboardQueried delegate.

	/**
	 * Query the global top entries of a leaderboard (ranks 1..MaxEntries).
	 * EOS serves at most the top 1000 rankings; MaxEntries is clamped to [1, 1000].
	 */
	UFUNCTION(BlueprintCallable, Category = "EOS|Leaderboards")
	bool QueryLeaderboard(const FString& LeaderboardId, int32 MaxEntries = 100);

	/**
	 * Query a friends-only leaderboard.
	 * Note: friend scores are fetched per-user by STAT name — pass the name of the stat
	 * backing the leaderboard (they are usually identical). Ranks are re-computed locally
	 * among the returned users (1..N), not global ranks.
	 */
	UFUNCTION(BlueprintCallable, Category = "EOS|Leaderboards")
	bool QueryFriendsLeaderboard(const FString& LeaderboardId);

	/**
	 * Query the local player's own entry on a leaderboard.
	 * Note: fetched by STAT name — pass the name of the stat backing the leaderboard.
	 * The EOS OSS ignores Range for user queries; only the local player's row is returned.
	 */
	UFUNCTION(BlueprintCallable, Category = "EOS|Leaderboards")
	bool QueryLeaderboardAroundPlayer(const FString& LeaderboardId, int32 Range = 5);

	/** Query leaderboard by 1-based rank range, inclusive (e.g. 1..10 for the top 10). Capped at rank 1000 by EOS. */
	UFUNCTION(BlueprintCallable, Category = "EOS|Leaderboards")
	bool QueryLeaderboardByRange(const FString& LeaderboardId, int32 StartRank, int32 EndRank);

	/**
	 * Upload a score to a leaderboard. EOS leaderboards are stat-backed: this ingests the
	 * score into the backing STAT (pass its name; the Dev Portal aggregates ingests into the
	 * leaderboard per its Min/Max/Latest/Sum configuration). Broadcasts OnScoreUploaded with
	 * the real ingest result.
	 */
	UFUNCTION(BlueprintCallable, Category = "EOS|Leaderboards")
	bool UploadScore(const FString& LeaderboardId, int32 Score);

	// ── Queries ──────────────────────────────────────────────────────────────

	/** Get the cached leaderboard entries from the last query */
	UFUNCTION(BlueprintPure, Category = "EOS|Leaderboards")
	TArray<FEEOSLeaderboardEntry> GetLeaderboardEntries() const;

	/** Get the local player's rank (from cache, -1 if not found) */
	UFUNCTION(BlueprintPure, Category = "EOS|Leaderboards")
	int32 GetLocalPlayerRank() const;

	/** Get the local player's score (from cache, 0 if not found) */
	UFUNCTION(BlueprintPure, Category = "EOS|Leaderboards")
	int32 GetLocalPlayerScore() const;

	/** Get the entry count from the last query */
	UFUNCTION(BlueprintPure, Category = "EOS|Leaderboards")
	int32 GetEntryCount() const;

	// ── Delegates ────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "EOS|Leaderboards")
	FOnEOSLeaderboardQueried OnLeaderboardQueried;

	UPROPERTY(BlueprintAssignable, Category = "EOS|Leaderboards")
	FOnEOSScoreUploaded OnScoreUploaded;

private:

	TArray<FEEOSLeaderboardEntry> CachedEntries;

	/** Cached read object for extracting leaderboard rows in the callback.
	 *  Doubles as the in-flight guard: a new query is rejected while this is valid — set at
	 *  query start, reset only when our completion is consumed (or on Deinitialize). Its
	 *  ReadState is deliberately NOT part of the guard: the engine's deferred-failure paths
	 *  (zero-friends ReadLeaderboardsForFriends, rank > 1000) fire the completion next tick
	 *  without ever setting the state past NotStarted. */
	FOnlineLeaderboardReadPtr CachedLeaderboardRead;

	/** Handle for our binding on the interface-wide OnLeaderboardReadCompleteDelegates —
	 *  cleared (only this handle, never RemoveAll) when our read completes. */
	FDelegateHandle LeaderboardReadCompleteHandle;

	/** Inclusive 1-based rank window the current query asked for (-1 = no trimming).
	 *  Rank-based reads over-fetch by up to one record because the engine's AroundRank
	 *  takes a (center, range) window — the completion handler trims rows back to this. */
	int32 PendingRankMin = -1;
	int32 PendingRankMax = -1;

	/** Rejects the new query if a leaderboard read is already in flight. Log-only — never
	 *  broadcasts, so the in-flight query's waiters are not misled on the shared delegate.
	 *  Returns true if rejected. */
	bool RejectIfQueryInFlight(const TCHAR* FunctionName, const FString& LeaderboardId);

	/** Builds a read object with LeaderboardName, SortedColumn, and ColumnMetadata populated —
	 *  the EOS OSS requests zero stats (and can never return scores) without the metadata. */
	FOnlineLeaderboardReadRef MakeLeaderboardReadObject(const FString& LeaderboardId) const;

	/** Binds HandleLeaderboardReadComplete on the interface delegate list and stores the handle. */
	void BindLeaderboardReadDelegate(IOnlineLeaderboardsPtr LeaderboardInterface);

	void HandleLeaderboardReadComplete(bool bWasSuccessful);
};
