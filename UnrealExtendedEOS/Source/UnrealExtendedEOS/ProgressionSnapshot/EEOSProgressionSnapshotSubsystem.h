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

	/**
	 * End and submit the snapshot to the EOS cloud. Returns true when the submit was handed
	 * to the SDK (result arrives on OnSnapshotComplete) and false when it could not be — a
	 * failure was already broadcast in that case.
	 *
	 * State semantics on failure: when the SDK cannot be reached (platform/interface gone),
	 * the local snapshot is KEPT so the submit can be retried once the SDK is back; if it
	 * never comes back, ForceResetSnapshotState() is the escape hatch. Once the submit ran,
	 * local state is cleared regardless of the SDK-side EndSnapshot result — EndSnapshot
	 * only fails with EOS_NotFound (eos_progressionsnapshot.h), meaning the SDK snapshot is
	 * already gone and there is nothing left to retry against.
	 */
	UFUNCTION(BlueprintCallable, Category = "EOS|ProgressionSnapshot")
	bool EndSnapshot(int32 SnapshotId);

	/**
	 * Delete ALL submitted snapshot data for the local user from the EOS cloud (the SDK's
	 * DeleteSnapshot is user-scoped, not per-snapshot). On success, a still-open local
	 * snapshot with this id is ended SDK-side before its tracking is dropped. Returns true
	 * when the request was handed to the SDK; false broadcasts a failure first.
	 */
	UFUNCTION(BlueprintCallable, Category = "EOS|ProgressionSnapshot")
	bool DeleteSnapshot(int32 SnapshotId);

	/**
	 * Escape hatch: clears ALL local snapshot tracking so IsSnapshotInProgress() is no
	 * longer stuck when EndSnapshot repeatedly cannot reach the EOS SDK (e.g. platform torn
	 * down). Best-effort ends every mapped SDK snapshot first when the SDK is reachable, so
	 * nothing leaks when it is. Does not touch cloud data and broadcasts nothing.
	 */
	UFUNCTION(BlueprintCallable, Category = "EOS|ProgressionSnapshot")
	void ForceResetSnapshotState();

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
