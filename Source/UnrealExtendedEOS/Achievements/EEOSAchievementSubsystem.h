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
 */
UCLASS()
class UNREALEXTENDEDEOS_API UEEOSAchievementSubsystem : public UEEOSSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Actions ──────────────────────────────────────────────────────────────

	/** Query all achievements and their progress for the local user */
	UFUNCTION(BlueprintCallable, Category = "EOS|Achievements")
	void QueryAchievements();

	/** Unlock (complete) an achievement by ID */
	UFUNCTION(BlueprintCallable, Category = "EOS|Achievements")
	void UnlockAchievement(const FString& AchievementId);

	/** Set progress on an achievement (0.0 to 1.0) */
	UFUNCTION(BlueprintCallable, Category = "EOS|Achievements")
	void SetAchievementProgress(const FString& AchievementId, float Progress);

	/** Increment achievement progress by a specific amount */
	UFUNCTION(BlueprintCallable, Category = "EOS|Achievements")
	void IncrementAchievementProgress(const FString& AchievementId, float IncrementAmount);

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

	void HandleQueryAchievementsComplete(const FUniqueNetId& PlayerId, const bool bWasSuccessful);
};
