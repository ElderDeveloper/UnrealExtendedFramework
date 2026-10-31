// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tasks/Task.h"
#include "EGSoftCollisionGrid.h"
#include "EGSoftCollisionTypes.h"

#include "EGSoftCollisionSubsystem.generated.h"

class UEGSoftCollisionComponent;
class UEGSoftCollisionSubsystem;

/** Runs the end-of-frame snapshot after all movement of the frame. */
USTRUCT()
struct FEGSoftCollisionTickFunction : public FTickFunction
{
	GENERATED_BODY()

	TWeakObjectPtr<UEGSoftCollisionSubsystem> Subsystem;

	virtual void ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent) override;
	virtual FString DiagnosticMessage() override;
	virtual FName DiagnosticContext(bool bDetailed) override;
};

template <>
struct TStructOpsTypeTraits<FEGSoftCollisionTickFunction> : public TStructOpsTypeTraitsBase2<FEGSoftCollisionTickFunction>
{
	enum
	{
		WithCopy = false
	};
};

/**
 * UEGSoftCollisionSubsystem
 * One per game world. Owns the registry of soft collision pawns, the spatial grid and the frame
 * pipeline. Components register at BeginPlay and never tick.
 *
 * ---------------------------------------------------------------------------
 * Frame pipeline
 * ---------------------------------------------------------------------------
 *  1. Snapshot (game thread, TG_PostUpdateWork, after every pawn moved this frame). Apply deferred
 *     registrations, copy each pawn into a plain FEGSoftBody, re-bin pawns whose cell changed, and
 *     pick the pawns this machine scans now (control, relevance, sleep, scan interval).
 *  2. Compute (worker threads). A UE::Tasks task runs ParallelFor over the scanners. Each scanner
 *     reads the snapshot and the grid and writes only its own result slot: no locks, no
 *     allocation, no UObject. Below eg.SoftCollision.ParallelThreshold scanners it runs inline.
 *  3. Apply (game thread, FWorldDelegates::OnWorldPreActorTick of the next frame, before any
 *     movement ticks). Wait for the task, normally already finished, hand each pawn its result, and
 *     run the per-frame work of active pushes and pending requests.
 *
 * Registrations and removals that happen between the snapshot and the apply (network receive runs
 * at the start of the frame) are deferred to the next snapshot, so the arrays the task reads never
 * change under it.
 */
UCLASS()
class UNREALEXTENDEDGAMEPLAY_API UEGSoftCollisionSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UEGSoftCollisionSubsystem* Get(const UWorld* World);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	void Register(UEGSoftCollisionComponent& Component);
	void Unregister(UEGSoftCollisionComponent& Component);

	/** Components with an active ticking push or pending requests get per-frame calls before movement. */
	void SetNeedsFrameUpdate(UEGSoftCollisionComponent& Component, bool bNeedsUpdate);

	/**
	 * Server validation: whether a pawn that may push the registered pawn is within Reach of its
	 * shape, in the last snapshot. Reads only, so it is safe while the scan task runs.
	 */
	bool HasPushingNeighbourWithin(int32 RegistryIndex, float Reach) const;

	int32 GetNumRegistered() const { return Entries.Num(); }

	static bool IsSystemEnabled();
	static int32 GetDebugLevel();
	static bool IsForceServerOnly();
	static int32 GetDebugRejectEvery();
	static EEGSoftCollisionSafetyLevel ResolveSafetyLevel(EEGSoftCollisionSafetyLevel ProfileLevel);

	void NoteRequestsSent(int32 Count);
	void NoteRequestAccepted();
	void NoteRequestRejected();
	void NoteLateRequest();
	void NoteUnansweredPush();
	void NotePushIssued();

	/** Called by the tick function. */
	void RunSnapshot(float DeltaSeconds);

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	struct FEntry
	{
		TWeakObjectPtr<UEGSoftCollisionComponent> Component;
		FIntPoint Cell = FIntPoint::ZeroValue;
		double NextScanTime = 0.0;
		float SleepSpeed = 0.0f;
		bool bInGrid = false;
		bool bHadContact = false;
		bool bScanner = false;
		bool bPlayer = false;
	};

	void HandlePreActorTick(UWorld* InWorld, ELevelTick TickType, float DeltaSeconds);
	void WaitForCompute();
	void ProcessPendingRegistrations();
	void RemoveEntryAt(int32 Index);
	void UpdateGrid();
	void BuildScanList(double Now);
	void LaunchCompute();
	void ComputeScanner(int32 ScanSlot);
	void ApplyResults();
	void RunFrameUpdates(float DeltaSeconds);
	bool IsCellNearActivity(const FIntPoint& Cell) const;
	void DrawDebug() const;

	TArray<FEntry> Entries;
	TArray<FEGSoftBody> Bodies;
	FEGSoftCollisionGrid Grid;
	float LargestExtent = 0.0f;

	/** Registry index of each scanner of the last snapshot, with its tuning and result. */
	TArray<int32> ScanEntries;
	TArray<FEGSoftCollisionTuning> ScanTuning;
	TArray<FEGSoftPushResult> ScanResults;
	bool bResultsPending = false;
	UE::Tasks::FTask ComputeTask;

	TArray<TWeakObjectPtr<UEGSoftCollisionComponent>> PendingAdds;
	TArray<int32> PendingRemovals;
	TArray<TWeakObjectPtr<UEGSoftCollisionComponent>> FrameUpdates;
	TArray<TWeakObjectPtr<UEGSoftCollisionComponent>> FrameUpdatesScratch;

	/** Scratch, reused every frame. */
	TSet<FIntPoint> ActiveCells;
	TArray<FVector> PlayerLocations;

	FEGSoftCollisionTickFunction SnapshotTickFunction;
	FDelegateHandle PreActorTickHandle;
};
