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
	// All async actions return true when the request was handed to the SDK (the result
	// arrives on the corresponding delegate) and false when it could not be started —
	// a failure was already broadcast in that case.

	/**
	 * Ingest (add) a stat value for the local user. Goes through the raw EOS SDK
	 * (EOS_Stats_IngestStat) so OnStatIngested carries the REAL async backend result —
	 * the engine's OSS stats path reports success synchronously without waiting.
	 */
	UFUNCTION(BlueprintCallable, Category = "EOS|Stats")
	bool IngestStat(const FString& StatName, int32 Amount);

	/** Ingest multiple stats at once in a single SDK call (same honest-result semantics as IngestStat) */
	UFUNCTION(BlueprintCallable, Category = "EOS|Stats")
	bool IngestStatsBatch(const TMap<FString, int32>& StatsToIngest);

	/**
	 * Query stats for a specific user. UserId may be the composite
	 * "<EpicAccountId>|<ProductUserId>" (as returned by net id ToString()) or a bare
	 * Product User ID. The user must have a Product User ID half — EOS stats are keyed
	 * by PUID and the engine silently drops users without one.
	 */
	UFUNCTION(BlueprintCallable, Category = "EOS|Stats")
	bool QueryStats(const FString& UserId, const TArray<FString>& StatNames);

	/** Query stats for the local user */
	UFUNCTION(BlueprintCallable, Category = "EOS|Stats")
	bool QueryLocalStats(const TArray<FString>& StatNames);

	// ── Queries ──────────────────────────────────────────────────────────────
	// The cache holds ONLY the stats of the user targeted by the last query (the
	// engine's completion payload is its interface-wide accumulated cache of every
	// user ever queried — it is filtered to the queried user before caching).

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

	/** Shared raw-SDK ingest used by IngestStat and IngestStatsBatch — one
	 *  EOS_Stats_IngestStat call carrying every entry, honest async result. */
	bool IngestStatsInternal(const TCHAR* FunctionName, const TMap<FString, int32>& StatsToIngest);
};
