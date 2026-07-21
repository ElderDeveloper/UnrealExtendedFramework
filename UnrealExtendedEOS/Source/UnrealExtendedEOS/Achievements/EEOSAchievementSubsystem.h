// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EEOSSubsystem.h"
#include "EEOSAchievementSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSAchievementsQueried, bool, bSuccess, const TArray<FEEOSAchievement>&, Achievements);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSAchievementUnlocked, bool, bSuccess, const FString&, AchievementId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnEOSAchievementProgressUpdated, const FString&, AchievementId, float, Progress, bool, bUnlocked);

/**
 * Manages EOS achievements — query, progress tracking, and unlocking.
 *
 * Progress model: the cache (CachedAchievements) is the BACKEND state merged with LOCAL-ONLY
 * partial progress. EOS has no client API for partial progress — the backend derives it from
 * Dev Portal stat thresholds — so SetAchievementProgress/IncrementAchievementProgress below
 * 1.0 record progress locally (LocalPartialProgress). Local partials survive QueryAchievements:
 * the cache rebuild takes max(backend progress, local partial) for achievements the backend
 * still reports locked. A confirmed unlock (UnlockAchievement or SetAchievementProgress(1.0)
 * whose backend write succeeds) updates the cache immediately and drops the local partial.
 * ResetAchievement clears both the cached entry and the local partial (local-only — the EOS
 * backend is forward-only).
 */
UCLASS()
class UNREALEXTENDEDEOS_API UEEOSAchievementSubsystem : public UEEOSSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Actions ──────────────────────────────────────────────────────────────
	// All async actions return true when the request was started (the result arrives on
	// the corresponding delegate) and false when it could not be — a failure was already
	// broadcast in that case.

	/** Query all achievements and their progress for the local user */
	UFUNCTION(BlueprintCallable, Category = "EOS|Achievements")
	bool QueryAchievements();

	/** Unlock (complete) an achievement by ID */
	UFUNCTION(BlueprintCallable, Category = "EOS|Achievements")
	bool UnlockAchievement(const FString& AchievementId);

	/**
	 * Set progress on an achievement (0.0 to 1.0).
	 * Progress below 1.0 is tracked LOCALLY only — the EOS backend derives partial progress from
	 * stat thresholds configured in the Dev Portal (ingest stats via EEOSStatsSubsystem).
	 * Reaching 1.0 performs a real (permanent) backend unlock.
	 * Returns true when the local update was applied or the backend unlock was started.
	 */
	UFUNCTION(BlueprintCallable, Category = "EOS|Achievements")
	bool SetAchievementProgress(const FString& AchievementId, float Progress);

	/** Increment achievement progress by a specific amount (same local-only semantics as SetAchievementProgress) */
	UFUNCTION(BlueprintCallable, Category = "EOS|Achievements")
	bool IncrementAchievementProgress(const FString& AchievementId, float IncrementAmount);

	/** Reset achievement progress (if supported) */
	UFUNCTION(BlueprintCallable, Category = "EOS|Achievements")
	void ResetAchievement(const FString& AchievementId);

	// ── Queries ──────────────────────────────────────────────────────────────

	/** Get the cached achievements from the last query */
	UFUNCTION(BlueprintPure, Category = "EOS|Achievements")
	TArray<FEEOSAchievement> GetAchievements() const;

	/** Get a specific achievement by ID (from cache) */
	UFUNCTION(BlueprintPure, Category = "EOS|Achievements")
	bool GetAchievementById(const FString& AchievementId, FEEOSAchievement& OutAchievement) const;

	/** Get the total unlock progress (0.0 to 1.0) */
	UFUNCTION(BlueprintPure, Category = "EOS|Achievements")
	float GetOverallProgress() const;

	/** Get the count of unlocked achievements */
	UFUNCTION(BlueprintPure, Category = "EOS|Achievements")
	int32 GetUnlockedCount() const;

	/** Get total achievement count */
	UFUNCTION(BlueprintPure, Category = "EOS|Achievements")
	int32 GetTotalCount() const;

	/** Check if a specific achievement is unlocked */
	UFUNCTION(BlueprintPure, Category = "EOS|Achievements")
	bool IsAchievementUnlocked(const FString& AchievementId) const;

	// ── Delegates ────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "EOS|Achievements")
	FOnEOSAchievementsQueried OnAchievementsQueried;

	UPROPERTY(BlueprintAssignable, Category = "EOS|Achievements")
	FOnEOSAchievementUnlocked OnAchievementUnlocked;

	UPROPERTY(BlueprintAssignable, Category = "EOS|Achievements")
	FOnEOSAchievementProgressUpdated OnAchievementProgressUpdated;

private:

	TArray<FEEOSAchievement> CachedAchievements;

	/** Local-only partial progress (AchievementId → 0..1). See the class comment: survives
	 *  QueryAchievements rebuilds (merged back as max(backend, local partial) on locked
	 *  achievements); dropped per-id on confirmed unlock or ResetAchievement. */
	TMap<FString, float> LocalPartialProgress;

	/** Marks an achievement unlocked in the cache (adds the entry if missing) and drops its
	 *  local partial — called after a CONFIRMED backend unlock so IsAchievementUnlocked()
	 *  reflects it without a re-query. */
	void MarkAchievementUnlockedInCache(const FString& AchievementId);

	void HandleQueryAchievementsComplete(const FUniqueNetId& PlayerId, const bool bWasSuccessful);
};
