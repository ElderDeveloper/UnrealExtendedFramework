// Copyright Devil of the Plague. All Rights Reserved.
#include "EGQuestComponent.h"

#include "EGQuestContext.h"
#include "EGQuestGraph.h"
#include "EGQuestFactsSubsystem.h"
#include "EGQuestPluginSettings.h"
#include "EGQuestTargetRegistry.h"
#include "EGQuestScript.h"
#include "EGQuestEventCustom.h"
#include "Nodes/EGQuestNode_End.h"
#include "Nodes/EGQuestNode_Objective.h"
#include "Nodes/EGQuestNode_Stage.h"
#include "Nodes/EGQuestNode_Start.h"
#include "Logging/EGQuestLogger.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"

//
// FEGQuestSnapshotArray replication callbacks (fire on clients only)
//

void FEGQuestSnapshotArray::PostReplicatedAdd(const TArrayView<int32>& AddedIndices, int32 FinalSize)
{
	if (!OwnerComponent)
	{
		return;
	}
	for (const int32 Index : AddedIndices)
	{
		if (Items.IsValidIndex(Index))
		{
			OwnerComponent->NotifySnapshotReplicatedAdd(Items[Index]);
		}
	}
	OwnerComponent->NotifySnapshotsChanged();
}

void FEGQuestSnapshotArray::PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 FinalSize)
{
	if (!OwnerComponent)
	{
		return;
	}
	for (const int32 Index : ChangedIndices)
	{
		if (Items.IsValidIndex(Index))
		{
			OwnerComponent->NotifySnapshotReplicatedChange(Items[Index]);
		}
	}
	OwnerComponent->NotifySnapshotsChanged();
}

void FEGQuestSnapshotArray::PreReplicatedRemove(const TArrayView<int32>& RemovedIndices, int32 FinalSize)
{
	if (!OwnerComponent)
	{
		return;
	}
	for (const int32 Index : RemovedIndices)
	{
		if (Items.IsValidIndex(Index))
		{
			OwnerComponent->NotifySnapshotReplicatedRemove(Items[Index]);
		}
	}
	OwnerComponent->NotifySnapshotsChanged();
}

//
// UEGQuestComponent
//

UEGQuestComponent::UEGQuestComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = false;
	SharedQuestSnapshots.OwnerComponent = this;
	PrivateQuestSnapshots.OwnerComponent = this;
}

FEGQuestOperationResult UEGQuestComponent::ExecuteOrQueue(TFunction<FEGQuestOperationResult()>&& Input, FGuid RequestedRunId)
{
	if (bProcessingInputQueue)
	{
		QueuedInputs.Add(MoveTemp(Input));
		FEGQuestOperationResult Deferred;
		Deferred.Status = EEGQuestOperationStatus::Deferred;
		Deferred.Diagnostic = TEXT("QueuedAfterCurrentTransaction");
		Deferred.RunId = RequestedRunId;
		Deferred.BeforeRevision = GetRunRevision(RequestedRunId);
		Deferred.AfterRevision = Deferred.BeforeRevision;
		return Deferred;
	}

	bProcessingInputQueue = true;
	QueuedInputs.Add(MoveTemp(Input));
	FEGQuestOperationResult PrimaryResult;
	bool bHavePrimaryResult = false;

	while (QueuedInputs.Num() > 0)
	{
		TFunction<FEGQuestOperationResult()> Current = MoveTemp(QueuedInputs[0]);
		QueuedInputs.RemoveAt(0, 1, EAllowShrinking::No);

		TransactionRunRecords = RunRecords;
		DirtyRunIds.Reset();
		PostCommitCallbacks.Reset();
		bTransactionStateChanged = false;
		bTransactionActive = true;
		FEGQuestOperationResult Result = Current();
		bTransactionActive = false;

		if (Result.Status == EEGQuestOperationStatus::Applied)
		{
			RunRecords = MoveTemp(TransactionRunRecords);
			TArray<FGuid> RunsToProject = DirtyRunIds.Array();
			RunsToProject.Sort();
			for (const FGuid& RunId : RunsToProject)
			{
				ProjectRun(RunId);
			}
			for (const FGuid& RunId : RunsToProject)
			{
				if (const FEGQuestRunRecord* Record = RunRecords.Find(RunId); Record && Record->IsTerminal())
				{
					PurgeReplicatedTerminalSnapshot(RunId);
				}
			}
			CapTerminalRunHistory();
		}
		else
		{
			TransactionRunRecords.Reset();
		}

		TArray<TFunction<void()>> Callbacks = MoveTemp(PostCommitCallbacks);
		PostCommitCallbacks.Reset();
		for (TFunction<void()>& Callback : Callbacks)
		{
			Callback();
		}

		if (!bHavePrimaryResult)
		{
			PrimaryResult = MoveTemp(Result);
			bHavePrimaryResult = true;
		}
	}

	bProcessingInputQueue = false;
	UpdateTrackerPulseTimer();
	return PrimaryResult;
}

void UEGQuestComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TrackerPulseTimerHandle);
	}
	bTrackerPulseTimerArmed = false;
	Super::EndPlay(EndPlayReason);
}

bool UEGQuestComponent::HasPulsableRun() const
{
	for (const TPair<FGuid, FEGQuestRunRecord>& Pair : RunRecords)
	{
		if (!Pair.Value.IsTerminal())
		{
			return true;
		}
	}
	return false;
}

void UEGQuestComponent::HandleTrackerPulseTimer()
{
	PulseActiveTrackers();
}

void UEGQuestComponent::UpdateTrackerPulseTimer()
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// bTrackerPulseTimerArmed rather than FTimerManager::IsTimerActive: this runs inside the pulse's
	// own transaction, where a looping timer is mid-callback and querying it back is not worth
	// depending on. Tracking what we armed keeps the decision ours. Clearing a looping timer from
	// inside its own callback is well defined - it just does not reschedule.
	const bool bWantTimer = TrackerPulseInterval > 0.f && HasQuestAuthority() && HasPulsableRun();
	if (bWantTimer == bTrackerPulseTimerArmed)
	{
		return;
	}

	bTrackerPulseTimerArmed = bWantTimer;
	FTimerManager& TimerManager = World->GetTimerManager();
	if (bWantTimer)
	{
		TimerManager.SetTimer(
			TrackerPulseTimerHandle, this, &UEGQuestComponent::HandleTrackerPulseTimer,
			TrackerPulseInterval, true
		);
	}
	else
	{
		TimerManager.ClearTimer(TrackerPulseTimerHandle);
	}
}

void UEGQuestComponent::QueuePostCommit(TFunction<void()>&& Callback)
{
	if (bTransactionActive)
	{
		PostCommitCallbacks.Add(MoveTemp(Callback));
	}
	else
	{
		Callback();
	}
}

void UEGQuestComponent::MarkRunDirty(FGuid QuestInstanceGuid)
{
	SyncMainTrack(QuestInstanceGuid);
	bTransactionStateChanged = true;
	DirtyRunIds.Add(QuestInstanceGuid);
}

void UEGQuestComponent::QueueLifecycleFactWrite(FGuid Instance, const FString& Suffix)
{
	const UEGQuestPluginSettings* Settings = GetDefault<UEGQuestPluginSettings>();
	const FEGQuestRunRecord* Record = FindRunRecord(Instance);
	if (!Settings->bQuestLifecycleWritesFacts || !Settings->QuestWriteBackRoot.IsValid() || !Record) return;
	const FString TagString = FString::Printf(TEXT("%s.%s.%s"), *Settings->QuestWriteBackRoot.ToString(),
		*Record->DefinitionId.ToString(), *Suffix);
	const FGameplayTag FactTag = FGameplayTag::RequestGameplayTag(FName(*TagString), false);
	if (!FactTag.IsValid()) return;
	QueuePostCommit([this, FactTag]()
	{
		if (UWorld* World = GetWorld())
		{
			if (UEGQuestFactsSubsystem* Facts = World->GetSubsystem<UEGQuestFactsSubsystem>())
			{
				Facts->SetFact(FactTag, 1, EEGQuestFactScope::World, nullptr, GetOwner());
			}
		}
	});
}

void UEGQuestComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UEGQuestComponent, SharedQuestSnapshots);
	DOREPLIFETIME_CONDITION(UEGQuestComponent, PrivateQuestSnapshots, COND_OwnerOnly);
}

bool UEGQuestComponent::CanHostSharedQuests() const
{
	return GetOwner() && GetOwner()->IsA<AGameStateBase>();
}

bool UEGQuestComponent::CanHostPrivateQuests() const
{
	return GetOwner() && GetOwner()->IsA<APlayerState>();
}

FEGQuestOperationResult UEGQuestComponent::StartSharedQuest(UEGQuestGraph* QuestGraph)
{
	const FGuid RunId = FGuid::NewGuid();
	return ExecuteOrQueue([this, QuestGraph, RunId]()
	{
		if (!CanHostSharedQuests()) return MakeRejectedResult(RunId, TEXT("InvalidSharedQuestHost"));
		const FGuid Started = StartQuestInternal(QuestGraph, false, nullptr, RunId);
		return Started.IsValid()
			? FEGQuestOperationResult::Applied(Started, 0, GetRunRevision(Started))
			: MakeRejectedResult(RunId, TEXT("StartFailed"));
	}, RunId);
}

FEGQuestOperationResult UEGQuestComponent::StartSharedQuestNow(UEGQuestGraph* QuestGraph)
{
	if (!CanHostSharedQuests()) return MakeRejectedResult({}, TEXT("InvalidSharedQuestHost"));
	const FGuid RunId = StartQuestInternal(QuestGraph, false);
	return RunId.IsValid()
		? FEGQuestOperationResult::Applied(RunId, 0, GetRunRevision(RunId))
		: MakeRejectedResult({}, TEXT("StartFailed"));
}

FEGQuestOperationResult UEGQuestComponent::StartPrivateQuest(UEGQuestGraph* QuestGraph)
{
	const FGuid RunId = FGuid::NewGuid();
	return ExecuteOrQueue([this, QuestGraph, RunId]()
	{
		if (!CanHostPrivateQuests()) return MakeRejectedResult(RunId, TEXT("InvalidPrivateQuestHost"));
		const FGuid Started = StartQuestInternal(QuestGraph, true, nullptr, RunId);
		return Started.IsValid()
			? FEGQuestOperationResult::Applied(Started, 0, GetRunRevision(Started))
			: MakeRejectedResult(RunId, TEXT("StartFailed"));
	}, RunId);
}

FEGQuestOperationResult UEGQuestComponent::StartPrivateQuestNow(UEGQuestGraph* QuestGraph)
{
	if (!CanHostPrivateQuests()) return MakeRejectedResult({}, TEXT("InvalidPrivateQuestHost"));
	const FGuid RunId = StartQuestInternal(QuestGraph, true);
	return RunId.IsValid()
		? FEGQuestOperationResult::Applied(RunId, 0, GetRunRevision(RunId))
		: MakeRejectedResult({}, TEXT("StartFailed"));
}

FEGQuestOperationResult UEGQuestComponent::StartQuestFromTemplate(UEGQuestGraph* QuestGraph,
	const FEGQuestTemplateParameters& Parameters)
{
	const FGuid RunId = FGuid::NewGuid();
	return ExecuteOrQueue([this, QuestGraph, Parameters, RunId]()
	{
		const bool bPrivate = CanHostPrivateQuests() && !CanHostSharedQuests();
		if ((!bPrivate && !CanHostSharedQuests()) || (bPrivate && !CanHostPrivateQuests()))
			return MakeRejectedResult(RunId, TEXT("InvalidQuestHost"));
		const FGuid Started = StartQuestInternal(QuestGraph, bPrivate, &Parameters, RunId);
		return Started.IsValid()
			? FEGQuestOperationResult::Applied(Started, 0, GetRunRevision(Started))
			: MakeRejectedResult(RunId, TEXT("TemplateStartFailed"));
	}, RunId);
}

FEGQuestOperationResult UEGQuestComponent::StartQuestFromTemplateNow(UEGQuestGraph* QuestGraph,
	FEGQuestTemplateParameters Parameters)
{
	const bool bPrivate = CanHostPrivateQuests() && !CanHostSharedQuests();
	if ((!bPrivate && !CanHostSharedQuests()) || (bPrivate && !CanHostPrivateQuests()))
		return MakeRejectedResult({}, TEXT("InvalidQuestHost"));
	const FGuid RunId = StartQuestInternal(QuestGraph, bPrivate, &Parameters);
	return RunId.IsValid()
		? FEGQuestOperationResult::Applied(RunId, 0, GetRunRevision(RunId))
		: MakeRejectedResult({}, TEXT("TemplateStartFailed"));
}

FText UEGQuestComponent::GetRoleDisplayText(FGuid Instance, FName RoleName) const
{
	const FEGQuestRunRecord* Record = FindRunRecord(Instance);
	if (!Record) return FText::GetEmpty();
	const FEGQuestRoleBinding* Binding = nullptr;
	for (int32 Index = Record->RoleBindings.Num() - 1; Index >= 0; --Index)
		if (Record->RoleBindings[Index].RoleName == RoleName && !Record->RoleBindings[Index].Handles.IsEmpty())
		{ Binding = &Record->RoleBindings[Index]; break; }
	if (!Binding) return FText::GetEmpty();
	if (const UWorld* World = GetWorld())
		if (const UEGQuestTargetRegistrySubsystem* Registry = World->GetSubsystem<UEGQuestTargetRegistrySubsystem>())
			return Registry->ResolveDisplayName(Binding->Handles[0]);
	return FText::FromName(Binding->Handles[0].StableId);
}

TArray<AActor*> UEGQuestComponent::ResolveRoleActors(FGuid Instance, FName RoleName) const
{
	TArray<AActor*> Actors;
	const FEGQuestRunRecord* Record = FindRunRecord(Instance);
	const UWorld* World = GetWorld();
	const UEGQuestTargetRegistrySubsystem* Registry = World ? World->GetSubsystem<UEGQuestTargetRegistrySubsystem>() : nullptr;
	if (!Record || !Registry) return Actors;
	for (const FEGQuestRoleBinding& Binding : Record->RoleBindings)
	{
		if (Binding.RoleName != RoleName) continue;
		for (const FEGQuestEntityHandle& Handle : Binding.Handles)
			if (AActor* Actor = Registry->ResolveActor(Handle))
				Actors.AddUnique(Actor);
	}
	return Actors;
}

bool UEGQuestComponent::GetRoleTransform(FGuid Instance, FName RoleName, FTransform& OutTransform) const
{
	OutTransform = FTransform::Identity;
	const FEGQuestRunRecord* Record = FindRunRecord(Instance);
	const UWorld* World = GetWorld();
	const UEGQuestTargetRegistrySubsystem* Registry = World ? World->GetSubsystem<UEGQuestTargetRegistrySubsystem>() : nullptr;
	if (!Record || !Registry) return false;
	for (const FEGQuestRoleBinding& Binding : Record->RoleBindings)
	{
		if (Binding.RoleName != RoleName) continue;
		for (const FEGQuestEntityHandle& Handle : Binding.Handles)
		{
			bool bResolved = false;
			const FTransform Candidate = Registry->ResolveTransform(Handle, bResolved);
			if (bResolved)
			{
				OutTransform = Candidate;
				return true;
			}
		}
	}
	return false;
}

bool UEGQuestComponent::ResolveRoleDefinitions(FGuid Instance, const TArray<FEGQuestRoleDefinition>& Definitions,
	bool bStageScoped, FName TrackName, FGuid StageGuid)
{
	FEGQuestRunRecord* Record = FindMutableRunRecord(Instance);
	if (!Record) return false;
	FEGQuestRoleResolveContext Context;
	Context.World = GetWorld();
	Context.QuestComponent = this;
	Context.RunId = Instance;
	Context.ExistingBindings = &Record->RoleBindings;
	if (const AActor* OwnerActor = GetOwner()) Context.Origin = OwnerActor->GetActorTransform();
	for (const FEGQuestRoleDefinition& Definition : Definitions)
	{
		if (Definition.RoleName.IsNone())
		{
			if (Definition.bRequired) return false;
			continue;
		}
		const bool bHasTemplateOrQuestBinding = Record->RoleBindings.ContainsByPredicate(
			[&Definition](const FEGQuestRoleBinding& Binding)
			{
				return Binding.RoleName == Definition.RoleName && !Binding.bStageScoped && !Binding.Handles.IsEmpty();
			});
		if (bHasTemplateOrQuestBinding) continue;
		TArray<FEGQuestEntityHandle> Handles;
		const bool bResolved = Definition.Resolver && Definition.Resolver->Resolve(Context, Definition.RoleName, Handles);
		Handles.RemoveAll([](const FEGQuestEntityHandle& Handle){ return !Handle.IsValid(); });
		if ((!bResolved || Handles.IsEmpty()) && Definition.bRequired) return false;
		if (Handles.IsEmpty()) continue;
		FEGQuestRoleBinding& Binding = Record->RoleBindings.AddDefaulted_GetRef();
		Binding.RoleName = Definition.RoleName;
		Binding.Handles = MoveTemp(Handles);
		Binding.bStageScoped = bStageScoped;
		Binding.ScopeTrackName = TrackName;
		Binding.ScopeStageGuid = StageGuid;
		Binding.LossPolicy = Definition.LossPolicy;
	}
	return true;
}

const FEGQuestRoleDefinition* UEGQuestComponent::FindRoleDefinition(const FEGQuestRunRecord& Record,
	const FEGQuestRoleBinding& Binding) const
{
	if (Binding.bStageScoped)
	{
		if (const UEGQuestNode_Stage* Stage = GetActiveStage(Record.QuestInstanceGuid, Binding.ScopeTrackName))
			return Stage->GetRoleDefinitions().FindByPredicate([&Binding](const FEGQuestRoleDefinition& Definition)
			{
				return Definition.RoleName == Binding.RoleName;
			});
		return nullptr;
	}
	if (const UEGQuestGraph* Graph = Record.QuestGraph.Get())
		return Graph->GetRoleDefinitions().FindByPredicate([&Binding](const FEGQuestRoleDefinition& Definition)
		{
			return Definition.RoleName == Binding.RoleName;
		});
	return nullptr;
}

bool UEGQuestComponent::ApplyRoleLossPolicies(FGuid Instance)
{
	FEGQuestRunRecord* Record = FindMutableRunRecord(Instance);
	UWorld* World = GetWorld();
	UEGQuestTargetRegistrySubsystem* Registry = World ? World->GetSubsystem<UEGQuestTargetRegistrySubsystem>() : nullptr;
	if (!Record || !Registry || Record->IsTerminal()) return false;
	bool bChanged = false;
	bool bMustSuspend = false;
	for (FEGQuestRoleBinding& Binding : Record->RoleBindings)
	{
		const bool bLost = Binding.Handles.IsEmpty() || Binding.Handles.ContainsByPredicate(
			[Registry](const FEGQuestEntityHandle& Handle){ return Registry->ResolveActor(Handle) == nullptr; });
		if (!bLost)
		{
			if (Binding.bLossReported) { Binding.bLossReported = false; bChanged = true; }
			continue;
		}
		const FEGQuestRoleDefinition* Definition = FindRoleDefinition(*Record, Binding);
		const EEGQuestRoleLossPolicy Policy = Definition ? Definition->LossPolicy : Binding.LossPolicy;
		if (Policy == EEGQuestRoleLossPolicy::ReResolve && Definition && Definition->Resolver)
		{
			FEGQuestRoleResolveContext Context;
			Context.World = World;
			Context.QuestComponent = this;
			Context.RunId = Instance;
			Context.ExistingBindings = &Record->RoleBindings;
			if (const AActor* OwnerActor = GetOwner()) Context.Origin = OwnerActor->GetActorTransform();
			TArray<FEGQuestEntityHandle> Handles;
			if (Definition->Resolver->Resolve(Context, Binding.RoleName, Handles) && !Handles.IsEmpty())
			{
				Binding.Handles = MoveTemp(Handles);
				Binding.bLossReported = false;
				bChanged = true;
				RebuildRoleTexts(Instance, Binding.bStageScoped ? Binding.ScopeTrackName : FName(TEXT("Main")));
				continue;
			}
		}
		if (Policy == EEGQuestRoleLossPolicy::Suspend || Policy == EEGQuestRoleLossPolicy::ReResolve)
			bMustSuspend = true;
		else if (!Binding.bLossReported)
		{
			Binding.bLossReported = true;
			bChanged = true;
			const FEGQuestEntityHandle LostHandle = Binding.Handles.IsEmpty() ? FEGQuestEntityHandle{} : Binding.Handles[0];
			const FName RoleName = Binding.RoleName;
			QueuePostCommit([this, Instance, RoleName, LostHandle](){ OnQuestRoleLost.Broadcast(Instance, RoleName, LostHandle); });
			NotifyScriptRoleLost(Instance, RoleName);
			EmitTelemetry(EEGQuestTelemetryEventType::RoleLost, Instance);
		}
	}
	if (Record->bSuspendedByRoleLoss != bMustSuspend)
	{
		Record->bSuspendedByRoleLoss = bMustSuspend;
		bChanged = true;
	}
	if (bChanged)
	{
		RebuildRoleTexts(Instance, TEXT("Main"));
		if (const FEGQuestActiveTrackRuntimeSet* Sentinels = ActiveSentinelTracks.Find(Instance))
			for (const FEGQuestActiveTrackRuntime& Track : Sentinels->Tracks)
				RebuildRoleTexts(Instance, Track.TrackName);
		MarkRunDirty(Instance);
	}
	return bChanged;
}

bool UEGQuestComponent::ResolveStageRoles(FGuid Instance, FName TrackName)
{
	const UEGQuestNode_Stage* Stage = GetActiveStage(Instance, TrackName);
	return !Stage || ResolveRoleDefinitions(Instance, Stage->GetRoleDefinitions(), true, TrackName, Stage->GetGUID());
}

void UEGQuestComponent::ReleaseStageRoles(FGuid Instance, FName TrackName)
{
	if (FEGQuestRunRecord* Record = FindMutableRunRecord(Instance))
		Record->RoleBindings.RemoveAll([TrackName](const FEGQuestRoleBinding& Binding)
		{
			return Binding.bStageScoped && Binding.ScopeTrackName == TrackName;
		});
}

void UEGQuestComponent::RebuildRoleTexts(FGuid Instance, FName TrackName)
{
	UEGQuestContext* Context = GetTrackContext(Instance, TrackName);
	const UEGQuestNode_Stage* Stage = GetActiveStage(Instance, TrackName);
	if (!Context || !Stage) return;
	Stage->RebuildConstructedText(*Context);
	for (const FEGQuestEdge& Edge : Stage->GetNodeChildren())
		if (const UEGQuestNode_Objective* Objective = Cast<UEGQuestNode_Objective>(Context->GetNodeFromIndex(Edge.TargetIndex)))
			Objective->RebuildConstructedText(*Context);
}

void UEGQuestComponent::RefreshRoleMarkers(const FEGQuestRunRecord& Record,
	TArray<FEGQuestRoleMarker>& OutMarkers) const
{
	OutMarkers.Reset();
	const UWorld* World = GetWorld();
	const UEGQuestTargetRegistrySubsystem* Registry = World ? World->GetSubsystem<UEGQuestTargetRegistrySubsystem>() : nullptr;
	for (const FEGQuestRoleBinding& Binding : Record.RoleBindings)
		for (const FEGQuestEntityHandle& Handle : Binding.Handles)
		{
			FEGQuestRoleMarker& Marker = OutMarkers.AddDefaulted_GetRef();
			Marker.RoleName = Binding.RoleName;
			Marker.Handle = Handle;
			Marker.Transform = Registry ? Registry->ResolveTransform(Handle, Marker.bResolved) : FTransform::Identity;
		}
}

void UEGQuestComponent::QueueStageDirectives(FGuid Instance, FName TrackName,
	const UEGQuestNode_Stage& Stage, bool bEntering)
{
	TArray<FEGQuestDirective> Directives;
	if (bEntering) Directives = Stage.GetActivateDirectives();
	else
	{
		Directives = Stage.GetDeactivateDirectives();
		if (Stage.ShouldAutoRevertDirectives()) Directives.Append(Stage.GetActivateDirectives());
	}
	const EEGQuestDirectivePhase Phase = bEntering ? EEGQuestDirectivePhase::Activate : EEGQuestDirectivePhase::Deactivate;
	for (const FEGQuestDirective& Directive : Directives)
	{
		if (!Directive.DirectiveTag.IsValid()) continue;
		QueuePostCommit([this, Instance, Directive, Phase]()
		{
			OnQuestDirective.Broadcast(Instance, Directive, Phase);
			if (UWorld* World = GetWorld())
				if (UEGQuestDirectiveSubsystem* Router = World->GetSubsystem<UEGQuestDirectiveSubsystem>())
					Router->Dispatch(Instance, Directive, Phase);
		});
		EmitTelemetry(EEGQuestTelemetryEventType::Directive, Instance, Stage.GetGUID());
	}
}

bool UEGQuestComponent::ServerAbandonPrivateQuest_Validate(FGuid Instance)
{
	return Instance.IsValid();
}

void UEGQuestComponent::ServerAbandonPrivateQuest_Implementation(FGuid Instance)
{
	bool bPrivate = false;
	if (!FindMutableRunRecord(Instance, &bPrivate) || !bPrivate)
	{
		Reject(Instance, TEXT("UnknownOrNotPrivateQuest"));
		ClientQuestRequestRejected(Instance, LastRejectReason);
		return;
	}
	if (!AbandonQuest(Instance))
	{
		Reject(Instance, TEXT("AbandonFailed"));
		ClientQuestRequestRejected(Instance, LastRejectReason);
	}
}

void UEGQuestComponent::ClientQuestRequestRejected_Implementation(FGuid Instance, FName Reason)
{
	// On a listen server this RPC executes locally for the host, which already received the
	// authority-side broadcast from Reject(); only relay for true remote clients.
	if (!HasQuestAuthority())
	{
		OnQuestRequestRejected.Broadcast(Instance, Reason);
	}
}

bool UEGQuestComponent::BuildQuestSaveData(FGuid Instance, FEGQuestSaveEnvelope& OutSaveData) const
{
	if (!HasQuestAuthority()) return false;
	const FEGQuestRunRecord* Record = FindRunRecord(Instance);
	if (!Record || Record->IsTerminal()) return false;
	OutSaveData.SchemaVersion = 1;
	OutSaveData.AppliedMigrations.Reset();
	OutSaveData.RunRecord = *Record;
	OutSaveData.bPrivate = Record->bPrivate;
	ConvertTimerDeadlinesForSave(OutSaveData.RunRecord, GetServerTime());
	return true;
}

FEGQuestOperationResult UEGQuestComponent::ResumeQuest(const FEGQuestSaveEnvelope& SaveData)
{
	return ExecuteOrQueue([this, SaveData]() { return ResumeQuestNow(SaveData); }, SaveData.RunRecord.QuestInstanceGuid);
}

FEGQuestOperationResult UEGQuestComponent::ResumeQuestNow(const FEGQuestSaveEnvelope& SaveData)
{
	const FGuid SavedRunId = SaveData.RunRecord.QuestInstanceGuid;
	const int32 SavedRevision = SaveData.RunRecord.Revision;
	if (!HasQuestAuthority()) return MakeRejectedResult(SavedRunId, TEXT("NoAuthority"));
	if (SaveData.SchemaVersion != 1) return MakeRejectedResult(SavedRunId, TEXT("UnsupportedSaveSchema"));
	if (!SavedRunId.IsValid()) return MakeRejectedResult({}, TEXT("InvalidRunId"));
	if (FindRunRecord(SavedRunId)) return MakeRejectedResult(SavedRunId, TEXT("RunAlreadyExists"));
	if (SaveData.RunRecord.IsTerminal()) return MakeRejectedResult(SavedRunId, TEXT("TerminalRunCannotResume"));
	if (SaveData.RunRecord.QuestGraph.IsNull()) return MakeRejectedResult(SavedRunId, TEXT("MissingQuestGraph"));
	if ((SaveData.bPrivate && !CanHostPrivateQuests()) || (!SaveData.bPrivate && !CanHostSharedQuests()))
	{
		return MakeRejectedResult(SavedRunId, TEXT("InvalidQuestHost"));
	}
	UEGQuestGraph* Graph = SaveData.RunRecord.QuestGraph.LoadSynchronous();
	if (!Graph || Graph->GetGUID() != SaveData.RunRecord.GraphGuid || Graph->GetDefinitionId() != SaveData.RunRecord.DefinitionId)
	{
		return MakeRejectedResult(SavedRunId, TEXT("DefinitionMismatch"));
	}
	const bool bContentChanged = Graph->GetContentVersion() != SaveData.RunRecord.ContentVersion;

	UEGQuestContext* Context = NewObject<UEGQuestContext>(this);
	Context->SetRoleContext(this, SavedRunId);
	FEGQuestHistory History;
	History.VisitedNodeGUIDs.Append(SaveData.RunRecord.VisitedNodeGuids);
	const bool bRestart = bContentChanged && Graph->GetResumePolicy() == EEGQuestResumePolicy::Restart;
	const bool bContextReady = bRestart
		? Context->Start(Graph)
		: Context->ResumeFromNodeGUID(Graph, SaveData.RunRecord.ActiveNodeGuid, History, false);
	if (!bContextReady)
	{
		return MakeRejectedResult(SavedRunId, TEXT("ResumeNodeUnavailable"));
	}

	FEGQuestRunRecord Added = bRestart ? FEGQuestRunRecord() : SaveData.RunRecord;
	Added.QuestAssetId = Graph->GetPrimaryAssetId();
	Added.QuestGraph = Graph;
	Added.GraphGuid = Graph->GetGUID();
	Added.DefinitionId = Graph->GetDefinitionId();
	Added.ContentVersion = Graph->GetContentVersion();
	Added.QuestInstanceGuid = SavedRunId;
	Added.bPrivate = SaveData.bPrivate;
	Added.LifecycleState = EEGQuestLifecycleState::Active;
	Added.Revision = FMath::Max(1, SavedRevision + 1);
	if (bRestart)
	{
		Added.StartServerTime = GetServerTime();
		for (const FEGQuestRoleBinding& Binding : SaveData.RunRecord.RoleBindings)
			if (!Binding.bStageScoped) Added.RoleBindings.Add(Binding);
		Added.ObjectiveRequiredCountOverrides = SaveData.RunRecord.ObjectiveRequiredCountOverrides;
	}
	else
	{
		// Saved TrackerEndServerTime values are remaining durations (see BuildQuestSaveData).
		ConvertTimerDeadlinesForResume(Added, GetServerTime());
	}
	const FGuid NewInstanceGuid = SavedRunId;
	TerminalSnapshotCache.Remove(NewInstanceGuid);
	TransactionRunRecords.Add(NewInstanceGuid, Added);
	BindContextDelegates(NewInstanceGuid, *Context);
	ActiveContexts.Add(NewInstanceGuid, Context);
	RefreshRunRecord(NewInstanceGuid, false);
	if (bRestart && !ResolveRoleDefinitions(NewInstanceGuid, Graph->GetRoleDefinitions(), false))
	{
		ActiveContexts.Remove(NewInstanceGuid);
		TransactionRunRecords.Remove(NewInstanceGuid);
		return MakeRejectedResult(SavedRunId, TEXT("RequiredQuestRoleUnresolved"));
	}
	if (bRestart && !ResolveStageRoles(NewInstanceGuid, TEXT("Main")))
	{
		ActiveContexts.Remove(NewInstanceGuid);
		TransactionRunRecords.Remove(NewInstanceGuid);
		return MakeRejectedResult(SavedRunId, TEXT("RequiredStageRoleUnresolved"));
	}
	RebuildRoleTexts(NewInstanceGuid, TEXT("Main"));

	FEGQuestActiveTrackRuntimeSet& SentinelSet = ActiveSentinelTracks.Add(NewInstanceGuid);
	for (const UEGQuestNode* EntryNode : Graph->GetStartNodes())
	{
		const UEGQuestNode_Start* Entry = Cast<UEGQuestNode_Start>(EntryNode);
		if (!Entry || Entry->GetTrackType() != EEGQuestTrackType::Sentinel) continue;
		const FName TrackName = Entry->GetTrackName();
		FEGQuestTrackState* SavedTrack = FindMutableTrackState(NewInstanceGuid, TrackName);
		if (!bRestart && !SavedTrack) continue;
		UEGQuestContext* TrackContext = NewObject<UEGQuestContext>(this);
		TrackContext->SetRoleContext(this, NewInstanceGuid);
		bool bReady = false;
		if (bRestart)
		{
			bReady = TrackContext->StartFromEntry(Graph, *Entry);
		}
		else
		{
			FEGQuestHistory TrackHistory;
			TrackHistory.VisitedNodeGUIDs.Append(SavedTrack->VisitedNodeGuids);
			bReady = TrackContext->ResumeFromNodeGUID(Graph, SavedTrack->ActiveNodeGuid, TrackHistory, false);
		}
		if (!bReady) continue;
		BindContextDelegates(NewInstanceGuid, *TrackContext);
		FEGQuestActiveTrackRuntime& Runtime = SentinelSet.Tracks.AddDefaulted_GetRef();
		Runtime.TrackName = TrackName;
		Runtime.TrackType = EEGQuestTrackType::Sentinel;
		Runtime.Context = TrackContext;
		if (!bRestart) RebuildRoleTexts(NewInstanceGuid, TrackName);
		if (bRestart)
		{
			FEGQuestTrackState& NewTrack = FindMutableRunRecord(NewInstanceGuid)->Tracks.AddDefaulted_GetRef();
			NewTrack.TrackName = TrackName;
			NewTrack.TrackType = EEGQuestTrackType::Sentinel;
			if (!ResolveStageRoles(NewInstanceGuid, TrackName))
			{
				SentinelSet.Tracks.Pop();
				FindMutableRunRecord(NewInstanceGuid)->Tracks.Pop();
				continue;
			}
			RebuildRoleTexts(NewInstanceGuid, TrackName);
			RebuildTrackObjectives(NewInstanceGuid, TrackName, EEGQuestActivationReason::Migrated);
		}
		else
		{
			SavedTrack->ActiveNodeGuid = TrackContext->GetActiveNodeGUID();
			if (const UEGQuestNode_Stage* Stage = GetActiveStage(NewInstanceGuid, TrackName))
			{
				SavedTrack->StageTitle = Stage->GetTitle();
				const FText* Description = TrackContext->GetConstructedNodeText(Stage->GetGUID());
				SavedTrack->StageDescription = Description ? *Description : Stage->GetDescription();
				for (FEGQuestSnapshotObjective& Line : SavedTrack->ActiveObjectives)
					if (const UEGQuestNode_Objective* Objective = FindActiveObjectiveNode(NewInstanceGuid, Line.Guid))
						RefreshObjectivePresentation(NewInstanceGuid, TrackName, Line, *Objective);
			}
			CreateTrackEvaluators(NewInstanceGuid, TrackName, EEGQuestActivationReason::Restored);
		}
	}

	// The saved checklist is authoritative: rebuilding it here would reset the progress and the
	// outcomes the payload carries. Only the texts, which are not saved, are restored - and
	// ResumeFromNodeGUID has already rebuilt the stage's; the objectives' need doing here.
	if (const UEGQuestNode_Stage* Stage = GetActiveStage(NewInstanceGuid))
	{
		FEGQuestRunRecord* MutableRecord = FindMutableRunRecord(NewInstanceGuid);
		const FText* StageDescription = Context->GetConstructedNodeText(Stage->GetGUID());
		MutableRecord->ActiveStageTitle = Stage->GetTitle();
		MutableRecord->ActiveStageDescription = StageDescription ? *StageDescription : Stage->GetDescription();
		if (bRestart)
		{
			RebuildActiveObjectives(NewInstanceGuid, EEGQuestActivationReason::Migrated);
		}
		else for (FEGQuestSnapshotObjective& Line : MutableRecord->ActiveObjectives)
		{
			if (const UEGQuestNode_Objective* Objective = FindActiveObjectiveNode(NewInstanceGuid, Line.Guid))
				RefreshObjectivePresentation(NewInstanceGuid, TEXT("Main"), Line, *Objective);
		}
		MarkRunDirty(NewInstanceGuid);
	}
	if (const UEGQuestNode_Stage* MainStage = GetActiveStage(NewInstanceGuid, TEXT("Main")))
		QueueStageDirectives(NewInstanceGuid, TEXT("Main"), *MainStage, true);
	if (const FEGQuestActiveTrackRuntimeSet* Sentinels = ActiveSentinelTracks.Find(NewInstanceGuid))
		for (const FEGQuestActiveTrackRuntime& Track : Sentinels->Tracks)
			if (const UEGQuestNode_Stage* Stage = GetActiveStage(NewInstanceGuid, Track.TrackName))
				QueueStageDirectives(NewInstanceGuid, Track.TrackName, *Stage, true);

	QueuePostCommit([this, NewInstanceGuid]() { OnQuestStarted.Broadcast(NewInstanceGuid); });
	QueuePostCommit([this]() { OnQuestSnapshotsChanged.Broadcast(); });

	// A resumed quest gets a fresh script: scripts are never saved, so it must rebuild whatever it
	// needs from the snapshot. It is told about the resumed stage the same way a fresh start is.
	CreateScriptInstance(NewInstanceGuid, *Graph);
	{
		// Fresh evaluators reseed from the saved checklist - their counters are the lines themselves.
		// A still-pending objective that completes on activation resolves here, deferred so the
		// script hears about the resumed stage first.
		if (!bRestart)
		{
			CreateObjectiveEvaluators(NewInstanceGuid, EEGQuestActivationReason::Restored);
		}
		if (UEGQuestScript* Script = FindScript(NewInstanceGuid))
		{
			TWeakObjectPtr<UEGQuestScript> WeakScript = Script;
			QueuePostCommit([WeakScript]() { if (WeakScript.IsValid()) WeakScript->HandleQuestResumed(); });
		}
		NotifyScriptStageEntered(NewInstanceGuid);
		if (const UEGQuestNode* ActiveNode = Context->GetActiveNode())
		{
			QueueLifecycleFactWrite(NewInstanceGuid, FString::Printf(TEXT("Stage.%s"), *ActiveNode->GetGUID().ToString(EGuidFormats::Digits)));
		}
	}

	// Whatever was pending when this was saved may already be satisfied now.
	SettleQuest(NewInstanceGuid);
	MarkRunDirty(NewInstanceGuid);
	EmitTelemetry(EEGQuestTelemetryEventType::Resumed, NewInstanceGuid);
	return FEGQuestOperationResult::Applied(NewInstanceGuid, SavedRevision, GetRunRevision(NewInstanceGuid));
}

TArray<FEGQuestSaveEnvelope> UEGQuestComponent::ExtractAllQuestSaveData() const
{
	TArray<FEGQuestSaveEnvelope> Result;
	if (!HasQuestAuthority()) return Result;
	for (const TPair<FGuid, FEGQuestRunRecord>& Pair : RunRecords)
	{
		const FEGQuestRunRecord& Record = Pair.Value;
		if (!Record.IsTerminal())
		{
			FEGQuestSaveEnvelope& Data = Result.AddDefaulted_GetRef();
			Data.SchemaVersion = 1;
			Data.RunRecord = Record;
			Data.bPrivate = Record.bPrivate;
			ConvertTimerDeadlinesForSave(Data.RunRecord, GetServerTime());
		}
	}
	return Result;
}

TArray<FEGQuestOperationResult> UEGQuestComponent::RestoreAllQuestSaveData(const TArray<FEGQuestSaveEnvelope>& SaveData, bool bReplaceExisting)
{
	TArray<FEGQuestOperationResult> Results;
	Results.Reserve(SaveData.Num());
	if (bReplaceExisting && HasQuestAuthority())
	{
		for (const FEGQuestSaveEnvelope& Data : SaveData)
		{
			const FGuid CollisionId = Data.RunRecord.QuestInstanceGuid;
			if (!CollisionId.IsValid()) continue;
			if (const FEGQuestRunRecord* Existing = FindRunRecord(CollisionId))
			{
				if (Existing->IsTerminal())
				{
					PurgeTerminalRun(CollisionId);
				}
				else
				{
					AbandonQuest(CollisionId);
					PurgeTerminalRun(CollisionId);
				}
			}
			else if (TerminalSnapshotCache.Contains(CollisionId))
			{
				PurgeTerminalRun(CollisionId);
			}
		}
	}
	for (const FEGQuestSaveEnvelope& Data : SaveData)
	{
		Results.Add(ResumeQuest(Data));
	}
	return Results;
}

FGuid UEGQuestComponent::StartQuestInternal(UEGQuestGraph* QuestGraph, bool bPrivate,
	const FEGQuestTemplateParameters* Parameters, FGuid PreferredInstanceGuid)
{
	if (!HasQuestAuthority() || !IsValid(QuestGraph)) return {};
	const UEGQuestNode_Start* MainEntry = nullptr;
	TArray<const UEGQuestNode_Start*> SentinelEntries;
	for (const UEGQuestNode* EntryNode : QuestGraph->GetStartNodes())
	{
		const UEGQuestNode_Start* Entry = Cast<UEGQuestNode_Start>(EntryNode);
		if (!Entry) continue;
		if (Entry->GetTrackType() == EEGQuestTrackType::Main && !MainEntry) MainEntry = Entry;
		else if (Entry->GetTrackType() == EEGQuestTrackType::Sentinel) SentinelEntries.Add(Entry);
	}
	if (!MainEntry) return {};
	UEGQuestContext* Context = NewObject<UEGQuestContext>(this);
	if (!Context) return {};
	// Bind before Start so notifies fired by the first stage's enter events are not lost.
	const FGuid NewInstanceGuid = PreferredInstanceGuid.IsValid() ? PreferredInstanceGuid : FGuid::NewGuid();
	if (FindRunRecord(NewInstanceGuid) || TransactionRunRecords.Contains(NewInstanceGuid)) return {};
	BindContextDelegates(NewInstanceGuid, *Context);
	Context->SetRoleContext(this, NewInstanceGuid);
	// Start only makes the first stage active; its enter events fire below, once this quest is
	// published and a handler can actually read it back.
	if (!Context->StartFromEntry(QuestGraph, *MainEntry)) return {};

	FEGQuestRunRecord& Record = TransactionRunRecords.Add(NewInstanceGuid);
	Record.QuestAssetId = QuestGraph->GetPrimaryAssetId();
	Record.QuestGraph = QuestGraph;
	Record.GraphGuid = QuestGraph->GetGUID();
	Record.DefinitionId = QuestGraph->GetDefinitionId();
	Record.ContentVersion = QuestGraph->GetContentVersion();
	Record.QuestInstanceGuid = NewInstanceGuid;
	Record.bPrivate = bPrivate;
	Record.LifecycleState = EEGQuestLifecycleState::Active;
	Record.StartServerTime = GetServerTime();
	Record.Revision = 1;
	if (Parameters)
	{
		Record.RoleBindings = Parameters->RoleBindings;
		Record.ObjectiveRequiredCountOverrides = Parameters->ObjectiveCountOverrides;
	}
	if (!ResolveRoleDefinitions(NewInstanceGuid, QuestGraph->GetRoleDefinitions(), false))
	{
		TransactionRunRecords.Remove(NewInstanceGuid);
		return {};
	}
	const FGuid InstanceGuid = Record.QuestInstanceGuid;
	ActiveContexts.Add(InstanceGuid, Context);
	RefreshRunRecord(InstanceGuid, false);
	if (!ResolveStageRoles(InstanceGuid, TEXT("Main")))
	{
		ActiveContexts.Remove(InstanceGuid);
		TransactionRunRecords.Remove(InstanceGuid);
		return {};
	}
	RebuildRoleTexts(InstanceGuid, TEXT("Main"));
	RebuildActiveObjectives(InstanceGuid);
	if (const UEGQuestNode_Stage* MainStage = GetActiveStage(InstanceGuid, TEXT("Main")))
		QueueStageDirectives(InstanceGuid, TEXT("Main"), *MainStage, true);

	FEGQuestActiveTrackRuntimeSet& SentinelSet = ActiveSentinelTracks.Add(InstanceGuid);
	for (const UEGQuestNode_Start* Entry : SentinelEntries)
	{
		const FName TrackName = Entry->GetTrackName();
		if (TrackName.IsNone() || TrackName == TEXT("Main") || SentinelSet.Tracks.ContainsByPredicate(
			[TrackName](const FEGQuestActiveTrackRuntime& Existing){ return Existing.TrackName == TrackName; })) continue;
		UEGQuestContext* SentinelContext = NewObject<UEGQuestContext>(this);
		BindContextDelegates(InstanceGuid, *SentinelContext);
		SentinelContext->SetRoleContext(this, InstanceGuid);
		if (!SentinelContext->StartFromEntry(QuestGraph, *Entry)) continue;
		FEGQuestActiveTrackRuntime& Runtime = SentinelSet.Tracks.AddDefaulted_GetRef();
		Runtime.TrackName = TrackName;
		Runtime.TrackType = EEGQuestTrackType::Sentinel;
		Runtime.Context = SentinelContext;
		FEGQuestTrackState& State = Record.Tracks.AddDefaulted_GetRef();
		State.TrackName = TrackName;
		State.TrackType = EEGQuestTrackType::Sentinel;
		if (!ResolveStageRoles(InstanceGuid, TrackName))
		{
			SentinelSet.Tracks.Pop();
			Record.Tracks.Pop();
			continue;
		}
		RebuildRoleTexts(InstanceGuid, TrackName);
		RebuildTrackObjectives(InstanceGuid, TrackName, EEGQuestActivationReason::Started);
		if (const UEGQuestNode_Stage* SentinelStage = GetActiveStage(InstanceGuid, TrackName))
			QueueStageDirectives(InstanceGuid, TrackName, *SentinelStage, true);
		TWeakObjectPtr<UEGQuestContext> WeakSentinel = SentinelContext;
		QueuePostCommit([WeakSentinel](){ if (WeakSentinel.IsValid()) WeakSentinel->FireActiveNodeEnterEvents(); });
	}
	MarkRunDirty(InstanceGuid);
	QueuePostCommit([this, InstanceGuid]() { OnQuestStarted.Broadcast(InstanceGuid); });
	QueuePostCommit([this]() { OnQuestSnapshotsChanged.Broadcast(); });

	CreateScriptInstance(InstanceGuid, *QuestGraph);
	if (UEGQuestScript* Script = FindScript(InstanceGuid))
	{
		TWeakObjectPtr<UEGQuestScript> WeakScript = Script;
		QueuePostCommit([WeakScript]() { if (WeakScript.IsValid()) WeakScript->HandleQuestStarted(); });
	}
	NotifyScriptStageEntered(InstanceGuid);
	if (const UEGQuestNode* ActiveNode = Context->GetActiveNode())
	{
		QueueLifecycleFactWrite(InstanceGuid, FString::Printf(TEXT("Stage.%s"), *ActiveNode->GetGUID().ToString(EGuidFormats::Digits)));
	}
	TWeakObjectPtr<UEGQuestContext> WeakContext = Context;
	QueuePostCommit([WeakContext]() { if (WeakContext.IsValid()) WeakContext->FireActiveNodeEnterEvents(); });
	ApplyAutoTrackPolicy(InstanceGuid, *QuestGraph);

	// The first stage may already be satisfied (objectives that complete on stage enter), so settle.
	SettleQuest(InstanceGuid);
	EmitTelemetry(EEGQuestTelemetryEventType::Started, InstanceGuid);
	return InstanceGuid;
}

void UEGQuestComponent::EmitTelemetry(EEGQuestTelemetryEventType EventType, FGuid Instance, FGuid ElementGuid)
{
	const FEGQuestRunRecord* Record = FindRunRecord(Instance);
	const FName DefinitionId = Record ? Record->DefinitionId : NAME_None;
	const double ServerTime = GetServerTime();
	QueuePostCommit([this, EventType, DefinitionId, Instance, ElementGuid, ServerTime]()
	{
		OnQuestTelemetry.Broadcast(EventType, DefinitionId, Instance, ElementGuid, ServerTime);
	});
}

void UEGQuestComponent::ApplyAutoTrackPolicy(FGuid Instance, const UEGQuestGraph& QuestGraph)
{
	const EEGQuestAutoTrackPolicy Policy = QuestGraph.GetAutoTrackPolicy();
	if (Policy == EEGQuestAutoTrackPolicy::Never) return;
	const bool bAlreadyTracked = GetTrackedQuest().IsValid();
	if (Policy == EEGQuestAutoTrackPolicy::IfNone && bAlreadyTracked) return;
	for (TPair<FGuid, FEGQuestRunRecord>& Pair : TransactionRunRecords)
	{
		const bool bShouldTrack = Pair.Key == Instance;
		if (Pair.Value.bTracked == bShouldTrack) continue;
		Pair.Value.bTracked = bShouldTrack;
		// The newly-started run is projected by its start transaction. Any previous tracked run
		// must also publish the handoff so clients never observe two tracked quests.
		if (Pair.Key != Instance)
		{
			++Pair.Value.Revision;
			MarkRunDirty(Pair.Key);
			BroadcastUpdated(Pair.Key);
		}
	}
}

FGuid UEGQuestComponent::GetTrackedQuest() const
{
	const TMap<FGuid, FEGQuestRunRecord>& Records = bTransactionActive ? TransactionRunRecords : RunRecords;
	for (const TPair<FGuid, FEGQuestRunRecord>& Pair : Records)
		if (Pair.Value.bTracked && !Pair.Value.IsTerminal()) return Pair.Key;
	return {};
}

FEGQuestOperationResult UEGQuestComponent::SetTrackedQuest(FGuid Instance)
{
	return ExecuteOrQueue([this, Instance]() { return SetTrackedQuestNow(Instance); }, Instance);
}

FEGQuestOperationResult UEGQuestComponent::SetTrackedQuestNow(FGuid Instance)
{
	if (!HasQuestAuthority()) return MakeRejectedResult(Instance, TEXT("NoAuthority"));
	if (Instance.IsValid())
	{
		const FEGQuestRunRecord* Requested = FindRunRecord(Instance);
		if (!Requested || Requested->IsTerminal()) return MakeRejectedResult(Instance, TEXT("UnknownOrTerminalQuest"));
	}
	const FGuid BeforeTracked = GetTrackedQuest();
	if (BeforeTracked == Instance) return FEGQuestOperationResult::NoChange(Instance, GetRunRevision(Instance), TEXT("AlreadyTracked"));
	for (TPair<FGuid, FEGQuestRunRecord>& Pair : TransactionRunRecords)
	{
		const bool bNewTracked = Pair.Key == Instance;
		if (Pair.Value.bTracked == bNewTracked) continue;
		Pair.Value.bTracked = bNewTracked;
		++Pair.Value.Revision;
		MarkRunDirty(Pair.Key);
		BroadcastUpdated(Pair.Key);
	}
	return FEGQuestOperationResult::Applied(Instance, 0, GetRunRevision(Instance));
}

FEGQuestOperationResult UEGQuestComponent::DebugJumpToStage(FGuid Instance, FGuid StageGuid)
{
#if UE_BUILD_SHIPPING
	return FEGQuestOperationResult::Rejected(Instance, GetRunRevision(Instance), TEXT("CheatsDisabled"));
#else
	return ExecuteOrQueue([this, Instance, StageGuid]() { return DebugJumpToStageNow(Instance, StageGuid); }, Instance);
#endif
}

FEGQuestOperationResult UEGQuestComponent::DebugJumpToStageNow(FGuid Instance, FGuid StageGuid)
{
#if UE_BUILD_SHIPPING
	return FEGQuestOperationResult::Rejected(Instance, GetRunRevision(Instance), TEXT("CheatsDisabled"));
#else
	if (!HasQuestAuthority()) return MakeRejectedResult(Instance, TEXT("NoAuthority"));
	UEGQuestContext* Context = GetTrackContext(Instance, TEXT("Main"));
	const int32 StageIndex = Context ? Context->GetNodeIndexForGUID(StageGuid) : INDEX_NONE;
	if (StageIndex == INDEX_NONE || !Cast<UEGQuestNode_Stage>(Context->GetNodeFromIndex(StageIndex)))
		return MakeRejectedResult(Instance, TEXT("UnknownStage"));
	const int32 Before = GetRunRevision(Instance);
	if (!EnterStage(Instance, TEXT("Main"), StageIndex)) return MakeRejectedResult(Instance, TEXT("JumpFailed"));
	if (FEGQuestRunRecord* Record = FindMutableRunRecord(Instance)) Record->Revision = Before + 1;
	MarkRunDirty(Instance);
	BroadcastUpdated(Instance);
	return FEGQuestOperationResult::Applied(Instance, Before, Before + 1);
#endif
}

void UEGQuestComponent::BindContextDelegates(FGuid QuestInstanceGuid, UEGQuestContext& Context)
{
	Context.OnGameplayNotify.AddWeakLambda(this,
		[this, QuestInstanceGuid](UEGQuestContext&, const FGameplayTag& NotifyTag, float Magnitude)
		{
			QueuePostCommit([this, QuestInstanceGuid, NotifyTag, Magnitude]()
			{
				OnQuestGameplayNotify.Broadcast(QuestInstanceGuid, NotifyTag, Magnitude);
			});
		});
}

const UEGQuestNode_Stage* UEGQuestComponent::GetActiveStage(FGuid Instance) const
{
	const TObjectPtr<UEGQuestContext>* ContextPtr = ActiveContexts.Find(Instance);
	if (!ContextPtr || !*ContextPtr) return nullptr;
	return Cast<UEGQuestNode_Stage>((*ContextPtr)->GetActiveNode());
}

UEGQuestContext* UEGQuestComponent::GetTrackContext(FGuid Instance, FName TrackName) const
{
	if (TrackName.IsNone() || TrackName == TEXT("Main"))
	{
		const TObjectPtr<UEGQuestContext>* Context = ActiveContexts.Find(Instance);
		return Context ? Context->Get() : nullptr;
	}
	const FEGQuestActiveTrackRuntime* Runtime = FindSentinelRuntime(Instance, TrackName);
	return Runtime ? Runtime->Context.Get() : nullptr;
}

const UEGQuestNode_Stage* UEGQuestComponent::GetActiveStage(FGuid Instance, FName TrackName) const
{
	const UEGQuestContext* Context = GetTrackContext(Instance, TrackName);
	return Context ? Cast<UEGQuestNode_Stage>(Context->GetActiveNode()) : nullptr;
}

FEGQuestActiveTrackRuntime* UEGQuestComponent::FindSentinelRuntime(FGuid Instance, FName TrackName)
{
	FEGQuestActiveTrackRuntimeSet* Set = ActiveSentinelTracks.Find(Instance);
	return Set ? Set->Tracks.FindByPredicate([TrackName](const FEGQuestActiveTrackRuntime& Track){ return Track.TrackName == TrackName; }) : nullptr;
}

const FEGQuestActiveTrackRuntime* UEGQuestComponent::FindSentinelRuntime(FGuid Instance, FName TrackName) const
{
	const FEGQuestActiveTrackRuntimeSet* Set = ActiveSentinelTracks.Find(Instance);
	return Set ? Set->Tracks.FindByPredicate([TrackName](const FEGQuestActiveTrackRuntime& Track){ return Track.TrackName == TrackName; }) : nullptr;
}

FEGQuestTrackState* UEGQuestComponent::FindMutableTrackState(FGuid Instance, FName TrackName)
{
	FEGQuestRunRecord* Record = FindMutableRunRecord(Instance);
	return Record ? Record->Tracks.FindByPredicate([TrackName](const FEGQuestTrackState& Track){ return Track.TrackName == TrackName; }) : nullptr;
}

const FEGQuestTrackState* UEGQuestComponent::FindTrackState(FGuid Instance, FName TrackName) const
{
	const FEGQuestRunRecord* Record = FindRunRecord(Instance);
	return Record ? Record->Tracks.FindByPredicate([TrackName](const FEGQuestTrackState& Track){ return Track.TrackName == TrackName; }) : nullptr;
}

void UEGQuestComponent::SyncMainTrack(FGuid Instance)
{
	FEGQuestRunRecord* Record = FindMutableRunRecord(Instance);
	if (!Record) return;
	FEGQuestTrackState* Main = Record->Tracks.FindByPredicate([](const FEGQuestTrackState& Track){ return Track.TrackType == EEGQuestTrackType::Main; });
	if (!Main)
	{
		Record->Tracks.InsertDefaulted(0);
		Main = &Record->Tracks[0];
		Main->TrackName = TEXT("Main");
		Main->TrackType = EEGQuestTrackType::Main;
	}
	Main->ActiveNodeGuid = Record->ActiveNodeGuid;
	Main->StageTitle = Record->ActiveStageTitle;
	Main->StageDescription = Record->ActiveStageDescription;
	Main->ActiveObjectives = Record->ActiveObjectives;
	if (const UEGQuestContext* Context = GetTrackContext(Instance, TEXT("Main"))) Main->VisitedNodeGuids = Context->GetVisitedNodeGUIDs().Array();
}

const UEGQuestNode_Objective* UEGQuestComponent::FindActiveObjectiveNode(FGuid Instance, FGuid ObjectiveGuid) const
{
	TArray<FName> Tracks = {TEXT("Main")};
	if (const FEGQuestActiveTrackRuntimeSet* Set = ActiveSentinelTracks.Find(Instance))
		for (const FEGQuestActiveTrackRuntime& Runtime : Set->Tracks) Tracks.Add(Runtime.TrackName);
	for (const FName TrackName : Tracks)
	{
		const UEGQuestNode_Stage* Stage = GetActiveStage(Instance, TrackName);
		const UEGQuestContext* Context = GetTrackContext(Instance, TrackName);
		if (!Stage || !Context) continue;
		for (const FEGQuestEdge& Child : Stage->GetNodeChildren())
		{
			const UEGQuestNode_Objective* Objective = Cast<UEGQuestNode_Objective>(Context->GetNodeFromIndex(Child.TargetIndex));
			if (Objective && Objective->GetGUID() == ObjectiveGuid) return Objective;
		}
	}
	return nullptr;
}

int32 UEGQuestComponent::ResolveRequiredCount(const FEGQuestRunRecord& Record, const UEGQuestNode_Objective& Objective) const
{
	const int32 Authored = Objective.GetPresentedRequiredCount();
	const FGuid Guid = Objective.GetGUID();
	const FEGQuestObjectiveCountOverride* Override = Record.ObjectiveRequiredCountOverrides.FindByPredicate(
		[Guid](const FEGQuestObjectiveCountOverride& Entry){ return Entry.ObjectiveGuid == Guid; });
	return (Override && Override->RequiredCount > 0) ? Override->RequiredCount : Authored;
}

void UEGQuestComponent::RebuildActiveObjectives(FGuid Instance, EEGQuestActivationReason Reason)
{
	FEGQuestRunRecord* Record = FindMutableRunRecord(Instance);
	if (!Record) return;
	SyncMainTrack(Instance);
	Record = FindMutableRunRecord(Instance);
	if (FEGQuestTrackState* Main = FindMutableTrackState(Instance, TEXT("Main")))
	{
		for (FEGQuestSnapshotObjective Line : Record->ActiveObjectives)
		{
			if (!Line.IsResolved()) Line.Outcome = EEGQuestObjectiveOutcome::Obsolete;
			Main->ObjectiveHistory.Add(MoveTemp(Line));
		}
	}

	// Leaving a stage cancels everything it had pending: the evaluators are discarded and the
	// checklist is rebuilt, never carried. This runs even with no context left - a quest that just
	// ended must not keep advertising the last stage's journal entry as if it were still worked on.
	DestroyObjectiveEvaluators(Instance);
	Record->ActiveObjectives.Reset();
	Record->ActiveStageTitle = FText::GetEmpty();
	Record->ActiveStageDescription = FText::GetEmpty();

	TObjectPtr<UEGQuestContext>* ContextPtr = ActiveContexts.Find(Instance);
	const UEGQuestNode_Stage* Stage = GetActiveStage(Instance);
	if (!Stage || !ContextPtr || !*ContextPtr)
	{
		MarkRunDirty(Instance);
		SyncMainTrack(Instance);
		return;
	}

	UEGQuestContext& Context = **ContextPtr;
	Record->ActiveStageTitle = Stage->GetTitle();
	const FText* StageDescription = Context.GetConstructedNodeText(Stage->GetGUID());
	Record->ActiveStageDescription = StageDescription ? *StageDescription : Stage->GetDescription();

	for (const FEGQuestEdge& Child : Stage->GetNodeChildren())
	{
		const UEGQuestNode_Objective* Objective = Cast<UEGQuestNode_Objective>(Context.GetNodeFromIndex(Child.TargetIndex));
		if (!Objective) continue;

		// An objective is never entered, but its text still has to be formatted for this instance.
		Objective->RebuildConstructedText(Context);

		FEGQuestSnapshotObjective& Line = Record->ActiveObjectives.AddDefaulted_GetRef();
		Line.Guid = Objective->GetGUID();
		const FText* Constructed = Context.GetConstructedNodeText(Line.Guid);
		Line.Text = Constructed ? *Constructed : Objective->GetNodeText();
		Line.RequiredCount = ResolveRequiredCount(*Record, *Objective);
		RefreshObjectivePresentation(Instance, TEXT("Main"), Line, *Objective);
	}
	MarkRunDirty(Instance);
	SyncMainTrack(Instance);

	// Fresh evaluators for the fresh checklist. Activation may already resolve lines (objectives
	// that complete on stage enter); those resolutions defer to the caller's scope and settle at
	// the caller's SettleQuest.
	CreateObjectiveEvaluators(Instance, Reason);
}

void UEGQuestComponent::CreateObjectiveEvaluators(FGuid Instance, EEGQuestActivationReason Reason)
{
	DestroyObjectiveEvaluators(Instance);
	const FEGQuestRunRecord* Record = FindRunRecord(Instance);
	if (!Record || Record->IsTerminal() || !GetActiveStage(Instance))
	{
		return;
	}

	// Instantiate every evaluator before activating any: an objective that completes on activation
	// must find the whole checklist and its sibling evaluators already in place.
	FEGQuestObjectiveEvaluatorSet& Set = ActiveObjectiveEvaluators.Add(Instance);
	for (const FEGQuestSnapshotObjective& Line : Record->ActiveObjectives)
	{
		// Resumed quests carry resolved lines; there is nothing left to evaluate for those.
		if (Line.IsResolved())
		{
			continue;
		}
		const UEGQuestNode_Objective* Authored = FindActiveObjectiveNode(Instance, Line.Guid);
		if (!Authored)
		{
			continue;
		}
		// The evaluator is a transient copy of the authored node: the asset node is shared by every
		// quest instance in the process and must never carry runtime state.
		UEGQuestNode_Objective* Evaluator = DuplicateObject(Authored, this);
		// UEGQuestNode::PostDuplicate normally regenerates identity for an editor copy. A runtime
		// evaluator is not a new authored node: it must address the existing snapshot checklist row.
		Evaluator->RestoreRuntimeEvaluatorGUID(Authored->GetGUID());
		Evaluator->SetFlags(RF_Transient);
		Set.Evaluators.Add(Evaluator);
	}

	// Iterate a copy: an activation handler is free to drive the quest, which may tear the set down
	// or rehash the evaluator map by starting other quests.
	TArray<TObjectPtr<UEGQuestNode_Objective>> Evaluators = Set.Evaluators;
	for (UEGQuestNode_Objective* Evaluator : Evaluators)
	{
		if (IsValid(Evaluator))
		{
			TWeakObjectPtr<UEGQuestNode_Objective> WeakEvaluator = Evaluator;
			QueuePostCommit([this, WeakEvaluator, Instance, Reason]()
			{
				if (WeakEvaluator.IsValid()) WeakEvaluator->ActivateEvaluator(*this, Instance, Reason);
			});
		}
	}
}

void UEGQuestComponent::DestroyObjectiveEvaluators(FGuid Instance)
{
	// Remove the set first so a deactivation handler cannot be re-entered for it.
	FEGQuestObjectiveEvaluatorSet Set;
	if (!ActiveObjectiveEvaluators.RemoveAndCopyValue(Instance, Set))
	{
		return;
	}
	for (UEGQuestNode_Objective* Evaluator : Set.Evaluators)
	{
		if (IsValid(Evaluator))
		{
			TWeakObjectPtr<UEGQuestNode_Objective> WeakEvaluator = Evaluator;
			QueuePostCommit([WeakEvaluator]() { if (WeakEvaluator.IsValid()) WeakEvaluator->DeactivateEvaluator(); });
		}
	}
}

void UEGQuestComponent::DestroyTrackEvaluators(FGuid Instance, FName TrackName)
{
	FEGQuestActiveTrackRuntime* Runtime = FindSentinelRuntime(Instance, TrackName);
	if (!Runtime) return;
	FEGQuestObjectiveEvaluatorSet Previous = MoveTemp(Runtime->Evaluators);
	Runtime->Evaluators.Evaluators.Reset();
	for (UEGQuestNode_Objective* Evaluator : Previous.Evaluators)
	{
		if (IsValid(Evaluator))
		{
			TWeakObjectPtr<UEGQuestNode_Objective> Weak = Evaluator;
			QueuePostCommit([Weak](){ if (Weak.IsValid()) Weak->DeactivateEvaluator(); });
		}
	}
}

void UEGQuestComponent::RefreshObjectivePresentation(FGuid Instance, FName TrackName,
	FEGQuestSnapshotObjective& Line, const UEGQuestNode_Objective& Objective)
{
	Line.bOptional = Objective.IsOptional();
	Line.bHidden = Objective.IsHidden();
	Line.UITag = Objective.GetUITag();
	Line.SortOrder = Objective.GetSortOrder();
	const UEGQuestTracker* Tracker = Objective.GetTracker();
	const int32 PresentedCount = Tracker && Tracker->IsA<UEGQuestTracker_Distinct>() ? Line.DistinctKeys.Num()
		: Tracker && Tracker->IsA<UEGQuestTracker_Sequence>() ? Line.SequenceIndex : Line.Count;
	Line.Progress.Count = PresentedCount;
	Line.Progress.RequiredCount = Line.RequiredCount;
	Line.Progress.Ratio = Line.RequiredCount > 0 ? FMath::Clamp(static_cast<float>(PresentedCount) / Line.RequiredCount, 0.0f, 1.0f) : 0.0f;
	Line.Progress.bValue = Line.IsResolved() && Line.Outcome == EEGQuestObjectiveOutcome::Succeeded;
	Line.Progress.EndServerTime = Line.TrackerEndServerTime;
	Line.Progress.UniqueSet = Line.DistinctKeys;
	if (Tracker && Tracker->IsA<UEGQuestTracker_Timer>()) Line.Progress.Type = EEGQuestSemanticProgressType::Time;
	else if (Tracker && Tracker->IsA<UEGQuestTracker_Distinct>()) Line.Progress.Type = EEGQuestSemanticProgressType::UniqueSet;
	else if (Tracker && Tracker->IsA<UEGQuestTracker_Composite>()) Line.Progress.Type = EEGQuestSemanticProgressType::Ratio;
	else if (Line.RequiredCount > 1 || (Tracker && (Tracker->IsA<UEGQuestTracker_EventCount>() || Tracker->IsA<UEGQuestTracker_Sequence>())))
		Line.Progress.Type = EEGQuestSemanticProgressType::Count;
	else Line.Progress.Type = EEGQuestSemanticProgressType::Boolean;

	UEGQuestContext* Context = GetTrackContext(Instance, TrackName);
	if (Context)
	{
		// Build every placeholder in one localized format pass. Calling the generic node rebuild
		// first would turn automatic arguments (which intentionally have no custom provider) into
		// empty strings before the component could supply their semantic values.
		FFormatNamedArguments Arguments;
		for (const FEGQuestTextArgument& Argument : Objective.GetTextArguments())
		{
			if (Argument.DisplayString == TEXT("Count"))
				Arguments.Add(Argument.DisplayString, FFormatArgumentValue(static_cast<int64>(PresentedCount)));
			else if (Argument.DisplayString == TEXT("RequiredCount"))
				Arguments.Add(Argument.DisplayString, FFormatArgumentValue(static_cast<int64>(Line.RequiredCount)));
			else if (Argument.DisplayString == TEXT("Remaining"))
				Arguments.Add(Argument.DisplayString, FFormatArgumentValue(static_cast<int64>(FMath::Max(0, Line.RequiredCount - PresentedCount))));
			else if (Argument.DisplayString == TEXT("TimeLeft"))
			{
				const int32 TimeLeft = Line.TrackerEndServerTime > 0.0
					? FMath::Max(0, FMath::CeilToInt(Line.TrackerEndServerTime - GetServerTime())) : 0;
				Arguments.Add(Argument.DisplayString, FFormatArgumentValue(static_cast<int64>(TimeLeft)));
			}
			else Arguments.Add(Argument.DisplayString, Argument.ConstructFormatArgumentValue(*Context));
		}
		Line.Text = Arguments.IsEmpty() ? Objective.GetNodeText() : FText::Format(Objective.GetNodeText(), Arguments);
		Context->SetConstructedNodeText(Line.Guid, Line.Text);
	}
}

void UEGQuestComponent::QueueObjectiveEvents(FGuid Instance, FGuid ObjectiveGuid,
	const TArray<TObjectPtr<UEGQuestEventCustom>>& Events)
{
	const FName TrackName = FindTrackNameForObjective(Instance, ObjectiveGuid);
	TWeakObjectPtr<UEGQuestContext> WeakContext = GetTrackContext(Instance, TrackName);
	TArray<TWeakObjectPtr<UEGQuestEventCustom>> WeakEvents;
	for (UEGQuestEventCustom* Event : Events) if (Event) WeakEvents.Add(Event);
	QueuePostCommit([WeakContext, WeakEvents]()
	{
		if (!WeakContext.IsValid()) return;
		for (const TWeakObjectPtr<UEGQuestEventCustom>& Event : WeakEvents)
			if (Event.IsValid()) Event->EnterEvent(WeakContext.Get());
	});
}

void UEGQuestComponent::EvaluateObjectiveMilestones(FGuid Instance, FGuid ObjectiveGuid)
{
	FEGQuestSnapshotObjective* Line = FindMutableObjectiveState(Instance, ObjectiveGuid);
	const UEGQuestNode_Objective* Objective = FindActiveObjectiveNode(Instance, ObjectiveGuid);
	if (!Line || !Objective) return;
	const float Ratio = Line->Progress.Ratio;
	const TArray<FEGQuestObjectiveMilestone>& Milestones = Objective->GetMilestones();
	for (int32 Index = 0; Index < Milestones.Num(); ++Index)
	{
		if (Line->EmittedMilestones.Contains(Index) || Ratio < Milestones[Index].Threshold) continue;
		Line->EmittedMilestones.Add(Index);
		QueueObjectiveEvents(Instance, ObjectiveGuid, Milestones[Index].Events);
	}
}

void UEGQuestComponent::RebuildTrackObjectives(FGuid Instance, FName TrackName, EEGQuestActivationReason Reason)
{
	FEGQuestTrackState* State = FindMutableTrackState(Instance, TrackName);
	UEGQuestContext* Context = GetTrackContext(Instance, TrackName);
	const UEGQuestNode_Stage* Stage = GetActiveStage(Instance, TrackName);
	if (!State || !Context) return;
	for (FEGQuestSnapshotObjective Line : State->ActiveObjectives)
	{
		if (!Line.IsResolved()) Line.Outcome = EEGQuestObjectiveOutcome::Obsolete;
		State->ObjectiveHistory.Add(MoveTemp(Line));
	}
	DestroyTrackEvaluators(Instance, TrackName);
	State->ActiveObjectives.Reset();
	State->StageTitle = FText::GetEmpty();
	State->StageDescription = FText::GetEmpty();
	State->ActiveNodeGuid = Context->GetActiveNodeGUID();
	State->VisitedNodeGuids = Context->GetVisitedNodeGUIDs().Array();
	if (!Stage) { MarkRunDirty(Instance); return; }
	State->StageTitle = Stage->GetTitle();
	const FText* Description = Context->GetConstructedNodeText(Stage->GetGUID());
	State->StageDescription = Description ? *Description : Stage->GetDescription();
	const FEGQuestRunRecord* Record = FindRunRecord(Instance);
	for (const FEGQuestEdge& Child : Stage->GetNodeChildren())
	{
		const UEGQuestNode_Objective* Objective = Cast<UEGQuestNode_Objective>(Context->GetNodeFromIndex(Child.TargetIndex));
		if (!Objective) continue;
		Objective->RebuildConstructedText(*Context);
		FEGQuestSnapshotObjective& Line = State->ActiveObjectives.AddDefaulted_GetRef();
		Line.Guid = Objective->GetGUID();
		const FText* Constructed = Context->GetConstructedNodeText(Line.Guid);
		Line.Text = Constructed ? *Constructed : Objective->GetNodeText();
		Line.RequiredCount = Record ? ResolveRequiredCount(*Record, *Objective) : Objective->GetPresentedRequiredCount();
		RefreshObjectivePresentation(Instance, TrackName, Line, *Objective);
	}
	MarkRunDirty(Instance);
	CreateTrackEvaluators(Instance, TrackName, Reason);
}

void UEGQuestComponent::CreateTrackEvaluators(FGuid Instance, FName TrackName, EEGQuestActivationReason Reason)
{
	DestroyTrackEvaluators(Instance, TrackName);
	FEGQuestActiveTrackRuntime* Runtime = FindSentinelRuntime(Instance, TrackName);
	const FEGQuestTrackState* State = FindTrackState(Instance, TrackName);
	if (!Runtime || !State || !GetActiveStage(Instance, TrackName)) return;
	UEGQuestContext* Context = Runtime->Context;
	for (const FEGQuestSnapshotObjective& Line : State->ActiveObjectives)
	{
		if (Line.IsResolved()) continue;
		const UEGQuestNode_Stage* Stage = GetActiveStage(Instance, TrackName);
		const UEGQuestNode_Objective* Authored = nullptr;
		for (const FEGQuestEdge& Child : Stage->GetNodeChildren())
		{
			const UEGQuestNode_Objective* Candidate = Cast<UEGQuestNode_Objective>(Context->GetNodeFromIndex(Child.TargetIndex));
			if (Candidate && Candidate->GetGUID() == Line.Guid) { Authored = Candidate; break; }
		}
		if (!Authored) continue;
		UEGQuestNode_Objective* Evaluator = DuplicateObject(Authored, this);
		Evaluator->RestoreRuntimeEvaluatorGUID(Authored->GetGUID());
		Evaluator->SetFlags(RF_Transient);
		Runtime->Evaluators.Evaluators.Add(Evaluator);
	}
	const TArray<TObjectPtr<UEGQuestNode_Objective>> Evaluators = Runtime->Evaluators.Evaluators;
	for (UEGQuestNode_Objective* Evaluator : Evaluators)
	{
		TWeakObjectPtr<UEGQuestNode_Objective> Weak = Evaluator;
		QueuePostCommit([this, Weak, Instance, Reason](){ if (Weak.IsValid()) Weak->ActivateEvaluator(*this, Instance, Reason); });
	}
}

bool UEGQuestComponent::EnterStage(FGuid Instance, int32 StageIndex)
{
	return EnterStage(Instance, TEXT("Main"), StageIndex);
}

bool UEGQuestComponent::EnterStage(FGuid Instance, FName TrackName, int32 StageIndex)
{
	UEGQuestContext* Context = GetTrackContext(Instance, TrackName);
	if (!Context) return false;
	if (const UEGQuestNode_Stage* PreviousStage = GetActiveStage(Instance, TrackName))
	{
		QueueStageDirectives(Instance, TrackName, *PreviousStage, false);
		// The script only follows the main track, mirroring NotifyScriptStageEntered.
		if (TrackName == TEXT("Main"))
			NotifyScriptStageExited(Instance, *PreviousStage, EEGQuestStageExitReason::Advanced);
	}
	ReleaseStageRoles(Instance, TrackName);
	// Hold the context itself, not the map slot: entering fires enter events, and a game handler is
	// free to start or abandon a quest from one, which rehashes or empties ActiveContexts.
	if (!Context->EnterNode(StageIndex))
	{
		Reject(Instance, TEXT("EnterNodeFailed"));
		return false;
	}
	const UEGQuestNode* ActiveNode = Context->GetActiveNode();
	if (!Cast<UEGQuestNode_Stage>(ActiveNode) && !Cast<UEGQuestNode_End>(ActiveNode))
	{
		Reject(Instance, TEXT("EnterNodeInvalidType"));
		return false;
	}
	// An End node is terminal: nothing follows it, so the context stops here.
	if (Cast<UEGQuestNode_End>(ActiveNode) != nullptr)
	{
		Context->MarkQuestEnded();
	}
	if (TrackName == TEXT("Main"))
	{
		RefreshRunRecord(Instance);
		if (const FEGQuestRunRecord* Record = FindRunRecord(Instance); Record && !Record->IsTerminal())
		{
			if (!ResolveStageRoles(Instance, TrackName))
			{
				Reject(Instance, TEXT("RequiredStageRoleUnresolved"));
				SetTerminalState(Instance, EEGQuestLifecycleState::Failed);
				return false;
			}
			RebuildRoleTexts(Instance, TrackName);
			RebuildActiveObjectives(Instance);
			NotifyScriptStageEntered(Instance);
		}
	}
	else if (Context->HasQuestEnded())
	{
		EEGQuestLifecycleState Final = EEGQuestLifecycleState::Completed;
		if (const UEGQuestNode_End* End = Cast<UEGQuestNode_End>(Context->GetActiveNode()))
		{
			if (End->GetQuestResult() == EEGQuestResult::Failed) Final = EEGQuestLifecycleState::Failed;
			else if (End->GetQuestResult() == EEGQuestResult::Abandoned) Final = EEGQuestLifecycleState::Abandoned;
		}
		SetTerminalState(Instance, Final);
	}
	else
	{
		if (!ResolveStageRoles(Instance, TrackName))
		{
			Reject(Instance, TEXT("RequiredStageRoleUnresolved"));
			SetTerminalState(Instance, EEGQuestLifecycleState::Failed);
			return false;
		}
		RebuildRoleTexts(Instance, TrackName);
		RebuildTrackObjectives(Instance, TrackName, EEGQuestActivationReason::Started);
	}
	// Enter events fire last, so a handler that reads the snapshot sees the stage it was told about.
	TWeakObjectPtr<UEGQuestContext> WeakContext = Context;
	QueuePostCommit([WeakContext]() { if (WeakContext.IsValid()) WeakContext->FireActiveNodeEnterEvents(); });
	if (ActiveNode)
	{
		QueueLifecycleFactWrite(Instance, FString::Printf(TEXT("Track.%s.Stage.%s"), *TrackName.ToString(),
			*ActiveNode->GetGUID().ToString(EGuidFormats::Digits)));
	}
	if (const UEGQuestNode_Stage* EnteredStage = GetActiveStage(Instance, TrackName))
	{
		QueueStageDirectives(Instance, TrackName, *EnteredStage, true);
		EmitTelemetry(EEGQuestTelemetryEventType::StageEntered, Instance, EnteredStage->GetGUID());
	}
	return true;
}

bool UEGQuestComponent::IsDestinationSatisfied(FGuid Instance, int32 TargetIndex) const
{
	return IsDestinationSatisfied(Instance, TEXT("Main"), TargetIndex);
}

bool UEGQuestComponent::IsDestinationSatisfied(FGuid Instance, FName TrackName, int32 TargetIndex) const
{
	const UEGQuestContext* Context = GetTrackContext(Instance, TrackName);
	const FEGQuestRunRecord* Snapshot = FindRunRecord(Instance);
	const UEGQuestNode_Stage* Stage = GetActiveStage(Instance, TrackName);
	if (!Context || !Snapshot || !Stage) return false;

	const UEGQuestGraph* Quest = Context->GetQuest();
	if (!Quest) return false;
	const TArray<FEGQuestSnapshotObjective>* Objectives = &Snapshot->ActiveObjectives;
	if (TrackName != TEXT("Main"))
	{
		const FEGQuestTrackState* Track = FindTrackState(Instance, TrackName);
		if (!Track) return false;
		Objectives = &Track->ActiveObjectives;
	}

	// The join lives at the destination: every arrow pointing into it must be satisfied. An arrow
	// whose source is not an objective of the active stage can never be, which is exactly why a
	// destination fed from two different stages is a compile error rather than a silent hang.
	bool bAnyArrowFromActiveStage = false;
	for (const UEGQuestNode* Node : Quest->GetNodes())
	{
		if (!Node) continue;
		for (const FEGQuestEdge& Edge : Node->GetNodeChildren())
		{
			if (Edge.TargetIndex != TargetIndex) continue;

			const UEGQuestNode_Objective* Source = Cast<UEGQuestNode_Objective>(Node);
			if (!Source)
			{
				// A start node's arrow was satisfied by the quest starting; anything else pointing
				// here is not an arrow at all.
				if (Node->IsA<UEGQuestNode_Start>()) continue;
				return false;
			}

			const FGuid SourceGuid = Source->GetGUID();
			const FEGQuestSnapshotObjective* Line = Objectives->FindByPredicate(
				[SourceGuid](const FEGQuestSnapshotObjective& Entry){ return Entry.Guid == SourceGuid; });
			if (!Line) return false;

			const bool bSatisfied = Edge.Outcome == EEGQuestArrowOutcome::Success
				? Line->Outcome == EEGQuestObjectiveOutcome::Succeeded
				: (Line->Outcome == EEGQuestObjectiveOutcome::Failed || Line->Outcome == EEGQuestObjectiveOutcome::Expired);
			if (!bSatisfied) return false;
			bAnyArrowFromActiveStage = true;
		}
	}
	return bAnyArrowFromActiveStage;
}

int32 UEGQuestComponent::FindFiringDestination(FGuid Instance) const
{
	return FindFiringDestination(Instance, TEXT("Main"));
}

int32 UEGQuestComponent::FindFiringDestination(FGuid Instance, FName TrackName) const
{
	const UEGQuestContext* Context = GetTrackContext(Instance, TrackName);
	const FEGQuestRunRecord* Snapshot = FindRunRecord(Instance);
	const UEGQuestNode_Stage* Stage = GetActiveStage(Instance, TrackName);
	if (!Context || !Snapshot || !Stage) return INDEX_NONE;
	const TArray<FEGQuestSnapshotObjective>* Objectives = &Snapshot->ActiveObjectives;
	if (TrackName != TEXT("Main"))
	{
		const FEGQuestTrackState* Track = FindTrackState(Instance, TrackName);
		if (!Track) return INDEX_NONE;
		Objectives = &Track->ActiveObjectives;
	}

	// Arbitration: the first fully-satisfied destination wins, in authored order. Two players
	// resolving the same objectives in a different order can therefore land in different stages -
	// that is the designer's problem to author around, not something the system smooths over.
	for (const FEGQuestSnapshotObjective& Line : *Objectives)
	{
		if (!Line.IsResolved() || Line.Outcome == EEGQuestObjectiveOutcome::Obsolete) continue;
		const UEGQuestNode_Objective* Objective = nullptr;
		for (const FEGQuestEdge& Child : Stage->GetNodeChildren())
		{
			const UEGQuestNode_Objective* Candidate = Cast<UEGQuestNode_Objective>(Context->GetNodeFromIndex(Child.TargetIndex));
			if (Candidate && Candidate->GetGUID() == Line.Guid) { Objective = Candidate; break; }
		}
		if (!Objective) continue;

		const EEGQuestArrowOutcome Reached = Line.Outcome == EEGQuestObjectiveOutcome::Succeeded
			? EEGQuestArrowOutcome::Success
			: EEGQuestArrowOutcome::Fail;
		for (const FEGQuestEdge& Arrow : Objective->GetNodeChildren())
		{
			if (!Arrow.IsValid() || Arrow.Outcome != Reached) continue;
			if (IsDestinationSatisfied(Instance, TrackName, Arrow.TargetIndex))
			{
				return Arrow.TargetIndex;
			}
		}
	}
	return INDEX_NONE;
}

void UEGQuestComponent::SettleQuest(FGuid Instance)
{
	SettleTrack(Instance, TEXT("Main"));
}

void UEGQuestComponent::SettleTrack(FGuid Instance, FName TrackName)
{
	// One extra pass over the limit, purely to tell "settled on the last allowed transition" apart
	// from "still going": a quest that legitimately chained 128 transitions and then came to rest
	// must not be failed for it.
	for (int32 Guard = 0; Guard <= 128; ++Guard)
	{
		UEGQuestContext* Context = GetTrackContext(Instance, TrackName);
		if (!Context || Context->HasQuestEnded()) return;

		const int32 Target = FindFiringDestination(Instance, TrackName);
		if (Target == INDEX_NONE) return;
		if (Guard == 128) break;
		if (!EnterStage(Instance, TrackName, Target)) return;
	}

	// Stages that fire into each other with nothing to wait for can never settle: an authoring
	// error. Leaving the quest half-advanced would strand it, so fail it explicitly and loudly.
	if (UEGQuestContext* Context = GetTrackContext(Instance, TrackName))
	{
		FEGQuestLogger::Get().Errorf(
			TEXT("SettleQuest - fired 128 destinations without settling; failing the quest. Fix the graph. Context: %s"),
			*Context->GetContextString());
	}
	Reject(Instance, TEXT("FiringLimitReached"));
	SetTerminalState(Instance, EEGQuestLifecycleState::Failed);
}

bool UEGQuestComponent::ResolveObjective(FGuid Instance, FGuid ObjectiveGuid, EEGQuestObjectiveOutcome Outcome)
{
	FEGQuestRunRecord* Snapshot = FindMutableRunRecord(Instance);
	if (!Snapshot || Snapshot->IsTerminal()) return false;

	FEGQuestSnapshotObjective* Line = FindMutableObjectiveState(Instance, ObjectiveGuid);
	if (!Line || Line->IsResolved()) return false;

	Line->Outcome = Outcome;
	if (const UEGQuestNode_Objective* Objective = FindActiveObjectiveNode(Instance, ObjectiveGuid))
	{
		RefreshObjectivePresentation(Instance, FindTrackNameForObjective(Instance, ObjectiveGuid), *Line, *Objective);
		QueueObjectiveEvents(Instance, ObjectiveGuid,
			Outcome == EEGQuestObjectiveOutcome::Succeeded ? Objective->GetSuccessEvents() : Objective->GetFailEvents());
	}
	EmitTelemetry(EEGQuestTelemetryEventType::ObjectiveResolved, Instance, ObjectiveGuid);
	++Snapshot->Revision;
	MarkRunDirty(Instance);
	// Copy the line: the script may mutate the quest, which can rebuild the checklist under Line.
	// While a resolution scope is open the notification is deferred to scope exit, so the script
	// never hears a stage's resolutions before OnStageEntered for that stage.
	const FEGQuestSnapshotObjective ResolvedLine = *Line;
	QueuePostCommit([this, Instance, ResolvedLine]() { NotifyScriptObjectiveResolved(Instance, ResolvedLine); });
	return true;
}

bool UEGQuestComponent::CompleteObjectiveFromEvaluator(UEGQuestNode_Objective& Evaluator, bool bSuccess)
{
	const FGuid Instance = Evaluator.GetQuestInstanceGuid();
	if (!bTransactionActive)
	{
		TWeakObjectPtr<UEGQuestNode_Objective> WeakEvaluator = &Evaluator;
		const FEGQuestOperationResult Result = ExecuteOrQueue([this, WeakEvaluator, bSuccess, Instance]()
		{
			if (!WeakEvaluator.IsValid()) return MakeRejectedResult(Instance, TEXT("EvaluatorExpired"));
			const int32 BeforeRevision = GetRunRevision(Instance);
			if (!CompleteObjectiveFromEvaluator(*WeakEvaluator.Get(), bSuccess))
			{
				return MakeRejectedResult(Instance, TEXT("EvaluatorResolutionRejected"));
			}
			if (FEGQuestRunRecord* Record = FindMutableRunRecord(Instance))
			{
				Record->Revision = BeforeRevision + 1;
				MarkRunDirty(Instance);
			}
			return FEGQuestOperationResult::Applied(Instance, BeforeRevision, GetRunRevision(Instance));
		}, Instance);
		return Result.WasApplied() || Result.Status == EEGQuestOperationStatus::Deferred;
	}
	if (!HasQuestAuthority() || !Instance.IsValid())
	{
		return false;
	}
	// Failing an objective with no fail routing would hang its stage: no arrow could ever leave it.
	if (!bSuccess && !Evaluator.CanEverFail())
	{
		Reject(Instance, TEXT("ObjectiveCannotFail"));
		return false;
	}
	const EEGQuestObjectiveOutcome Outcome = bSuccess ? EEGQuestObjectiveOutcome::Succeeded :
		(Evaluator.GetTracker() && Evaluator.GetTracker()->IsA<UEGQuestTracker_Timer>()
			? EEGQuestObjectiveOutcome::Expired : EEGQuestObjectiveOutcome::Failed);
	if (!ResolveObjective(Instance, Evaluator.GetGUID(), Outcome))
	{
		return false;
	}
	// Inside a scope (checklist rebuild, event fan-out) the surrounding flow settles once at the end.
	if (!bCollectingTrackerProposals)
	{
		FName TrackName = TEXT("Main");
		if (const FEGQuestRunRecord* Record = FindRunRecord(Instance))
		{
			for (const FEGQuestTrackState& Track : Record->Tracks)
				if (Track.TrackType != EEGQuestTrackType::Main && Track.ActiveObjectives.ContainsByPredicate(
					[&Evaluator](const FEGQuestSnapshotObjective& Line){ return Line.Guid == Evaluator.GetGUID(); }))
				{ TrackName = Track.TrackName; break; }
		}
		SettleTrack(Instance, TrackName);
		BroadcastUpdated(Instance);
	}
	return true;
}

bool UEGQuestComponent::ApplyObjectiveProgress(FGuid Instance, FGuid ObjectiveGuid, int32 Delta, bool bFailProgress)
{
	if (!bTransactionActive)
	{
		const FEGQuestOperationResult Result = ExecuteOrQueue([this, Instance, ObjectiveGuid, Delta, bFailProgress]()
		{
			const int32 BeforeRevision = GetRunRevision(Instance);
			if (!ApplyObjectiveProgress(Instance, ObjectiveGuid, Delta, bFailProgress))
			{
				return FEGQuestOperationResult::NoChange(Instance, BeforeRevision, TEXT("ProgressUnchanged"));
			}
			if (FEGQuestRunRecord* Record = FindMutableRunRecord(Instance))
			{
				Record->Revision = BeforeRevision + 1;
				MarkRunDirty(Instance);
			}
			BroadcastUpdated(Instance);
			return FEGQuestOperationResult::Applied(Instance, BeforeRevision, GetRunRevision(Instance));
		}, Instance);
		return Result.WasApplied() || Result.Status == EEGQuestOperationStatus::Deferred;
	}
	if (!HasQuestAuthority() || Delta == 0)
	{
		return false;
	}
	FEGQuestRunRecord* Snapshot = FindMutableRunRecord(Instance);
	if (!Snapshot || Snapshot->IsTerminal())
	{
		return false;
	}
	FEGQuestSnapshotObjective* Line = FindMutableObjectiveState(Instance, ObjectiveGuid);
	if (!Line || Line->IsResolved())
	{
		return false;
	}

	int32& Value = bFailProgress ? Line->FailCount : Line->Count;
	const int32 NewValue = FMath::Max(0, Value + Delta);
	// A delta that clamps away is not a change: it must not cost a replication.
	if (NewValue == Value)
	{
		return false;
	}
	Value = NewValue;
	if (const UEGQuestNode_Objective* Objective = FindActiveObjectiveNode(Instance, ObjectiveGuid))
		RefreshObjectivePresentation(Instance, FindTrackNameForObjective(Instance, ObjectiveGuid), *Line, *Objective);
	if (!bFailProgress) EvaluateObjectiveMilestones(Instance, ObjectiveGuid);
	EmitTelemetry(EEGQuestTelemetryEventType::ObjectiveProgress, Instance, ObjectiveGuid);
	// Copy the line: the script hears about it post-commit, after the checklist may have moved.
	NotifyScriptObjectiveProgress(Instance, *Line);
	++Snapshot->Revision;
	MarkRunDirty(Instance);
	return true;
}

bool UEGQuestComponent::SetObjectiveSequenceIndex(FGuid Instance, FGuid ObjectiveGuid, int32 NewIndex)
{
	if (!bTransactionActive)
	{
		const FEGQuestOperationResult Result = ExecuteOrQueue([this, Instance, ObjectiveGuid, NewIndex]()
		{
			const int32 Before = GetRunRevision(Instance);
			if (!SetObjectiveSequenceIndex(Instance, ObjectiveGuid, NewIndex))
				return FEGQuestOperationResult::NoChange(Instance, Before, TEXT("SequenceUnchanged"));
			if (FEGQuestRunRecord* Record = FindMutableRunRecord(Instance)) Record->Revision = Before + 1;
			BroadcastUpdated(Instance);
			return FEGQuestOperationResult::Applied(Instance, Before, GetRunRevision(Instance));
		}, Instance);
		return Result.WasApplied() || Result.Status == EEGQuestOperationStatus::Deferred;
	}
	FEGQuestRunRecord* Record = FindMutableRunRecord(Instance);
	FEGQuestSnapshotObjective* Line = FindMutableObjectiveState(Instance, ObjectiveGuid);
	if (!Line || Line->IsResolved() || Line->SequenceIndex == NewIndex) return false;
	Line->SequenceIndex = FMath::Max(0, NewIndex);
	if (const UEGQuestNode_Objective* Objective = FindActiveObjectiveNode(Instance, ObjectiveGuid))
		RefreshObjectivePresentation(Instance, FindTrackNameForObjective(Instance, ObjectiveGuid), *Line, *Objective);
	EvaluateObjectiveMilestones(Instance, ObjectiveGuid);
	EmitTelemetry(EEGQuestTelemetryEventType::ObjectiveProgress, Instance, ObjectiveGuid);
	++Record->Revision;
	MarkRunDirty(Instance);
	return true;
}

int32 UEGQuestComponent::AddObjectiveDistinctKey(FGuid Instance, FGuid ObjectiveGuid, FName Key)
{
	if (Key.IsNone()) return 0;
	if (!bTransactionActive)
	{
		ExecuteOrQueue([this, Instance, ObjectiveGuid, Key]()
		{
			const int32 Before = GetRunRevision(Instance);
			const int32 CountBefore = [&]()
			{
				FEGQuestSnapshotObjective Line;
				return FindObjectiveAuthorityState(Instance, ObjectiveGuid, Line) ? Line.DistinctKeys.Num() : 0;
			}();
			const int32 CountAfter = AddObjectiveDistinctKey(Instance, ObjectiveGuid, Key);
			if (CountAfter == CountBefore) return FEGQuestOperationResult::NoChange(Instance, Before, TEXT("DistinctKeyExists"));
			if (FEGQuestRunRecord* Record = FindMutableRunRecord(Instance)) Record->Revision = Before + 1;
			BroadcastUpdated(Instance);
			return FEGQuestOperationResult::Applied(Instance, Before, GetRunRevision(Instance));
		}, Instance);
		FEGQuestSnapshotObjective Line;
		return FindObjectiveAuthorityState(Instance, ObjectiveGuid, Line) ? Line.DistinctKeys.Num() : 0;
	}
	FEGQuestRunRecord* Record = FindMutableRunRecord(Instance);
	FEGQuestSnapshotObjective* Line = FindMutableObjectiveState(Instance, ObjectiveGuid);
	if (!Line || Line->IsResolved()) return 0;
	if (!Line->DistinctKeys.Contains(Key))
	{
		Line->DistinctKeys.Add(Key);
		Line->DistinctKeys.Sort(FNameLexicalLess());
		if (const UEGQuestNode_Objective* Objective = FindActiveObjectiveNode(Instance, ObjectiveGuid))
			RefreshObjectivePresentation(Instance, FindTrackNameForObjective(Instance, ObjectiveGuid), *Line, *Objective);
		EvaluateObjectiveMilestones(Instance, ObjectiveGuid);
		EmitTelemetry(EEGQuestTelemetryEventType::ObjectiveProgress, Instance, ObjectiveGuid);
		++Record->Revision;
		MarkRunDirty(Instance);
	}
	return Line->DistinctKeys.Num();
}

bool UEGQuestComponent::SetObjectiveTrackerEndTime(FGuid Instance, FGuid ObjectiveGuid, double EndServerTime)
{
	if (!bTransactionActive)
	{
		const FEGQuestOperationResult Result = ExecuteOrQueue([this, Instance, ObjectiveGuid, EndServerTime]()
		{
			const int32 Before = GetRunRevision(Instance);
			if (!SetObjectiveTrackerEndTime(Instance, ObjectiveGuid, EndServerTime))
				return FEGQuestOperationResult::NoChange(Instance, Before, TEXT("EndTimeUnchanged"));
			if (FEGQuestRunRecord* Record = FindMutableRunRecord(Instance)) Record->Revision = Before + 1;
			BroadcastUpdated(Instance);
			return FEGQuestOperationResult::Applied(Instance, Before, GetRunRevision(Instance));
		}, Instance);
		return Result.WasApplied() || Result.Status == EEGQuestOperationStatus::Deferred;
	}
	FEGQuestRunRecord* Record = FindMutableRunRecord(Instance);
	FEGQuestSnapshotObjective* Line = FindMutableObjectiveState(Instance, ObjectiveGuid);
	if (!Line || Line->IsResolved() || FMath::IsNearlyEqual(Line->TrackerEndServerTime, EndServerTime)) return false;
	Line->TrackerEndServerTime = EndServerTime;
	if (const UEGQuestNode_Objective* Objective = FindActiveObjectiveNode(Instance, ObjectiveGuid))
		RefreshObjectivePresentation(Instance, FindTrackNameForObjective(Instance, ObjectiveGuid), *Line, *Objective);
	++Record->Revision;
	MarkRunDirty(Instance);
	return true;
}

bool UEGQuestComponent::FindObjectiveAuthorityState(FGuid Instance, FGuid ObjectiveGuid,
	FEGQuestSnapshotObjective& OutObjective) const
{
	const FEGQuestRunRecord* Record = FindRunRecord(Instance);
	if (!Record) return false;
	const FEGQuestSnapshotObjective* Line = FindObjectiveState(Instance, ObjectiveGuid);
	if (!Line) return false;
	OutObjective = *Line;
	return true;
}

FEGQuestOperationResult UEGQuestComponent::CompleteActiveObjective(FGuid Instance, FGuid ObjectiveGuid)
{
	return ExecuteOrQueue([this, Instance, ObjectiveGuid]() { return CompleteActiveObjectiveNow(Instance, ObjectiveGuid); }, Instance);
}

FEGQuestOperationResult UEGQuestComponent::CompleteActiveObjectiveNow(FGuid Instance, FGuid ObjectiveGuid)
{
	const int32 BeforeRevision = GetRunRevision(Instance);
	if (!HasQuestAuthority()) return MakeRejectedResult(Instance, TEXT("NoAuthority"));
	if (!FindActiveObjectiveNode(Instance, ObjectiveGuid))
	{
		return MakeRejectedResult(Instance, TEXT("StaleObjective"));
	}
	if (!ResolveObjective(Instance, ObjectiveGuid, EEGQuestObjectiveOutcome::Succeeded))
	{
		return MakeRejectedResult(Instance, TEXT("ObjectiveAlreadyResolved"));
	}
	SettleTrack(Instance, FindTrackNameForObjective(Instance, ObjectiveGuid));
	if (FEGQuestRunRecord* Snapshot = FindMutableRunRecord(Instance))
	{
		Snapshot->Revision = BeforeRevision + 1;
		MarkRunDirty(Instance);
	}
	BroadcastUpdated(Instance);
	return FEGQuestOperationResult::Applied(Instance, BeforeRevision, GetRunRevision(Instance));
}

FEGQuestOperationResult UEGQuestComponent::FailActiveObjective(FGuid Instance, FGuid ObjectiveGuid)
{
	return ExecuteOrQueue([this, Instance, ObjectiveGuid]() { return FailActiveObjectiveNow(Instance, ObjectiveGuid); }, Instance);
}

FEGQuestOperationResult UEGQuestComponent::FailActiveObjectiveNow(FGuid Instance, FGuid ObjectiveGuid)
{
	const int32 BeforeRevision = GetRunRevision(Instance);
	if (!HasQuestAuthority()) return MakeRejectedResult(Instance, TEXT("NoAuthority"));
	const UEGQuestNode_Objective* Objective = FindActiveObjectiveNode(Instance, ObjectiveGuid);
	if (!Objective)
	{
		return MakeRejectedResult(Instance, TEXT("StaleObjective"));
	}
	// An objective with no fail routing cannot fail: no arrow could ever leave its stage on failure.
	if (!Objective->CanEverFail())
	{
		return MakeRejectedResult(Instance, TEXT("ObjectiveCannotFail"));
	}
	if (!ResolveObjective(Instance, ObjectiveGuid, EEGQuestObjectiveOutcome::Failed))
	{
		return MakeRejectedResult(Instance, TEXT("ObjectiveAlreadyResolved"));
	}
	SettleTrack(Instance, FindTrackNameForObjective(Instance, ObjectiveGuid));
	if (FEGQuestRunRecord* Snapshot = FindMutableRunRecord(Instance))
	{
		Snapshot->Revision = BeforeRevision + 1;
		MarkRunDirty(Instance);
	}
	BroadcastUpdated(Instance);
	return FEGQuestOperationResult::Applied(Instance, BeforeRevision, GetRunRevision(Instance));
}

FEGQuestOperationResult UEGQuestComponent::NotifyGameplayEvent(const FEGQuestGameplayEvent& Event)
{
	return ExecuteOrQueue([this, Event]() { return NotifyGameplayEventNow(Event); });
}

FEGQuestOperationResult UEGQuestComponent::PulseActiveTrackers()
{
	return ExecuteOrQueue([this]() { return PulseActiveTrackersNow(); });
}

FEGQuestOperationResult UEGQuestComponent::RefreshRoleBindings()
{
	return ExecuteOrQueue([this]() { return RefreshRoleBindingsNow(); });
}

FEGQuestOperationResult UEGQuestComponent::RefreshRoleBindingsNow()
{
	if (!HasQuestAuthority()) return MakeRejectedResult({}, TEXT("NoAuthority"));
	TArray<FGuid> Instances;
	ActiveContexts.GetKeys(Instances);
	Instances.Sort();
	FEGQuestOperationResult Result = FEGQuestOperationResult::NoChange({}, 0, TEXT("NoRoleBindingChanged"));
	for (const FGuid& Instance : Instances)
	{
		const int32 Before = GetRunRevision(Instance);
		if (!ApplyRoleLossPolicies(Instance)) continue;
		if (FEGQuestRunRecord* Record = FindMutableRunRecord(Instance)) Record->Revision = Before + 1;
		MarkRunDirty(Instance);
		BroadcastUpdated(Instance);
		Result = FEGQuestOperationResult::Applied(Instance, Before, Before + 1);
		Result.AffectedElementGuids.Add(Instance);
	}
	return Result;
}

FEGQuestOperationResult UEGQuestComponent::PulseActiveTrackersNow()
{
	if (!HasQuestAuthority()) return MakeRejectedResult({}, TEXT("NoAuthority"));
	TArray<FGuid> Instances;
	ActiveObjectiveEvaluators.GetKeys(Instances);
	Instances.Sort();
	TArray<FGuid> Changed;
	for (const FGuid& Instance : Instances)
	{
		FEGQuestRunRecord* Record = FindMutableRunRecord(Instance);
		const FEGQuestObjectiveEvaluatorSet* Set = ActiveObjectiveEvaluators.Find(Instance);
		if (!Record || Record->IsTerminal()) continue;
		ApplyRoleLossPolicies(Instance);
		Record = FindMutableRunRecord(Instance);
		if (!Record || Record->bSuspendedByRoleLoss) continue;
		const int32 Before = Record->Revision;
		bCollectingTrackerProposals = true;
		if (Set) for (UEGQuestNode_Objective* Evaluator : Set->Evaluators) if (IsValid(Evaluator)) Evaluator->PulseEvaluator();
		if (const FEGQuestActiveTrackRuntimeSet* Sentinels = ActiveSentinelTracks.Find(Instance))
			for (const FEGQuestActiveTrackRuntime& Track : Sentinels->Tracks)
				for (UEGQuestNode_Objective* Evaluator : Track.Evaluators.Evaluators)
					if (IsValid(Evaluator)) Evaluator->PulseEvaluator();
		bCollectingTrackerProposals = false;
		if (FEGQuestRunRecord* AfterPulse = FindMutableRunRecord(Instance); AfterPulse && AfterPulse->Revision != Before)
		{
			SettleQuest(Instance);
			TArray<FName> SentinelNames;
			if (const FEGQuestActiveTrackRuntimeSet* Sentinels = ActiveSentinelTracks.Find(Instance))
				for (const FEGQuestActiveTrackRuntime& Track : Sentinels->Tracks) SentinelNames.Add(Track.TrackName);
			for (const FName TrackName : SentinelNames)
				{
					const FEGQuestRunRecord* Current = FindRunRecord(Instance);
					if (!Current || Current->IsTerminal()) break;
					SettleTrack(Instance, TrackName);
				}
			if (FEGQuestRunRecord* Committed = FindMutableRunRecord(Instance)) Committed->Revision = Before + 1;
			MarkRunDirty(Instance);
			Changed.Add(Instance);
			BroadcastUpdated(Instance);
		}
	}
	if (Changed.IsEmpty()) return FEGQuestOperationResult::NoChange({}, 0, TEXT("NoTrackerChanged"));
	FEGQuestOperationResult Result = FEGQuestOperationResult::Applied({}, 0, 0);
	Result.AffectedElementGuids = MoveTemp(Changed);
	return Result;
}

FEGQuestOperationResult UEGQuestComponent::NotifyGameplayEventNow(const FEGQuestGameplayEvent& Event)
{
	if (!HasQuestAuthority()) return MakeRejectedResult({}, TEXT("NoAuthority"));
	if (!Event.EventTag.IsValid()) return MakeRejectedResult({}, TEXT("InvalidEventTag"));

	// Rounded, signed progress delta. A magnitude that rounds to 0 counts as one completion tick -
	// but only when it was not negative, or an event meant to remove progress would add some.
	int32 ProgressDelta = FMath::RoundToInt(Event.Magnitude);
	if (ProgressDelta == 0 && Event.Magnitude >= 0.f) ProgressDelta = 1;

	TArray<FGuid> UpdatedInstances;
	TArray<FGuid> Instances;
	ActiveContexts.GetKeys(Instances);
	Instances.Sort();
	bool bAnyRunAccepted = false;
	bool bAnyCursorAdvanced = false;
	const FName PublisherId = Event.PublisherId.IsNone() ? FName(TEXT("Legacy")) : Event.PublisherId;
	for (const FGuid& Instance : Instances)
	{
		FEGQuestRunRecord* Snapshot = FindMutableRunRecord(Instance);
		if (!Snapshot || Snapshot->IsTerminal() || !GetActiveStage(Instance)) continue;
		ApplyRoleLossPolicies(Instance);
		Snapshot = FindMutableRunRecord(Instance);
		if (!Snapshot || Snapshot->bSuspendedByRoleLoss) continue;
		const int32 BeforeRevision = Snapshot->Revision;
		bool bCursorAdvancedForRun = false;

		// Idempotency is per logical run and per publisher, and the cursor lives in the save payload.
		if (Event.Sequence != 0)
		{
			FEGQuestPublisherCursor* Cursor = Snapshot->PublisherCursors.FindByPredicate(
				[PublisherId](const FEGQuestPublisherCursor& Item){ return Item.PublisherId == PublisherId; });
			if (Cursor && Event.Sequence <= Cursor->Sequence) continue;
			if (!Cursor)
			{
				Cursor = &Snapshot->PublisherCursors.AddDefaulted_GetRef();
				Cursor->PublisherId = PublisherId;
			}
			Cursor->Sequence = Event.Sequence;
			bAnyCursorAdvanced = true;
			bCursorAdvancedForRun = true;
		}
		bAnyRunAccepted = true;

		TArray<TPair<FName, TArray<TObjectPtr<UEGQuestNode_Objective>>>> TrackEvaluators;
		if (const FEGQuestObjectiveEvaluatorSet* Set = ActiveObjectiveEvaluators.Find(Instance))
			TrackEvaluators.Emplace(TEXT("Main"), Set->Evaluators);
		if (const FEGQuestActiveTrackRuntimeSet* Sentinels = ActiveSentinelTracks.Find(Instance))
			for (const FEGQuestActiveTrackRuntime& Track : Sentinels->Tracks)
				TrackEvaluators.Emplace(Track.TrackName, Track.Evaluators.Evaluators);
		// Always fan out tag-bearing events. Counting trackers ignore a zero delta; Sequence/Distinct
		// must still see the event even when Magnitude rounds to 0 (negative no-ops).
		if (TrackEvaluators.Num() > 0)
		{
			// Every pending objective proposes against the same transaction-local run record. Settling
			// begins only after the proposal fan-out, so no stage rebuild invalidates this iteration.
			bCollectingTrackerProposals = true;
			for (const auto& Track : TrackEvaluators)
				for (UEGQuestNode_Objective* Evaluator : Track.Value)
					if (IsValid(Evaluator)) Evaluator->HandleGameplayEvent(Event, ProgressDelta);
			bCollectingTrackerProposals = false;
		}
		const bool bRunChanged = Snapshot->Revision != BeforeRevision || bCursorAdvancedForRun;
		if (!bRunChanged) continue;

		// SettleQuest may leave this stage, which rebuilds the checklist - so nothing above may
		// hold on to Snapshot or its objectives past this point.
		for (const auto& Track : TrackEvaluators)
		{
			const FEGQuestRunRecord* Current = FindRunRecord(Instance);
			if (!Current || Current->IsTerminal()) break;
			SettleTrack(Instance, Track.Key);
		}
		// Evaluators may resolve several objectives and settle through several nodes. The whole event
		// is one transaction, therefore its committed view consumes exactly one revision.
		if (FEGQuestRunRecord* Committed = FindMutableRunRecord(Instance))
		{
			Committed->Revision = BeforeRevision + 1;
			MarkRunDirty(Instance);
		}
		UpdatedInstances.Add(Instance);
	}
	if (!bAnyRunAccepted)
	{
		return FEGQuestOperationResult::NoChange({}, 0,
			Event.Sequence != 0 ? FName(TEXT("DuplicateEvent")) : FName(TEXT("NoActiveRuns")));
	}
	if (GetDefault<UEGQuestPluginSettings>()->bGameplayEventsWriteFacts && Event.EventTag.IsValid())
	{
		QueuePostCommit([this, Event]()
		{
			if (UWorld* World = GetWorld())
			{
				if (UEGQuestFactsSubsystem* Facts = World->GetSubsystem<UEGQuestFactsSubsystem>())
				{
					int32 Delta = FMath::RoundToInt(Event.Magnitude);
					if (Delta == 0 && Event.Magnitude >= 0.f) Delta = 1;
					if (Delta != 0) Facts->AddToFact(Event.EventTag, Delta, EEGQuestFactScope::World, nullptr, Event.Instigator);
				}
			}
		});
	}
	QueuePostCommit([this, Event]() { OnGameplayEventAccepted.Broadcast(Event); });
	for (const FGuid& Instance : UpdatedInstances)
	{
		QueuePostCommit([this, Instance]() { OnQuestUpdated.Broadcast(Instance); });
	}
	if (UpdatedInstances.Num() > 0)
	{
		QueuePostCommit([this]() { OnQuestSnapshotsChanged.Broadcast(); });
	}
	FEGQuestOperationResult Result = UpdatedInstances.Num() > 0 || bAnyCursorAdvanced
		? FEGQuestOperationResult::Applied({}, 0, 0)
		: FEGQuestOperationResult::NoChange({}, 0, TEXT("NoMatchingObjective"));
	Result.AffectedElementGuids = MoveTemp(UpdatedInstances);
	return Result;
}

FEGQuestOperationResult UEGQuestComponent::AbandonQuest(FGuid Instance)
{
	return ExecuteOrQueue([this, Instance]() { return AbandonQuestNow(Instance); }, Instance);
}

FEGQuestOperationResult UEGQuestComponent::AbandonQuestNow(FGuid Instance)
{
	const int32 BeforeRevision = GetRunRevision(Instance);
	return SetTerminalState(Instance, EEGQuestLifecycleState::Abandoned)
		? FEGQuestOperationResult::Applied(Instance, BeforeRevision, GetRunRevision(Instance))
		: MakeRejectedResult(Instance, TEXT("AbandonFailed"));
}

FEGQuestOperationResult UEGQuestComponent::FailQuest(FGuid Instance)
{
	return ExecuteOrQueue([this, Instance]() { return FailQuestNow(Instance); }, Instance);
}

FEGQuestOperationResult UEGQuestComponent::FailQuestNow(FGuid Instance)
{
	const int32 BeforeRevision = GetRunRevision(Instance);
	return SetTerminalState(Instance, EEGQuestLifecycleState::Failed)
		? FEGQuestOperationResult::Applied(Instance, BeforeRevision, GetRunRevision(Instance))
		: MakeRejectedResult(Instance, TEXT("FailQuestFailed"));
}

bool UEGQuestComponent::SetTerminalState(FGuid Instance, EEGQuestLifecycleState State)
{
	if (!HasQuestAuthority()) return false;
	FEGQuestRunRecord* Snapshot = FindMutableRunRecord(Instance);
	if (!Snapshot || Snapshot->IsTerminal()) return false;
	if (const UEGQuestNode_Stage* MainStage = GetActiveStage(Instance, TEXT("Main")))
	{
		QueueStageDirectives(Instance, TEXT("Main"), *MainStage, false);
		const EEGQuestStageExitReason ExitReason = State == EEGQuestLifecycleState::Completed
			? EEGQuestStageExitReason::QuestCompleted
			: (State == EEGQuestLifecycleState::Failed ? EEGQuestStageExitReason::QuestFailed : EEGQuestStageExitReason::QuestAbandoned);
		NotifyScriptStageExited(Instance, *MainStage, ExitReason);
	}
	if (const FEGQuestActiveTrackRuntimeSet* Sentinels = ActiveSentinelTracks.Find(Instance))
		for (const FEGQuestActiveTrackRuntime& Track : Sentinels->Tracks)
			if (const UEGQuestNode_Stage* Stage = GetActiveStage(Instance, Track.TrackName))
				QueueStageDirectives(Instance, Track.TrackName, *Stage, false);
	SyncMainTrack(Instance);
	Snapshot = FindMutableRunRecord(Instance);
	Snapshot->LifecycleState = State;
	Snapshot->CompletionServerTime = GetServerTime();
	Snapshot->bTracked = false;
	const EEGQuestTelemetryEventType TelemetryType = State == EEGQuestLifecycleState::Completed
		? EEGQuestTelemetryEventType::Completed
		: (State == EEGQuestLifecycleState::Failed ? EEGQuestTelemetryEventType::Failed : EEGQuestTelemetryEventType::Abandoned);
	EmitTelemetry(TelemetryType, Instance);
	Snapshot->ActiveNodeGuid.Invalidate();
	Snapshot->ActiveStageTitle = FText::GetEmpty();
	Snapshot->ActiveStageDescription = FText::GetEmpty();
	for (FEGQuestTrackState& Track : Snapshot->Tracks)
	{
		for (FEGQuestSnapshotObjective& Line : Track.ActiveObjectives)
		{
			if (!Line.IsResolved()) Line.Outcome = EEGQuestObjectiveOutcome::Obsolete;
			Track.ObjectiveHistory.Add(Line);
		}
		Track.ActiveObjectives.Reset();
		Track.ActiveNodeGuid.Invalidate();
		Track.StageTitle = FText::GetEmpty();
		Track.StageDescription = FText::GetEmpty();
	}
	Snapshot->RoleBindings.RemoveAll([](const FEGQuestRoleBinding& Binding){ return Binding.bStageScoped; });
	Snapshot->ActiveObjectives.Reset();
	QueueLifecycleFactWrite(Instance, State == EEGQuestLifecycleState::Completed ? TEXT("Completed") :
		(State == EEGQuestLifecycleState::Failed ? TEXT("Failed") : TEXT("Abandoned")));
	++Snapshot->Revision;
	MarkRunDirty(Instance);
	ActiveContexts.Remove(Instance);
	DestroyObjectiveEvaluators(Instance);
	if (FEGQuestActiveTrackRuntimeSet* Sentinels = ActiveSentinelTracks.Find(Instance))
	{
		for (FEGQuestActiveTrackRuntime& Track : Sentinels->Tracks)
			for (UEGQuestNode_Objective* Evaluator : Track.Evaluators.Evaluators)
				if (IsValid(Evaluator)) Evaluator->DeactivateEvaluator();
	}
	ActiveSentinelTracks.Remove(Instance);
	NotifyScriptQuestEnded(Instance, State);
	BroadcastUpdated(Instance);
	return true;
}

void UEGQuestComponent::RefreshRunRecord(FGuid Instance, bool bIncrementRevision)
{
	FEGQuestRunRecord* Snapshot = FindMutableRunRecord(Instance);
	TObjectPtr<UEGQuestContext>* ContextPtr = ActiveContexts.Find(Instance);
	if (!Snapshot || !ContextPtr || !*ContextPtr) return;
	UEGQuestContext& Context = **ContextPtr;
	Snapshot->ActiveNodeGuid = Context.GetActiveNodeGUID();
	Snapshot->VisitedNodeGuids = Context.GetVisitedNodeGUIDs().Array();
	if (bIncrementRevision) ++Snapshot->Revision;

	if (Context.HasQuestEnded())
	{
		EEGQuestLifecycleState FinalState = EEGQuestLifecycleState::Completed;
		if (const UEGQuestNode_End* End = Cast<UEGQuestNode_End>(Context.GetActiveNode()))
		{
			switch (End->GetQuestResult())
			{
			case EEGQuestResult::Failed: FinalState = EEGQuestLifecycleState::Failed; break;
			case EEGQuestResult::Abandoned: FinalState = EEGQuestLifecycleState::Abandoned; break;
			default: break;
			}
		}
		SetTerminalState(Instance, FinalState);
		return;
	}
	SyncMainTrack(Instance);
	MarkRunDirty(Instance);
}

FEGQuestOperationResult UEGQuestComponent::SetObjectiveRequiredCount(FGuid Instance, FGuid ObjectiveGuid, int32 RequiredCount)
{
	return ExecuteOrQueue([this, Instance, ObjectiveGuid, RequiredCount]()
	{
		return SetObjectiveRequiredCountNow(Instance, ObjectiveGuid, RequiredCount);
	}, Instance);
}

FEGQuestOperationResult UEGQuestComponent::SetObjectiveRequiredCountNow(FGuid Instance, FGuid ObjectiveGuid, int32 RequiredCount)
{
	if (!HasQuestAuthority()) return MakeRejectedResult(Instance, TEXT("NoAuthority"));
	if (RequiredCount < 0) return MakeRejectedResult(Instance, TEXT("InvalidRequiredCount"));
	if (!ObjectiveGuid.IsValid()) return MakeRejectedResult(Instance, TEXT("InvalidObjectiveId"));
	FEGQuestRunRecord* Snapshot = FindMutableRunRecord(Instance);
	if (!Snapshot) return MakeRejectedResult(Instance, TEXT("UnknownRun"));
	if (Snapshot->IsTerminal()) return MakeRejectedResult(Instance, TEXT("TerminalRun"));
	const int32 BeforeRevision = Snapshot->Revision;

	FEGQuestObjectiveCountOverride* Existing = Snapshot->ObjectiveRequiredCountOverrides.FindByPredicate(
		[ObjectiveGuid](const FEGQuestObjectiveCountOverride& Entry){ return Entry.ObjectiveGuid == ObjectiveGuid; });
	if (Existing && Existing->RequiredCount == RequiredCount)
	{
		return FEGQuestOperationResult::NoChange(Instance, BeforeRevision, TEXT("RequiredCountUnchanged"));
	}

	if (Existing)
	{
		Existing->RequiredCount = RequiredCount;
	}
	else
	{
		FEGQuestObjectiveCountOverride& Added = Snapshot->ObjectiveRequiredCountOverrides.AddDefaulted_GetRef();
		Added.ObjectiveGuid = ObjectiveGuid;
		Added.RequiredCount = RequiredCount;
	}

	// If the objective is already on the checklist its target changes now; if it activates later,
	// RebuildActiveObjectives picks the override up then.
	bool bNowSatisfied = false;
	if (FEGQuestSnapshotObjective* Line = FindMutableObjectiveState(Instance, ObjectiveGuid))
	{
		if (const UEGQuestNode_Objective* Objective = FindActiveObjectiveNode(Instance, ObjectiveGuid))
		{
			Line->RequiredCount = ResolveRequiredCount(*Snapshot, *Objective);
			RefreshObjectivePresentation(Instance, FindTrackNameForObjective(Instance, ObjectiveGuid), *Line, *Objective);
			// Lowering the target under what has already been counted resolves the objective now.
			// Waiting for one more event would strand a quest whose remaining events cannot happen.
			if (!Line->IsResolved() && Line->Progress.Count >= Line->RequiredCount)
			{
				bNowSatisfied = ResolveObjective(Instance, ObjectiveGuid, EEGQuestObjectiveOutcome::Succeeded);
			}
		}
	}

	++Snapshot->Revision;
	// Inside a resolution scope the surrounding flow settles; a plain call settles here.
	if (bNowSatisfied) SettleTrack(Instance, FindTrackNameForObjective(Instance, ObjectiveGuid));
	// The override plus any immediate resolution/settlement is one input transaction.
	if (FEGQuestRunRecord* Committed = FindMutableRunRecord(Instance))
	{
		Committed->Revision = BeforeRevision + 1;
		MarkRunDirty(Instance);
	}
	BroadcastUpdated(Instance);
	return FEGQuestOperationResult::Applied(Instance, BeforeRevision, GetRunRevision(Instance));
}

FEGQuestOperationResult UEGQuestComponent::SetObjectiveRequiredCountByEventTag(FGuid Instance, FGameplayTag EventTag, int32 RequiredCount)
{
	return ExecuteOrQueue([this, Instance, EventTag, RequiredCount]()
	{
		return SetObjectiveRequiredCountByEventTagNow(Instance, EventTag, RequiredCount);
	}, Instance);
}

FEGQuestOperationResult UEGQuestComponent::SetObjectiveRequiredCountByEventTagNow(FGuid Instance, FGameplayTag EventTag, int32 RequiredCount)
{
	TArray<FGuid> Scaled;
	if (!HasQuestAuthority()) return MakeRejectedResult(Instance, TEXT("NoAuthority"));
	if (!EventTag.IsValid()) return MakeRejectedResult(Instance, TEXT("InvalidEventTag"));
	const int32 BeforeRevision = GetRunRevision(Instance);
	const TObjectPtr<UEGQuestContext>* ContextPtr = ActiveContexts.Find(Instance);
	const UEGQuestGraph* Quest = ContextPtr && *ContextPtr ? (*ContextPtr)->GetQuest() : nullptr;
	if (!Quest) return MakeRejectedResult(Instance, TEXT("UnknownOrTerminalRun"));

	// The objective being scaled usually activates much later than the moment its target is known,
	// so this searches the whole graph rather than only the active stage.
	for (const UEGQuestNode* Node : Quest->GetNodes())
	{
		const UEGQuestNode_Objective* Objective = Cast<UEGQuestNode_Objective>(Node);
		if (!Objective || !Objective->GetPresentedEventTag().MatchesTagExact(EventTag)) continue;
		const FEGQuestOperationResult ItemResult = SetObjectiveRequiredCountNow(Instance, Objective->GetGUID(), RequiredCount);
		if (ItemResult.IsSuccess())
		{
			Scaled.Add(Objective->GetGUID());
		}
	}
	if (Scaled.Num() == 0)
	{
		return FEGQuestOperationResult::NoChange(Instance, BeforeRevision, TEXT("NoMatchingObjective"));
	}

	// The fan-out is one public mutation even though it reuses the single-objective implementation.
	if (FEGQuestRunRecord* Snapshot = FindMutableRunRecord(Instance))
	{
		Snapshot->Revision = BeforeRevision + 1;
		MarkRunDirty(Instance);
	}
	FEGQuestOperationResult Result = FEGQuestOperationResult::Applied(Instance, BeforeRevision, GetRunRevision(Instance));
	Result.AffectedElementGuids = MoveTemp(Scaled);
	return Result;
}

bool UEGQuestComponent::FindObjectiveSnapshot(FGuid Instance, FGuid ObjectiveGuid, FEGQuestSnapshotObjective& OutObjective) const
{
	const FEGQuestViewSnapshot* Snapshot = FindSnapshot(Instance);
	if (!Snapshot) return false;
	if (const FEGQuestSnapshotObjective* Line = Snapshot->ActiveObjectives.FindByPredicate(
			[ObjectiveGuid](const FEGQuestSnapshotObjective& Entry){ return Entry.Guid == ObjectiveGuid; }))
	{
		OutObjective = *Line;
		return true;
	}
	for (const FEGQuestTrackState& Track : Snapshot->Tracks)
	{
		if (const FEGQuestSnapshotObjective* Line = Track.ActiveObjectives.FindByPredicate(
			[ObjectiveGuid](const FEGQuestSnapshotObjective& Entry){ return Entry.Guid == ObjectiveGuid; }))
		{ OutObjective = *Line; return true; }
		if (const FEGQuestSnapshotObjective* Line = Track.ObjectiveHistory.FindByPredicate(
			[ObjectiveGuid](const FEGQuestSnapshotObjective& Entry){ return Entry.Guid == ObjectiveGuid; }))
		{ OutObjective = *Line; return true; }
	}
	return false;
}

int32 UEGQuestComponent::GetObjectiveRequiredCountOverride(FGuid Instance, FGuid ObjectiveGuid) const
{
	const FEGQuestRunRecord* Snapshot = FindRunRecord(Instance);
	if (!Snapshot) return 0;
	const FEGQuestObjectiveCountOverride* Override = Snapshot->ObjectiveRequiredCountOverrides.FindByPredicate(
		[ObjectiveGuid](const FEGQuestObjectiveCountOverride& Entry){ return Entry.ObjectiveGuid == ObjectiveGuid; });
	return Override ? Override->RequiredCount : 0;
}

bool UEGQuestComponent::FindQuestSnapshot(FGuid Instance, FEGQuestViewSnapshot& OutSnapshot) const
{
	if (const FEGQuestViewSnapshot* Snapshot = FindSnapshot(Instance)) { OutSnapshot = *Snapshot; return true; }
	return false;
}

const FEGQuestViewSnapshot* UEGQuestComponent::FindSnapshot(FGuid Instance) const
{
	if (const FEGQuestViewSnapshot* Found = SharedQuestSnapshots.Items.FindByPredicate([Instance](const auto& S){ return S.QuestInstanceGuid == Instance; })) return Found;
	if (const FEGQuestViewSnapshot* Found = PrivateQuestSnapshots.Items.FindByPredicate([Instance](const auto& S){ return S.QuestInstanceGuid == Instance; })) return Found;
	return TerminalSnapshotCache.Find(Instance);
}

int32 UEGQuestComponent::GetRunRevision(FGuid Instance) const
{
	const FEGQuestRunRecord* Record = FindRunRecord(Instance);
	return Record ? Record->Revision : 0;
}

FEGQuestOperationResult UEGQuestComponent::MakeRejectedResult(FGuid Instance, FName Reason)
{
	Reject(Instance, Reason);
	return FEGQuestOperationResult::Rejected(Instance, GetRunRevision(Instance), Reason);
}

FEGQuestRunRecord* UEGQuestComponent::FindMutableRunRecord(FGuid Instance, bool* bOutPrivate)
{
	TMap<FGuid, FEGQuestRunRecord>& Records = bTransactionActive ? TransactionRunRecords : RunRecords;
	FEGQuestRunRecord* Record = Records.Find(Instance);
	if (Record && bOutPrivate) *bOutPrivate = Record->bPrivate;
	return Record;
}

const FEGQuestRunRecord* UEGQuestComponent::FindRunRecord(FGuid Instance) const
{
	const TMap<FGuid, FEGQuestRunRecord>& Records = bTransactionActive ? TransactionRunRecords : RunRecords;
	return Records.Find(Instance);
}

FEGQuestSnapshotObjective* UEGQuestComponent::FindMutableObjectiveState(FGuid Instance, FGuid ObjectiveGuid)
{
	FEGQuestRunRecord* Record = FindMutableRunRecord(Instance);
	if (!Record) return nullptr;
	if (FEGQuestSnapshotObjective* Main = Record->ActiveObjectives.FindByPredicate(
		[ObjectiveGuid](const FEGQuestSnapshotObjective& Item){ return Item.Guid == ObjectiveGuid; })) return Main;
	for (FEGQuestTrackState& Track : Record->Tracks)
		if (Track.TrackType != EEGQuestTrackType::Main)
			if (FEGQuestSnapshotObjective* Found = Track.ActiveObjectives.FindByPredicate(
				[ObjectiveGuid](const FEGQuestSnapshotObjective& Item){ return Item.Guid == ObjectiveGuid; })) return Found;
	return nullptr;
}

const FEGQuestSnapshotObjective* UEGQuestComponent::FindObjectiveState(FGuid Instance, FGuid ObjectiveGuid) const
{
	const FEGQuestRunRecord* Record = FindRunRecord(Instance);
	if (!Record) return nullptr;
	if (const FEGQuestSnapshotObjective* Main = Record->ActiveObjectives.FindByPredicate(
		[ObjectiveGuid](const FEGQuestSnapshotObjective& Item){ return Item.Guid == ObjectiveGuid; })) return Main;
	for (const FEGQuestTrackState& Track : Record->Tracks)
		if (Track.TrackType != EEGQuestTrackType::Main)
			if (const FEGQuestSnapshotObjective* Found = Track.ActiveObjectives.FindByPredicate(
				[ObjectiveGuid](const FEGQuestSnapshotObjective& Item){ return Item.Guid == ObjectiveGuid; })) return Found;
	return nullptr;
}

FName UEGQuestComponent::FindTrackNameForObjective(FGuid Instance, FGuid ObjectiveGuid) const
{
	const FEGQuestRunRecord* Record = FindRunRecord(Instance);
	if (!Record) return TEXT("Main");
	for (const FEGQuestTrackState& Track : Record->Tracks)
		if (Track.TrackType != EEGQuestTrackType::Main && Track.ActiveObjectives.ContainsByPredicate(
			[ObjectiveGuid](const FEGQuestSnapshotObjective& Line){ return Line.Guid == ObjectiveGuid; })) return Track.TrackName;
	return TEXT("Main");
}

void UEGQuestComponent::ProjectRun(FGuid Instance)
{
	const FEGQuestRunRecord* Record = RunRecords.Find(Instance);
	if (!Record) return;
	FEGQuestSnapshotArray& Array = Record->bPrivate ? PrivateQuestSnapshots : SharedQuestSnapshots;
	FEGQuestViewSnapshot* View = Array.Items.FindByPredicate(
		[Instance](const FEGQuestViewSnapshot& Item) { return Item.QuestInstanceGuid == Instance; });
	if (!View)
	{
		View = &Array.Items.AddDefaulted_GetRef();
	}
	View->QuestAssetId = Record->QuestAssetId;
	View->QuestGraph = Record->QuestGraph;
	View->GraphGuid = Record->GraphGuid;
	View->DefinitionId = Record->DefinitionId;
	View->QuestInstanceGuid = Record->QuestInstanceGuid;
	View->LifecycleState = Record->LifecycleState;
	View->ActiveNodeGuid = Record->ActiveNodeGuid;
	View->ActiveStageTitle = Record->ActiveStageTitle;
	View->ActiveStageDescription = Record->ActiveStageDescription;
	View->ActiveObjectives = Record->ActiveObjectives;
	View->Tracks = Record->Tracks;
	auto FilterAndSort = [Record](TArray<FEGQuestSnapshotObjective>& Lines)
	{
		if (!Record->bPrivate) Lines.RemoveAll([](const FEGQuestSnapshotObjective& Line){ return Line.bHidden; });
		Lines.StableSort([](const FEGQuestSnapshotObjective& A, const FEGQuestSnapshotObjective& B)
		{
			return A.SortOrder < B.SortOrder;
		});
	};
	FilterAndSort(View->ActiveObjectives);
	for (FEGQuestTrackState& Track : View->Tracks)
	{
		FilterAndSort(Track.ActiveObjectives);
		FilterAndSort(Track.ObjectiveHistory);
	}
	RefreshRoleMarkers(*Record, View->RoleMarkers);
	View->StartServerTime = Record->StartServerTime;
	View->CompletionServerTime = Record->CompletionServerTime;
	View->Revision = Record->Revision;
	View->bTracked = Record->bTracked;
	View->bSuspendedByRoleLoss = Record->bSuspendedByRoleLoss;
	if (Record->IsTerminal())
	{
		TerminalSnapshotCache.Add(Instance, *View);
	}
	else
	{
		TerminalSnapshotCache.Remove(Instance);
	}
	Array.MarkItemDirty(*View);
}

void UEGQuestComponent::PurgeReplicatedTerminalSnapshot(FGuid Instance)
{
	auto RemoveFromArray = [this, Instance](FEGQuestSnapshotArray& Array)
	{
		const int32 Index = Array.Items.IndexOfByPredicate(
			[Instance](const FEGQuestViewSnapshot& Item) { return Item.QuestInstanceGuid == Instance; });
		if (Index == INDEX_NONE) return false;
		const FEGQuestViewSnapshot Removed = Array.Items[Index];
		Array.Items.RemoveAt(Index);
		Array.MarkArrayDirty();
		NotifySnapshotReplicatedRemove(Removed);
		NotifySnapshotsChanged();
		return true;
	};
	if (!RemoveFromArray(SharedQuestSnapshots))
	{
		RemoveFromArray(PrivateQuestSnapshots);
	}
}

void UEGQuestComponent::CapTerminalRunHistory()
{
	TArray<TPair<FGuid, double>> TerminalRuns;
	for (const TPair<FGuid, FEGQuestRunRecord>& Pair : RunRecords)
	{
		if (Pair.Value.IsTerminal())
		{
			TerminalRuns.Emplace(Pair.Key, Pair.Value.CompletionServerTime);
		}
	}
	if (TerminalRuns.Num() <= MaxTerminalRunHistory) return;
	TerminalRuns.Sort([](const TPair<FGuid, double>& A, const TPair<FGuid, double>& B)
	{
		return A.Value < B.Value;
	});
	const int32 RemoveCount = TerminalRuns.Num() - MaxTerminalRunHistory;
	for (int32 Index = 0; Index < RemoveCount; ++Index)
	{
		PurgeTerminalRun(TerminalRuns[Index].Key);
	}
}

void UEGQuestComponent::PurgeTerminalRun(FGuid Instance)
{
	PurgeReplicatedTerminalSnapshot(Instance);
	RunRecords.Remove(Instance);
	TransactionRunRecords.Remove(Instance);
	TerminalSnapshotCache.Remove(Instance);
	ActiveContexts.Remove(Instance);
	ActiveScripts.Remove(Instance);
	ActiveSentinelTracks.Remove(Instance);
	DestroyObjectiveEvaluators(Instance);
}

void UEGQuestComponent::ConvertTimerDeadlinesForSave(FEGQuestRunRecord& Record, double NowServerTime)
{
	auto ConvertLines = [NowServerTime](TArray<FEGQuestSnapshotObjective>& Lines)
	{
		for (FEGQuestSnapshotObjective& Line : Lines)
		{
			if (Line.TrackerEndServerTime > 0.0)
			{
				Line.TrackerEndServerTime = FMath::Max(0.0, Line.TrackerEndServerTime - NowServerTime);
			}
		}
	};
	ConvertLines(Record.ActiveObjectives);
	for (FEGQuestTrackState& Track : Record.Tracks)
	{
		ConvertLines(Track.ActiveObjectives);
		ConvertLines(Track.ObjectiveHistory);
	}
}

void UEGQuestComponent::ConvertTimerDeadlinesForResume(FEGQuestRunRecord& Record, double NowServerTime)
{
	auto ConvertLines = [NowServerTime](TArray<FEGQuestSnapshotObjective>& Lines)
	{
		for (FEGQuestSnapshotObjective& Line : Lines)
		{
			if (Line.TrackerEndServerTime > 0.0)
			{
				Line.TrackerEndServerTime = NowServerTime + Line.TrackerEndServerTime;
			}
		}
	};
	ConvertLines(Record.ActiveObjectives);
	for (FEGQuestTrackState& Track : Record.Tracks)
	{
		ConvertLines(Track.ActiveObjectives);
		ConvertLines(Track.ObjectiveHistory);
	}
}

void UEGQuestComponent::CreateScriptInstance(FGuid Instance, const UEGQuestGraph& QuestGraph)
{
	if (!HasQuestAuthority()) return;
	const TSubclassOf<UEGQuestScript> ScriptClass = QuestGraph.GetQuestScriptClass();
	if (!ScriptClass || ScriptClass->HasAnyClassFlags(CLASS_Abstract)) return;

	UEGQuestScript* Script = NewObject<UEGQuestScript>(this, ScriptClass, NAME_None, RF_Transient);
	Script->Initialize(this, Instance);
	ActiveScripts.Add(Instance, Script);
}

UEGQuestScript* UEGQuestComponent::FindScript(FGuid Instance) const
{
	const TObjectPtr<UEGQuestScript>* ScriptPtr = ActiveScripts.Find(Instance);
	return ScriptPtr ? ScriptPtr->Get() : nullptr;
}

void UEGQuestComponent::NotifyScriptStageEntered(FGuid Instance)
{
	UEGQuestScript* Script = FindScript(Instance);
	const UEGQuestNode_Stage* Stage = Script ? GetActiveStage(Instance) : nullptr;
	if (Stage)
	{
		TWeakObjectPtr<UEGQuestScript> WeakScript = Script;
		const FGuid StageGuid = Stage->GetGUID();
		const FName StageId = Stage->GetStageId();
		const FText StageTitle = Stage->GetTitle();
		QueuePostCommit([WeakScript, StageGuid, StageId, StageTitle]()
		{
			if (WeakScript.IsValid()) WeakScript->HandleStageEntered(StageGuid, StageId, StageTitle);
		});
	}
}

void UEGQuestComponent::NotifyScriptStageExited(FGuid Instance, const UEGQuestNode_Stage& Stage, EEGQuestStageExitReason Reason)
{
	if (UEGQuestScript* Script = FindScript(Instance))
	{
		TWeakObjectPtr<UEGQuestScript> WeakScript = Script;
		const FGuid StageGuid = Stage.GetGUID();
		const FName StageId = Stage.GetStageId();
		QueuePostCommit([WeakScript, StageGuid, StageId, Reason]()
		{
			if (WeakScript.IsValid()) WeakScript->HandleStageExited(StageGuid, StageId, Reason);
		});
	}
}

void UEGQuestComponent::NotifyScriptObjectiveProgress(FGuid Instance, FEGQuestSnapshotObjective ProgressedLine)
{
	if (UEGQuestScript* Script = FindScript(Instance))
	{
		TWeakObjectPtr<UEGQuestScript> WeakScript = Script;
		QueuePostCommit([WeakScript, ProgressedLine = MoveTemp(ProgressedLine)]()
		{
			if (WeakScript.IsValid()) WeakScript->HandleObjectiveProgress(ProgressedLine);
		});
	}
}

void UEGQuestComponent::NotifyScriptRoleLost(FGuid Instance, FName RoleName)
{
	if (UEGQuestScript* Script = FindScript(Instance))
	{
		TWeakObjectPtr<UEGQuestScript> WeakScript = Script;
		QueuePostCommit([WeakScript, RoleName]()
		{
			if (WeakScript.IsValid()) WeakScript->HandleRoleLost(RoleName);
		});
	}
}

void UEGQuestComponent::NotifyScriptObjectiveResolved(FGuid Instance, FEGQuestSnapshotObjective ResolvedLine)
{
	if (UEGQuestScript* Script = FindScript(Instance))
	{
		Script->HandleObjectiveResolved(ResolvedLine);
	}
}

void UEGQuestComponent::NotifyScriptQuestEnded(FGuid Instance, EEGQuestLifecycleState State)
{
	UEGQuestScript* Script = FindScript(Instance);
	if (!Script) return;

	// The script is retired before the callback: a script that reacts to the end by starting or
	// abandoning quests must not be re-entered for this already-finished instance.
	ActiveScripts.Remove(Instance);

	EEGQuestResult Result = EEGQuestResult::Completed;
	switch (State)
	{
		case EEGQuestLifecycleState::Failed: Result = EEGQuestResult::Failed; break;
		case EEGQuestLifecycleState::Abandoned: Result = EEGQuestResult::Abandoned; break;
		default: break;
	}
	TWeakObjectPtr<UEGQuestScript> WeakScript = Script;
	QueuePostCommit([WeakScript, Result]() { if (WeakScript.IsValid()) WeakScript->HandleQuestEnded(Result); });
}

void UEGQuestComponent::Reject(FGuid Instance, FName Reason)
{
	LastRejectReason = Reason;
	QueuePostCommit([this, Instance, Reason]() { OnQuestRequestRejected.Broadcast(Instance, Reason); });
}

void UEGQuestComponent::BroadcastUpdated(FGuid Instance)
{
	QueuePostCommit([this, Instance]() { OnQuestUpdated.Broadcast(Instance); });
	QueuePostCommit([this]() { OnQuestSnapshotsChanged.Broadcast(); });
}

void UEGQuestComponent::NotifySnapshotReplicatedAdd(const FEGQuestViewSnapshot& Snapshot) { OnQuestStarted.Broadcast(Snapshot.QuestInstanceGuid); }
void UEGQuestComponent::NotifySnapshotReplicatedChange(const FEGQuestViewSnapshot& Snapshot) { OnQuestUpdated.Broadcast(Snapshot.QuestInstanceGuid); }
void UEGQuestComponent::NotifySnapshotReplicatedRemove(const FEGQuestViewSnapshot& Snapshot) { OnQuestRemoved.Broadcast(Snapshot.QuestInstanceGuid); }
void UEGQuestComponent::NotifySnapshotsChanged() { OnQuestSnapshotsChanged.Broadcast(); }

bool UEGQuestComponent::HasQuestAuthority() const { return GetOwner() && GetOwner()->HasAuthority(); }
double UEGQuestComponent::GetServerTime() const
{
	if (QuestClock) return QuestClock->GetServerTime();
	// No default clock is constructed or cached here. Caching one would capture the world this call
	// happened to see, and a call that saw none would pin the component to 0.0 forever; see SetQuestClock.
	const UWorld* World = GetWorld(); if (!World) return 0.0;
	if (const AGameStateBase* GS = World->GetGameState()) return GS->GetServerWorldTimeSeconds();
	return World->GetTimeSeconds();
}
