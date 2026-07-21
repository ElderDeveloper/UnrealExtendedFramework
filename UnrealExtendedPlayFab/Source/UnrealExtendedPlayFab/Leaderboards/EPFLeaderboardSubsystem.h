// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EPFSubsystem.h"
#include "EPFLeaderboardSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEPFLeaderboardReceived, const FEPFResult&, Result, const TArray<FEPFLeaderboardEntry>&, Entries);

/**
 * Manages PlayFab Progression leaderboard queries.
 */
UCLASS()
class UNREALEXTENDEDPLAYFAB_API UEPFLeaderboardSubsystem : public UEPFSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Actions ──────────────────────────────────────────────────────────────

	/** Query a global leaderboard by leaderboard definition name */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Leaderboards")
	void GetLeaderboard(const FString& StatisticName, int32 StartPosition = 0, int32 MaxResultsCount = 10);

	/** Query a leaderboard centered around the authenticated entity */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Leaderboards")
	void GetLeaderboardAroundPlayer(const FString& StatisticName, int32 MaxResultsCount = 10);

	/** Query the authenticated entity's friends on a leaderboard */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Leaderboards")
	void GetFriendsLeaderboard(const FString& StatisticName, int32 MaxResultsCount = 10);

	// ── Queries ──────────────────────────────────────────────────────────────

	/** Get cached leaderboard entries from the last query */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Leaderboards")
	TArray<FEPFLeaderboardEntry> GetCachedEntries() const;

	/** Get the local player's position from cache (-1 if not found) */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Leaderboards")
	int32 GetLocalPlayerPosition() const;

	/** Get the entry count from the last query */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Leaderboards")
	int32 GetEntryCount() const;

	// ── Delegates ────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Leaderboards")
	FOnEPFLeaderboardReceived OnLeaderboardReceived;

private:

	TArray<FEPFLeaderboardEntry> CachedEntries;

	/** Parse leaderboard entries from a PlayFab response */
	void ParseLeaderboardResponse(const FEPFResult& Result, TSharedPtr<FJsonObject> Response);
};
