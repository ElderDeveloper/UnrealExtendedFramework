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

	/** Query a global leaderboard */
	UFUNCTION(BlueprintCallable, Category = "EOS|Leaderboards")
	void QueryLeaderboard(const FString& LeaderboardId);

	/** Query a friends-only leaderboard */
	UFUNCTION(BlueprintCallable, Category = "EOS|Leaderboards")
	void QueryFriendsLeaderboard(const FString& LeaderboardId);

	/** Query leaderboard around the local player's rank */
	UFUNCTION(BlueprintCallable, Category = "EOS|Leaderboards")
	void QueryLeaderboardAroundPlayer(const FString& LeaderboardId, int32 Range = 5);

	/** Query leaderboard by rank range (e.g., top 10) */
	UFUNCTION(BlueprintCallable, Category = "EOS|Leaderboards")
	void QueryLeaderboardByRange(const FString& LeaderboardId, int32 StartRank, int32 EndRank);

	/** Upload a score to a leaderboard */
	UFUNCTION(BlueprintCallable, Category = "EOS|Leaderboards")
	void UploadScore(const FString& LeaderboardId, int32 Score);

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

	/** Cached read object for extracting leaderboard rows in the callback */
	FOnlineLeaderboardReadPtr CachedLeaderboardRead;

	void HandleLeaderboardReadComplete(bool bWasSuccessful);
};
