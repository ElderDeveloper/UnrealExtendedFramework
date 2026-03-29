// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EEOSSubsystem.h"
#include "EEOSStatsSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSStatsQueried, bool, bSuccess, const TArray<FEEOSStat>&, Stats);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSStatIngested, bool, bSuccess);

/**
 * Manages player statistics through EOS — ingest and query stats.
 */
UCLASS()
class UNREALEXTENDEDEOS_API UEEOSStatsSubsystem : public UEEOSSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Actions ──────────────────────────────────────────────────────────────

	/** Ingest (add) a stat value for the local user */
	UFUNCTION(BlueprintCallable, Category = "EOS|Stats")
	void IngestStat(const FString& StatName, int32 Amount);

	/** Ingest multiple stats at once */
	UFUNCTION(BlueprintCallable, Category = "EOS|Stats")
	void IngestStatsBatch(const TMap<FString, int32>& StatsToIngest);

	/** Query stats for a specific user */
	UFUNCTION(BlueprintCallable, Category = "EOS|Stats")
	void QueryStats(const FString& UserId, const TArray<FString>& StatNames);

	/** Query stats for the local user */
	UFUNCTION(BlueprintCallable, Category = "EOS|Stats")
	void QueryLocalStats(const TArray<FString>& StatNames);

	// ── Queries ──────────────────────────────────────────────────────────────

	/** Get the cached stats from the last query */
	UFUNCTION(BlueprintPure, Category = "EOS|Stats")
	TArray<FEEOSStat> GetCachedStats() const;

	/** Get a specific stat value by name from cache (returns 0 if not found) */
	UFUNCTION(BlueprintPure, Category = "EOS|Stats")
	int32 GetStatValue(const FString& StatName) const;

	/** Check if a stat exists in the cache */
	UFUNCTION(BlueprintPure, Category = "EOS|Stats")
	bool HasStat(const FString& StatName) const;

	// ── Delegates ────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "EOS|Stats")
	FOnEOSStatsQueried OnStatsQueried;

	UPROPERTY(BlueprintAssignable, Category = "EOS|Stats")
	FOnEOSStatIngested OnStatIngested;

private:

	TArray<FEEOSStat> CachedStats;
};
