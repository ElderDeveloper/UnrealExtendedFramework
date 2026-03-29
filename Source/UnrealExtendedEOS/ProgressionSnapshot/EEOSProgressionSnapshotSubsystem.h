// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EEOSSubsystem.h"
#include "EEOSProgressionSnapshotSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSSnapshotComplete, bool, bSuccess, int32, SnapshotId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSSnapshotDeleted, bool, bSuccess);

/**
 * Cloud progression snapshots — save game state checkpoints to EOS servers.
 * Snapshots capture key-value pairs of game progress data (level, score, inventory).
 * 
 * Flow: BeginSnapshot → AddProgressData (multiple) → EndSnapshot → submit to cloud
 * 
 * Note: Full implementation requires direct EOS SDK calls (EOS_ProgressionSnapshot_*).
 */
UCLASS()
class UNREALEXTENDEDEOS_API UEEOSProgressionSnapshotSubsystem : public UEEOSSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Snapshot Lifecycle ────────────────────────────────────────────────────

	/** Begin a new progression snapshot */
	UFUNCTION(BlueprintCallable, Category = "EOS|ProgressionSnapshot")
	int32 BeginSnapshot();

	/** Add a key-value pair of progress data to the active snapshot */
	UFUNCTION(BlueprintCallable, Category = "EOS|ProgressionSnapshot")
	bool AddProgressData(int32 SnapshotId, const FString& Key, const FString& Value);

	/** End and submit the snapshot to the EOS cloud */
	UFUNCTION(BlueprintCallable, Category = "EOS|ProgressionSnapshot")
	void EndSnapshot(int32 SnapshotId);

	/** Delete a previously submitted snapshot */
	UFUNCTION(BlueprintCallable, Category = "EOS|ProgressionSnapshot")
	void DeleteSnapshot(int32 SnapshotId);

	// ── Queries ──────────────────────────────────────────────────────────────

	/** Check if a snapshot is currently being built */
	UFUNCTION(BlueprintPure, Category = "EOS|ProgressionSnapshot")
	bool IsSnapshotInProgress() const;

	/** Get the current active snapshot ID (-1 if none) */
	UFUNCTION(BlueprintPure, Category = "EOS|ProgressionSnapshot")
	int32 GetActiveSnapshotId() const;

	/** Get the data stored in a snapshot */
	UFUNCTION(BlueprintPure, Category = "EOS|ProgressionSnapshot")
	TMap<FString, FString> GetSnapshotData(int32 SnapshotId) const;

	// ── Delegates ────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "EOS|ProgressionSnapshot")
	FOnEOSSnapshotComplete OnSnapshotComplete;

	UPROPERTY(BlueprintAssignable, Category = "EOS|ProgressionSnapshot")
	FOnEOSSnapshotDeleted OnSnapshotDeleted;

private:

	/** Active snapshots: InternalId → Key-Value Data */
	TMap<int32, TMap<FString, FString>> ActiveSnapshots;

	/** Maps our internal snapshot IDs to EOS SDK snapshot IDs */
	TMap<int32, int32> SnapshotIdMapping;

	int32 NextSnapshotId = 1;
};
