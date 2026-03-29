// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EPFSubsystem.h"
#include "EPFStatsSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEPFStatsReceived, const FEPFResult&, Result, const TArray<FEPFStatistic>&, Stats);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFStatsUpdated, const FEPFResult&, Result);

/**
 * Manages PlayFab player statistics — update and query stats.
 */
UCLASS()
class UNREALEXTENDEDPLAYFAB_API UEPFStatsSubsystem : public UEPFSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Actions ──────────────────────────────────────────────────────────────

	/** Update one or more player statistics */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Stats")
	void UpdateStats(const TMap<FString, int32>& StatsToUpdate);

	/** Update a single statistic */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Stats")
	void UpdateStat(const FString& StatName, int32 Value);

	/** Query player statistics (specific names) */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Stats")
	void GetStats(const TArray<FString>& StatNames);

	/** Query all player statistics */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Stats")
	void GetAllStats();

	// ── Queries ──────────────────────────────────────────────────────────────

	/** Get cached stats from last query */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Stats")
	TArray<FEPFStatistic> GetCachedStats() const;

	/**
	 * Get a cached stat value. Returns -1 if not found — but -1 is also a valid
	 * stat value. Use HasStat() first, or use GetCachedStatValueSafe() instead.
	 */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Stats")
	int32 GetCachedStatValue(const FString& StatName) const;

	/**
	 * Get a cached stat value with an explicit "found" output.
	 * Prefer this over GetCachedStatValue() to avoid the -1 ambiguity.
	 */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Stats")
	int32 GetCachedStatValueSafe(const FString& StatName, bool& bFound) const;

	/** Check if a stat exists in the cache */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Stats")
	bool HasStat(const FString& StatName) const;

	// ── Delegates ────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Stats")
	FOnEPFStatsReceived OnStatsReceived;

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Stats")
	FOnEPFStatsUpdated OnStatsUpdated;

private:

	TArray<FEPFStatistic> CachedStats;
};
