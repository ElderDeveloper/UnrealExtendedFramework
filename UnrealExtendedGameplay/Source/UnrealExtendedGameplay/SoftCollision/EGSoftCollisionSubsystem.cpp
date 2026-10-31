// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGSoftCollisionSubsystem.h"

#include "Async/ParallelFor.h"
#include "DrawDebugHelpers.h"
#include "EGSoftCollisionComponent.h"
#include "EGSoftCollisionMath.h"
#include "EGSoftCollisionProfile.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

DECLARE_CYCLE_STAT(TEXT("Snapshot"), STAT_EGSoftCollision_Snapshot, STATGROUP_EGSoftCollision);
DECLARE_CYCLE_STAT(TEXT("Scan (inline)"), STAT_EGSoftCollision_ScanInline, STATGROUP_EGSoftCollision);
DECLARE_CYCLE_STAT(TEXT("Wait For Scan"), STAT_EGSoftCollision_Wait, STATGROUP_EGSoftCollision);
DECLARE_CYCLE_STAT(TEXT("Apply"), STAT_EGSoftCollision_Apply, STATGROUP_EGSoftCollision);
DECLARE_CYCLE_STAT(TEXT("Frame Updates"), STAT_EGSoftCollision_FrameUpdates, STATGROUP_EGSoftCollision);
DECLARE_DWORD_COUNTER_STAT(TEXT("Registered Pawns"), STAT_EGSoftCollision_Registered, STATGROUP_EGSoftCollision);
DECLARE_DWORD_COUNTER_STAT(TEXT("Scanners"), STAT_EGSoftCollision_Scanners, STATGROUP_EGSoftCollision);
DECLARE_DWORD_COUNTER_STAT(TEXT("Sleeping"), STAT_EGSoftCollision_Sleeping, STATGROUP_EGSoftCollision);
DECLARE_DWORD_COUNTER_STAT(TEXT("Neighbour Tests"), STAT_EGSoftCollision_NeighbourTests, STATGROUP_EGSoftCollision);
DECLARE_DWORD_COUNTER_STAT(TEXT("Grid Cells"), STAT_EGSoftCollision_Cells, STATGROUP_EGSoftCollision);
DECLARE_DWORD_COUNTER_STAT(TEXT("Pushes Issued"), STAT_EGSoftCollision_Pushes, STATGROUP_EGSoftCollision);
DECLARE_DWORD_COUNTER_STAT(TEXT("Requests Sent"), STAT_EGSoftCollision_RequestsSent, STATGROUP_EGSoftCollision);
DECLARE_DWORD_COUNTER_STAT(TEXT("Requests Accepted"), STAT_EGSoftCollision_RequestsAccepted, STATGROUP_EGSoftCollision);
DECLARE_DWORD_COUNTER_STAT(TEXT("Requests Rejected"), STAT_EGSoftCollision_RequestsRejected, STATGROUP_EGSoftCollision);
DECLARE_DWORD_COUNTER_STAT(TEXT("Late Requests"), STAT_EGSoftCollision_LateRequests, STATGROUP_EGSoftCollision);
DECLARE_DWORD_COUNTER_STAT(TEXT("Unanswered Pushes"), STAT_EGSoftCollision_Unanswered, STATGROUP_EGSoftCollision);

namespace EGSoftCollisionCVars
{
	static TAutoConsoleVariable<int32> Enable(
		TEXT("eg.SoftCollision.Enable"), 1,
		TEXT("Enable Soft Collision scanning and pushes. 0 stops new pushes; active ones run out."),
		ECVF_Default);

	static TAutoConsoleVariable<int32> Debug(
		TEXT("eg.SoftCollision.Debug"), 0,
		TEXT("0: off. 1: draw issued pushes. 2: also draw every registered shape. 3: also log network events."),
		ECVF_Default);

	static TAutoConsoleVariable<int32> ShowGrid(
		TEXT("eg.SoftCollision.ShowGrid"), 0,
		TEXT("Draw the occupied grid cells."),
		ECVF_Default);

	static TAutoConsoleVariable<int32> ForceServerOnly(
		TEXT("eg.SoftCollision.ForceServerOnly"), 0,
		TEXT("Treat predicted drivers as server-authority drivers, to compare against prediction. Set it on server and clients alike."),
		ECVF_Default);

	static TAutoConsoleVariable<int32> Parallel(
		TEXT("eg.SoftCollision.Parallel"), 1,
		TEXT("Run the scan on worker threads when there are enough scanners."),
		ECVF_Default);

	static TAutoConsoleVariable<int32> RejectEvery(
		TEXT("eg.SoftCollision.Debug.RejectEvery"), 0,
		TEXT("Server: reject every Nth push request, to test the client's reject path. 0 rejects nothing."),
		ECVF_Cheat);

	static TAutoConsoleVariable<int32> SafetyLevelOverride(
		TEXT("eg.SoftCollision.SafetyLevelOverride"), -1,
		TEXT("Server: -1 uses each profile's safety level. 0 keys only, 1 accept client position, 2 client authoritative window."),
		ECVF_Cheat);

	static TAutoConsoleVariable<int32> ParallelThreshold(
		TEXT("eg.SoftCollision.ParallelThreshold"), 64,
		TEXT("Scanner count below which the scan runs inline on the game thread."),
		ECVF_Default);
}

namespace
{
	/** Grid cells are at least this large, so tiny pawns do not explode the cell count. */
	constexpr float MinGridCellSize = 64.0f;

	/** Headroom when the grid has to grow, so it does not rebuild for every slightly larger pawn. */
	constexpr float GridGrowthFactor = 1.25f;
}

// =====================================================================
// Tick function
// =====================================================================

void FEGSoftCollisionTickFunction::ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	if (UEGSoftCollisionSubsystem* Owner = Subsystem.Get())
	{
		Owner->RunSnapshot(DeltaTime);
	}
}

FString FEGSoftCollisionTickFunction::DiagnosticMessage()
{
	return TEXT("FEGSoftCollisionTickFunction");
}

FName FEGSoftCollisionTickFunction::DiagnosticContext(bool bDetailed)
{
	return FName(TEXT("EGSoftCollisionSnapshot"));
}

// =====================================================================
// Lifetime
// =====================================================================

UEGSoftCollisionSubsystem* UEGSoftCollisionSubsystem::Get(const UWorld* World)
{
	return World ? World->GetSubsystem<UEGSoftCollisionSubsystem>() : nullptr;
}

bool UEGSoftCollisionSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UEGSoftCollisionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	PreActorTickHandle = FWorldDelegates::OnWorldPreActorTick.AddUObject(this, &UEGSoftCollisionSubsystem::HandlePreActorTick);
}

void UEGSoftCollisionSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	SnapshotTickFunction.Subsystem = this;
	SnapshotTickFunction.TickGroup = TG_PostUpdateWork;
	SnapshotTickFunction.EndTickGroup = TG_PostUpdateWork;
	SnapshotTickFunction.bCanEverTick = true;
	SnapshotTickFunction.bStartWithTickEnabled = true;
	SnapshotTickFunction.bTickEvenWhenPaused = false;
	SnapshotTickFunction.bRunOnAnyThread = false;
	if (InWorld.PersistentLevel && !SnapshotTickFunction.IsTickFunctionRegistered())
	{
		SnapshotTickFunction.RegisterTickFunction(InWorld.PersistentLevel);
	}
}

void UEGSoftCollisionSubsystem::Deinitialize()
{
	WaitForCompute();

	if (SnapshotTickFunction.IsTickFunctionRegistered())
	{
		SnapshotTickFunction.UnRegisterTickFunction();
	}
	FWorldDelegates::OnWorldPreActorTick.Remove(PreActorTickHandle);
	PreActorTickHandle.Reset();

	for (const FEntry& Entry : Entries)
	{
		if (UEGSoftCollisionComponent* Component = Entry.Component.Get())
		{
			Component->RegistryIndex = INDEX_NONE;
		}
	}
	Entries.Reset();
	Bodies.Reset();
	ScanEntries.Reset();
	ScanTuning.Reset();
	ScanResults.Reset();
	PendingAdds.Reset();
	PendingRemovals.Reset();
	FrameUpdates.Reset();
	Grid.Reset(Grid.GetCellSize());

	Super::Deinitialize();
}

// =====================================================================
// Registration
// =====================================================================

void UEGSoftCollisionSubsystem::Register(UEGSoftCollisionComponent& Component)
{
	if (Component.RegistryIndex != INDEX_NONE)
	{
		return;
	}
	PendingAdds.AddUnique(&Component);
}

void UEGSoftCollisionSubsystem::Unregister(UEGSoftCollisionComponent& Component)
{
	PendingAdds.Remove(&Component);
	SetNeedsFrameUpdate(Component, false);

	const int32 Index = Component.RegistryIndex;
	if (Index == INDEX_NONE)
	{
		return;
	}

	// The component may be gone before the next snapshot: forget it now, remove the slot then.
	Component.RegistryIndex = INDEX_NONE;
	if (Entries.IsValidIndex(Index))
	{
		Entries[Index].Component.Reset();
		PendingRemovals.AddUnique(Index);
	}
}

void UEGSoftCollisionSubsystem::SetNeedsFrameUpdate(UEGSoftCollisionComponent& Component, bool bNeedsUpdate)
{
	if (bNeedsUpdate)
	{
		FrameUpdates.AddUnique(&Component);
	}
	else
	{
		FrameUpdates.RemoveSwap(&Component);
	}
}

void UEGSoftCollisionSubsystem::ProcessPendingRegistrations()
{
	if (!PendingRemovals.IsEmpty())
	{
		// Highest first: a swap only ever moves an entry that is not itself waiting for removal.
		PendingRemovals.Sort(TGreater<int32>());
		for (const int32 Index : PendingRemovals)
		{
			RemoveEntryAt(Index);
		}
		PendingRemovals.Reset();
	}

	for (const TWeakObjectPtr<UEGSoftCollisionComponent>& Weak : PendingAdds)
	{
		UEGSoftCollisionComponent* Component = Weak.Get();
		if (!Component || Component->RegistryIndex != INDEX_NONE)
		{
			continue;
		}

		const int32 Index = Entries.AddDefaulted();
		Entries[Index].Component = Component;
		Bodies.AddDefaulted();
		Bodies[Index].bEnabled = false;
		Component->RegistryIndex = Index;
	}
	PendingAdds.Reset();
}

void UEGSoftCollisionSubsystem::RemoveEntryAt(int32 Index)
{
	if (!Entries.IsValidIndex(Index))
	{
		return;
	}

	if (Entries[Index].bInGrid)
	{
		Grid.Remove(Index, Entries[Index].Cell);
	}

	const int32 LastIndex = Entries.Num() - 1;
	if (Index != LastIndex)
	{
		FEntry& Last = Entries[LastIndex];
		if (Last.bInGrid)
		{
			Grid.Remove(LastIndex, Last.Cell);
			Grid.Add(Index, Last.Cell);
		}
		Entries[Index] = MoveTemp(Last);
		Bodies[Index] = Bodies[LastIndex];
		Bodies[Index].Id = Index;
		if (UEGSoftCollisionComponent* Moved = Entries[Index].Component.Get())
		{
			Moved->RegistryIndex = Index;
		}
	}

	Entries.Pop(EAllowShrinking::No);
	Bodies.Pop(EAllowShrinking::No);
}

// =====================================================================
// Snapshot
// =====================================================================

void UEGSoftCollisionSubsystem::RunSnapshot(float DeltaSeconds)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(EGSoftCollision_Snapshot);
	SCOPE_CYCLE_COUNTER(STAT_EGSoftCollision_Snapshot);

	WaitForCompute();
	if (bResultsPending)
	{
		// The previous results were never applied (no actor tick ran in between). They are stale now.
		ScanEntries.Reset();
		bResultsPending = false;
	}

	ProcessPendingRegistrations();
	SET_DWORD_STAT(STAT_EGSoftCollision_Registered, Entries.Num());

	UWorld* World = GetWorld();
	if (!World || Entries.IsEmpty() || !IsSystemEnabled())
	{
		return;
	}

	// Copy every pawn into the snapshot. Only the game thread ever touches the components.
	LargestExtent = 0.0f;
	PlayerLocations.Reset();
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		FEntry& Entry = Entries[Index];
		FEGSoftBody& Body = Bodies[Index];
		UEGSoftCollisionComponent* Component = Entry.Component.Get();
		if (!Component)
		{
			Body.bEnabled = false;
			Entry.bScanner = false;
			continue;
		}

		FEGSoftCollisionSnapshotInfo Info;
		Component->WriteSnapshot(Body, Info);
		Body.Id = Index;
		Entry.bScanner = Info.bScanner;
		Entry.bPlayer = Info.bPlayerControlled;
		Entry.SleepSpeed = Info.SleepSpeed;

		if (Body.bEnabled)
		{
			LargestExtent = FMath::Max(LargestExtent, Body.GetMaxExtent());
			if (Info.bPlayerControlled)
			{
				PlayerLocations.Add(Body.Location);
			}
		}
	}

	UpdateGrid();
	BuildScanList(World->GetTimeSeconds());
	LaunchCompute();
	DrawDebug();
}

void UEGSoftCollisionSubsystem::UpdateGrid()
{
	// Every pair that can touch must sit in neighbouring cells: cell size >= twice the largest extent.
	const float Wanted = FMath::Max(2.0f * LargestExtent, MinGridCellSize);
	const bool bRebuild = Wanted > Grid.GetCellSize() || Wanted * 4.0f < Grid.GetCellSize();
	if (bRebuild)
	{
		Grid.Reset(Wanted * GridGrowthFactor);
		for (FEntry& Entry : Entries)
		{
			Entry.bInGrid = false;
		}
	}

	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		FEntry& Entry = Entries[Index];
		const FIntPoint Cell = Grid.GetCell(Bodies[Index].Location);
		if (!Entry.bInGrid)
		{
			Grid.Add(Index, Cell);
			Entry.bInGrid = true;
			Entry.Cell = Cell;
		}
		else if (Cell != Entry.Cell)
		{
			Grid.Move(Index, Entry.Cell, Cell);
			Entry.Cell = Cell;
		}
	}
	SET_DWORD_STAT(STAT_EGSoftCollision_Cells, Grid.GetNumCells());
}

bool UEGSoftCollisionSubsystem::IsCellNearActivity(const FIntPoint& Cell) const
{
	for (int32 OffsetX = -1; OffsetX <= 1; ++OffsetX)
	{
		for (int32 OffsetY = -1; OffsetY <= 1; ++OffsetY)
		{
			if (ActiveCells.Contains(FIntPoint(Cell.X + OffsetX, Cell.Y + OffsetY)))
			{
				return true;
			}
		}
	}
	return false;
}

void UEGSoftCollisionSubsystem::BuildScanList(double Now)
{
	ScanEntries.Reset();
	ScanTuning.Reset();
	ScanResults.Reset();

	// Cells with a moving pawn wake the sleepers around them.
	ActiveCells.Reset();
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		const FEGSoftBody& Body = Bodies[Index];
		const float SleepSpeed = FMath::Max(Entries[Index].SleepSpeed, 0.0f);
		if (Body.bEnabled && Body.Velocity.SizeSquared() > static_cast<double>(SleepSpeed * SleepSpeed))
		{
			ActiveCells.Add(Entries[Index].Cell);
		}
	}

	int32 Sleeping = 0;
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		FEntry& Entry = Entries[Index];
		const FEGSoftBody& Body = Bodies[Index];
		if (!Entry.bScanner || !Body.bEnabled || !Body.bCanBePushed)
		{
			Entry.bHadContact = false;
			continue;
		}

		UEGSoftCollisionComponent* Component = Entry.Component.Get();
		const UEGSoftCollisionProfile* Profile = Component ? Component->GetEffectiveProfile() : nullptr;
		if (!Profile)
		{
			continue;
		}
		const FEGSoftCollisionScanSettings& Scan = Profile->Scan;

		float Interval = Scan.PlayerScanInterval;
		if (!Entry.bPlayer && !PlayerLocations.IsEmpty())
		{
			double NearestSquared = TNumericLimits<double>::Max();
			for (const FVector& PlayerLocation : PlayerLocations)
			{
				NearestSquared = FMath::Min(NearestSquared, FVector::DistSquared2D(PlayerLocation, Body.Location));
			}
			if (Scan.CullDistance > 0.0f && NearestSquared > FMath::Square(static_cast<double>(Scan.CullDistance)))
			{
				Entry.bHadContact = false;
				continue;
			}
			Interval = NearestSquared <= FMath::Square(static_cast<double>(Scan.NearDistance)) ? Scan.NearScanInterval : Scan.FarScanInterval;
		}
		else if (!Entry.bPlayer)
		{
			Interval = Scan.NearScanInterval;
		}

		if (Now < Entry.NextScanTime)
		{
			continue;
		}

		const bool bSlow = Body.Velocity.SizeSquared() <= static_cast<double>(Scan.SleepSpeed * Scan.SleepSpeed);
		if (!Entry.bHadContact && bSlow && !IsCellNearActivity(Entry.Cell))
		{
			++Sleeping;
			continue;
		}

		Entry.NextScanTime = Now + Interval;
		ScanEntries.Add(Index);
		Component->GetEffectiveTuning(ScanTuning.AddDefaulted_GetRef());
	}

	ScanResults.SetNum(ScanEntries.Num());
	SET_DWORD_STAT(STAT_EGSoftCollision_Scanners, ScanEntries.Num());
	SET_DWORD_STAT(STAT_EGSoftCollision_Sleeping, Sleeping);
}

// =====================================================================
// Compute
// =====================================================================

void UEGSoftCollisionSubsystem::ComputeScanner(int32 ScanSlot)
{
	const int32 SelfIndex = ScanEntries[ScanSlot];
	const FEGSoftBody& Self = Bodies[SelfIndex];
	const FEGSoftCollisionTuning& Tuning = ScanTuning[ScanSlot];

	EGSoftCollision::FPushAccumulator Accumulator;
	Grid.ForEachNear(Entries[SelfIndex].Cell, [this, SelfIndex, &Self, &Tuning, &Accumulator](int32 OtherIndex)
	{
		if (OtherIndex != SelfIndex)
		{
			Accumulator.AddNeighbour(Tuning, Self, Bodies[OtherIndex]);
		}
	});
	ScanResults[ScanSlot] = Accumulator.Finish(Tuning);
}

void UEGSoftCollisionSubsystem::LaunchCompute()
{
	if (ScanEntries.IsEmpty())
	{
		return;
	}

	bResultsPending = true;
	const int32 Count = ScanEntries.Num();
	const bool bParallel = EGSoftCollisionCVars::Parallel.GetValueOnGameThread() != 0
		&& FApp::ShouldUseThreadingForPerformance()
		&& Count >= FMath::Max(EGSoftCollisionCVars::ParallelThreshold.GetValueOnGameThread(), 1);

	if (!bParallel)
	{
		SCOPE_CYCLE_COUNTER(STAT_EGSoftCollision_ScanInline);
		for (int32 Slot = 0; Slot < Count; ++Slot)
		{
			ComputeScanner(Slot);
		}
		return;
	}

	// The task reads Entries, Bodies, Grid and ScanTuning and writes ScanResults; nothing on the game
	// thread writes to any of them until WaitForCompute.
	ComputeTask = UE::Tasks::Launch(UE_SOURCE_LOCATION, [this, Count]()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(EGSoftCollision_Scan);
		ParallelFor(Count, [this](int32 Slot)
		{
			ComputeScanner(Slot);
		}, EParallelForFlags::Unbalanced);
	});
}

void UEGSoftCollisionSubsystem::WaitForCompute()
{
	if (ComputeTask.IsValid())
	{
		if (!ComputeTask.IsCompleted())
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(EGSoftCollision_WaitForScan);
			SCOPE_CYCLE_COUNTER(STAT_EGSoftCollision_Wait);
			ComputeTask.Wait();
		}
		ComputeTask = UE::Tasks::FTask();
	}
}

// =====================================================================
// Apply
// =====================================================================

void UEGSoftCollisionSubsystem::HandlePreActorTick(UWorld* InWorld, ELevelTick TickType, float DeltaSeconds)
{
	if (InWorld != GetWorld() || TickType == LEVELTICK_ViewportsOnly)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(EGSoftCollision_Apply);
	SCOPE_CYCLE_COUNTER(STAT_EGSoftCollision_Apply);

	WaitForCompute();
	if (!InWorld->IsPaused())
	{
		ApplyResults();
		RunFrameUpdates(DeltaSeconds);
	}
}

void UEGSoftCollisionSubsystem::ApplyResults()
{
	if (!bResultsPending)
	{
		return;
	}
	bResultsPending = false;

	int32 NeighbourTests = 0;
	for (int32 Slot = 0; Slot < ScanEntries.Num(); ++Slot)
	{
		const int32 Index = ScanEntries[Slot];
		if (!Entries.IsValidIndex(Index))
		{
			continue;
		}

		const FEGSoftPushResult& Result = ScanResults[Slot];
		NeighbourTests += Result.NeighbourTests;
		Entries[Index].bHadContact = Result.NeighbourCount > 0;
		if (UEGSoftCollisionComponent* Component = Entries[Index].Component.Get())
		{
			Component->ApplyScanResult(Result, ScanTuning[Slot]);
		}
	}
	ScanEntries.Reset();
	SET_DWORD_STAT(STAT_EGSoftCollision_NeighbourTests, NeighbourTests);
}

void UEGSoftCollisionSubsystem::RunFrameUpdates(float DeltaSeconds)
{
	if (FrameUpdates.IsEmpty())
	{
		return;
	}

	SCOPE_CYCLE_COUNTER(STAT_EGSoftCollision_FrameUpdates);

	// Updates may add or remove components from the list; iterate a copy.
	FrameUpdatesScratch = FrameUpdates;
	for (const TWeakObjectPtr<UEGSoftCollisionComponent>& Weak : FrameUpdatesScratch)
	{
		UEGSoftCollisionComponent* Component = Weak.Get();
		if (!Component)
		{
			FrameUpdates.RemoveSwap(Weak);
			continue;
		}
		if (!Component->TickFrame(DeltaSeconds))
		{
			FrameUpdates.RemoveSwap(Weak);
		}
	}
	FrameUpdatesScratch.Reset();
}

// =====================================================================
// Queries
// =====================================================================

bool UEGSoftCollisionSubsystem::HasPushingNeighbourWithin(int32 RegistryIndex, float Reach) const
{
	if (!Entries.IsValidIndex(RegistryIndex) || !Entries[RegistryIndex].bInGrid)
	{
		return false;
	}

	const FEGSoftBody& Self = Bodies[RegistryIndex];
	const float SafeReach = FMath::Max(Reach, 0.0f);
	const int32 Radius = FMath::Max(1, FMath::CeilToInt32((2.0f * LargestExtent + SafeReach) / Grid.GetCellSize()));

	bool bFound = false;
	Grid.ForEachNear(Entries[RegistryIndex].Cell, [this, RegistryIndex, &Self, SafeReach, &bFound](int32 OtherIndex)
	{
		if (bFound || OtherIndex == RegistryIndex)
		{
			return;
		}
		const FEGSoftBody& Other = Bodies[OtherIndex];
		if (!EGSoftCollision::CanPush(Other, Self))
		{
			return;
		}
		const double VerticalGap = FMath::Abs(Self.Location.Z - Other.Location.Z) - (static_cast<double>(Self.HalfHeight) + Other.HalfHeight);
		const double HorizontalGap = FVector::Dist2D(Self.Location, Other.Location) - (static_cast<double>(Self.GetMaxExtent()) + Other.GetMaxExtent());
		bFound = VerticalGap <= SafeReach && HorizontalGap <= SafeReach;
	}, Radius);
	return bFound;
}

bool UEGSoftCollisionSubsystem::IsSystemEnabled()
{
	return EGSoftCollisionCVars::Enable.GetValueOnGameThread() != 0;
}

int32 UEGSoftCollisionSubsystem::GetDebugLevel()
{
	return EGSoftCollisionCVars::Debug.GetValueOnGameThread();
}

bool UEGSoftCollisionSubsystem::IsForceServerOnly()
{
	return EGSoftCollisionCVars::ForceServerOnly.GetValueOnGameThread() != 0;
}

int32 UEGSoftCollisionSubsystem::GetDebugRejectEvery()
{
	return FMath::Max(EGSoftCollisionCVars::RejectEvery.GetValueOnGameThread(), 0);
}

EEGSoftCollisionSafetyLevel UEGSoftCollisionSubsystem::ResolveSafetyLevel(EEGSoftCollisionSafetyLevel ProfileLevel)
{
	const int32 Override = EGSoftCollisionCVars::SafetyLevelOverride.GetValueOnGameThread();
	if (Override < 0)
	{
		return ProfileLevel;
	}
	return static_cast<EEGSoftCollisionSafetyLevel>(FMath::Clamp(Override, 0, static_cast<int32>(EEGSoftCollisionSafetyLevel::ClientAuthoritativeWindow)));
}

void UEGSoftCollisionSubsystem::NoteRequestsSent(int32 Count)
{
	INC_DWORD_STAT_BY(STAT_EGSoftCollision_RequestsSent, Count);
}

void UEGSoftCollisionSubsystem::NoteRequestAccepted()
{
	INC_DWORD_STAT(STAT_EGSoftCollision_RequestsAccepted);
}

void UEGSoftCollisionSubsystem::NoteRequestRejected()
{
	INC_DWORD_STAT(STAT_EGSoftCollision_RequestsRejected);
}

void UEGSoftCollisionSubsystem::NoteLateRequest()
{
	INC_DWORD_STAT(STAT_EGSoftCollision_LateRequests);
}

void UEGSoftCollisionSubsystem::NoteUnansweredPush()
{
	INC_DWORD_STAT(STAT_EGSoftCollision_Unanswered);
}

void UEGSoftCollisionSubsystem::NotePushIssued()
{
	INC_DWORD_STAT(STAT_EGSoftCollision_Pushes);
}

// =====================================================================
// Debug
// =====================================================================

void UEGSoftCollisionSubsystem::DrawDebug() const
{
#if UE_ENABLE_DEBUG_DRAWING
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (GetDebugLevel() >= 2)
	{
		for (int32 Index = 0; Index < Bodies.Num(); ++Index)
		{
			const FEGSoftBody& Body = Bodies[Index];
			if (!Body.bEnabled)
			{
				continue;
			}
			const FColor Color = Entries[Index].bScanner ? FColor::Cyan : FColor(90, 90, 90);
			const FVector Bottom = Body.Location - FVector(0.0, 0.0, Body.HalfHeight);
			const FVector Top = Body.Location + FVector(0.0, 0.0, Body.HalfHeight);
			if (Body.Shape == EEGSoftCollisionShapeType::Box)
			{
				const FQuat Rotation(FVector::UpVector, FMath::Atan2(Body.Axis.Y, Body.Axis.X));
				DrawDebugBox(World, Body.Location, FVector(Body.BoxHalfExtent.X, Body.BoxHalfExtent.Y, Body.HalfHeight), Rotation, Color, false, -1.0f, 0, 1.0f);
			}
			else
			{
				DrawDebugCylinder(World, Bottom, Top, Body.Radius, 16, Color, false, -1.0f, 0, 1.0f);
			}
		}
	}

	if (EGSoftCollisionCVars::ShowGrid.GetValueOnGameThread() != 0)
	{
		const float Size = Grid.GetCellSize();
		Grid.ForEachCell([World, Size](const FIntPoint& Cell, const FEGSoftCollisionGrid::FBucket& Bucket)
		{
			const FVector Centre((Cell.X + 0.5) * Size, (Cell.Y + 0.5) * Size, 0.0);
			const FColor Color = Bucket.Num() > 4 ? FColor::Red : FColor::Yellow;
			DrawDebugBox(World, Centre, FVector(Size * 0.5f, Size * 0.5f, 2.0f), Color, false, -1.0f, 0, 1.0f);
		});
	}
#endif
}
