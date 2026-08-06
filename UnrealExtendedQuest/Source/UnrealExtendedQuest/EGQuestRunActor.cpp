// Copyright Devil of the Plague. All Rights Reserved.
#include "EGQuestRunActor.h"

#include "EGQuestComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

namespace
{
	/** A client can derive the exit reason exactly: still running means the stage merely advanced. */
	EEGQuestStageExitReason StageExitReasonFor(const EEGQuestLifecycleState State)
	{
		switch (State)
		{
			case EEGQuestLifecycleState::Completed: return EEGQuestStageExitReason::QuestCompleted;
			case EEGQuestLifecycleState::Failed:    return EEGQuestStageExitReason::QuestFailed;
			case EEGQuestLifecycleState::Abandoned: return EEGQuestStageExitReason::QuestAbandoned;
			default:                                return EEGQuestStageExitReason::Advanced;
		}
	}

	EEGQuestStageExitReason StageExitReasonFor(const EEGQuestResult Result)
	{
		switch (Result)
		{
			case EEGQuestResult::Failed:    return EEGQuestStageExitReason::QuestFailed;
			case EEGQuestResult::Abandoned: return EEGQuestStageExitReason::QuestAbandoned;
			default:                        return EEGQuestStageExitReason::QuestCompleted;
		}
	}
}

AEGQuestRunActor::AEGQuestRunActor()
{
	// Left on so a Blueprint subclass's Event Tick works without the author discovering that the
	// class silently disabled it. A run actor that does not need it can turn it off in its own ctor.
	PrimaryActorTick.bCanEverTick = true;

	bReplicates = true;
	// Overridden per run by the component: a private run is owned by its PlayerState and becomes
	// owner-only instead, so one player's private quest never reaches another player's client.
	bAlwaysRelevant = true;
	SetReplicateMovement(false);
	SetCanBeDamaged(false);
}

void AEGQuestRunActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AEGQuestRunActor, QuestComponent);
	DOREPLIFETIME(AEGQuestRunActor, QuestInstanceGuid);
	DOREPLIFETIME(AEGQuestRunActor, FinalResult);
	DOREPLIFETIME(AEGQuestRunActor, bRunEnded);
}

void AEGQuestRunActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	EndClientTracking();
	Super::EndPlay(EndPlayReason);
}

void AEGQuestRunActor::Initialize(UEGQuestComponent* InOwnerComponent, const FGuid& InQuestInstanceGuid)
{
	QuestComponent = InOwnerComponent;
	QuestInstanceGuid = InQuestInstanceGuid;
}

//
// Authority-side entry points.
//

void AEGQuestRunActor::HandleQuestStarted()
{
	if (bQuestStartedDispatched)
	{
		return;
	}
	bQuestStartedDispatched = true;
	OnQuestStarted();
}

void AEGQuestRunActor::HandleStageEntered(const FGuid& StageGuid, const FName StageId, const FText& StageTitle)
{
	LastStageGuid = StageGuid;
	LastStageId = StageId;
	OnStageEntered(StageGuid, StageId, StageTitle);
}

void AEGQuestRunActor::HandleStageExited(const FGuid& StageGuid, const FName StageId, const EEGQuestStageExitReason Reason)
{
	// Clearing the baseline is what stops DispatchQuestEnded closing this stage a second time on the
	// authority, where the component already reported the exit.
	if (LastStageGuid == StageGuid)
	{
		LastStageGuid.Invalidate();
		LastStageId = NAME_None;
	}
	OnStageExited(StageGuid, StageId, Reason);
}

void AEGQuestRunActor::HandleObjectiveProgress(const FEGQuestSnapshotObjective& Objective)
{
	OnObjectiveProgress(Objective);
}

void AEGQuestRunActor::HandleObjectiveResolved(const FEGQuestSnapshotObjective& Objective)
{
	OnObjectiveResolved(Objective);
}

void AEGQuestRunActor::HandleRoleLost(const FName RoleName)
{
	OnRoleLost(RoleName);
}

void AEGQuestRunActor::HandleQuestEnded(const EEGQuestResult Result)
{
	DispatchQuestEnded(Result);

	// Replicate the outcome, then go. Destroying in the same breath would race the property update
	// and clients would never see OnQuestEnded; the terminal snapshot cannot cover for it either,
	// because a terminal run is purged from the replicated array.
	FinalResult = Result;
	bRunEnded = true;
	SetLifeSpan(1.0f);
}

//
// Replication callbacks - the client half of the same lifecycle.
//

void AEGQuestRunActor::OnRep_QuestBinding()
{
	BeginClientTracking();
}

void AEGQuestRunActor::OnRep_RunEnded()
{
	if (bRunEnded)
	{
		DispatchQuestEnded(FinalResult);
		EndClientTracking();
	}
}

void AEGQuestRunActor::DispatchQuestEnded(const EEGQuestResult Result)
{
	if (bQuestEndedDispatched)
	{
		return;
	}
	bQuestEndedDispatched = true;

	// Close an open stage first. A client cannot observe this from the snapshot - a terminal run is
	// purged from the replicated array, so SyncClientState never sees the stage go - and without it
	// OnStageExited would silently not fire on clients for the last stage, which is exactly the
	// teardown call per-stage setup relies on. The authority has already exited its stage by now
	// (HandleStageExited cleared the baseline), so this only ever fires once per side.
	if (LastStageGuid.IsValid())
	{
		const FGuid ClosingStageGuid = LastStageGuid;
		const FName ClosingStageId = LastStageId;
		LastStageGuid.Invalidate();
		LastStageId = NAME_None;
		OnStageExited(ClosingStageGuid, ClosingStageId, StageExitReasonFor(Result));
	}

	OnQuestEnded(Result);
}

void AEGQuestRunActor::BeginClientTracking()
{
	// Both halves of the binding replicate independently, so this runs on whichever arrives last.
	if (bClientTrackingBound || HasAuthority() || !QuestComponent || !QuestInstanceGuid.IsValid())
	{
		return;
	}
	bClientTrackingBound = true;
	QuestComponent->OnQuestSnapshotsChanged.AddDynamic(this, &AEGQuestRunActor::HandleSnapshotsChanged);

	// The actor arriving is the start, as far as this client is concerned. Firing here rather than
	// off the snapshot means a client that has the actor but not yet the snapshot is still told.
	if (!bQuestStartedDispatched)
	{
		bQuestStartedDispatched = true;
		OnQuestStarted();
	}
	SyncClientState();
}

void AEGQuestRunActor::EndClientTracking()
{
	if (!bClientTrackingBound)
	{
		return;
	}
	bClientTrackingBound = false;
	if (QuestComponent)
	{
		QuestComponent->OnQuestSnapshotsChanged.RemoveDynamic(this, &AEGQuestRunActor::HandleSnapshotsChanged);
	}
}

void AEGQuestRunActor::HandleSnapshotsChanged()
{
	SyncClientState();
}

void AEGQuestRunActor::SyncClientState()
{
	FEGQuestViewSnapshot Snapshot;
	if (!GetQuestSnapshot(Snapshot))
	{
		// Not replicated yet, or already purged because the run is terminal. Either way there is
		// nothing to diff against; the run-ended property carries the outcome.
		return;
	}

	if (Snapshot.ActiveNodeGuid != LastStageGuid)
	{
		if (LastStageGuid.IsValid())
		{
			OnStageExited(LastStageGuid, LastStageId, StageExitReasonFor(Snapshot.LifecycleState));
		}
		LastStageGuid = Snapshot.ActiveNodeGuid;
		LastStageId = Snapshot.ActiveStageId;
		if (LastStageGuid.IsValid())
		{
			// A client joining mid-run lands here for the stage that is already active, so per-stage
			// setup catches up without the subclass needing a separate "what did I miss" path.
			OnStageEntered(LastStageGuid, LastStageId, Snapshot.ActiveStageTitle);
		}
	}

	for (const FEGQuestSnapshotObjective& Line : Snapshot.ActiveObjectives)
	{
		const FEGQuestSnapshotObjective* Previous = LastObjectives.FindByPredicate(
			[&Line](const FEGQuestSnapshotObjective& Item) { return Item.Guid == Line.Guid; });

		const bool bWasPending = !Previous || Previous->Outcome == EEGQuestObjectiveOutcome::Pending;
		if (Line.Outcome != EEGQuestObjectiveOutcome::Pending)
		{
			if (bWasPending)
			{
				OnObjectiveResolved(Line);
			}
			continue;
		}
		if (Previous && Previous->Count != Line.Count)
		{
			OnObjectiveProgress(Line);
		}
	}
	LastObjectives = Snapshot.ActiveObjectives;
}

//
// Queries.
//

bool AEGQuestRunActor::GetQuestSnapshot(FEGQuestViewSnapshot& OutSnapshot) const
{
	return QuestComponent && QuestComponent->FindQuestSnapshot(QuestInstanceGuid, OutSnapshot);
}

int32 AEGQuestRunActor::GetFact(const FGameplayTag Tag, const EEGQuestFactScope Scope, APlayerState* Player) const
{
	const UWorld* World = GetWorld();
	const UEGQuestFactsSubsystem* Facts = World ? World->GetSubsystem<UEGQuestFactsSubsystem>() : nullptr;
	return Facts ? Facts->GetFact(Tag, Scope, Player) : 0;
}

bool AEGQuestRunActor::HasFact(const FGameplayTag Tag, const EEGQuestFactScope Scope, APlayerState* Player) const
{
	const UWorld* World = GetWorld();
	const UEGQuestFactsSubsystem* Facts = World ? World->GetSubsystem<UEGQuestFactsSubsystem>() : nullptr;
	return Facts && Facts->HasFact(Tag, Scope, Player);
}

bool AEGQuestRunActor::SetFact(const FGameplayTag Tag, const int32 Value, const EEGQuestFactScope Scope, APlayerState* Player)
{
	UWorld* World = GetWorld();
	UEGQuestFactsSubsystem* Facts = World ? World->GetSubsystem<UEGQuestFactsSubsystem>() : nullptr;
	return Facts && Facts->SetFact(Tag, Value, Scope, Player, this);
}

bool AEGQuestRunActor::AddToFact(const FGameplayTag Tag, const int32 Delta, const EEGQuestFactScope Scope, APlayerState* Player)
{
	UWorld* World = GetWorld();
	UEGQuestFactsSubsystem* Facts = World ? World->GetSubsystem<UEGQuestFactsSubsystem>() : nullptr;
	return Facts && Facts->AddToFact(Tag, Delta, Scope, Player, this);
}

TArray<AActor*> AEGQuestRunActor::ResolveRoleActors(const FName RoleName) const
{
	return QuestComponent ? QuestComponent->ResolveRoleActors(QuestInstanceGuid, RoleName) : TArray<AActor*>();
}

bool AEGQuestRunActor::GetRoleTransform(const FName RoleName, FTransform& OutTransform) const
{
	return QuestComponent && QuestComponent->GetRoleTransform(QuestInstanceGuid, RoleName, OutTransform);
}

FText AEGQuestRunActor::GetRoleDisplayText(const FName RoleName) const
{
	return QuestComponent ? QuestComponent->GetRoleDisplayText(QuestInstanceGuid, RoleName) : FText::GetEmpty();
}

//
// Driving the quest. Each of these is authority-gated inside the component.
//

bool AEGQuestRunActor::EmitQuestEvent(const FGameplayTag EventTag, const float Magnitude, const FName ContextName)
{
	if (!QuestComponent || !EventTag.IsValid())
	{
		return false;
	}

	FEGQuestGameplayEvent Event;
	Event.EventTag = EventTag;
	Event.Magnitude = Magnitude;
	Event.ContextName = ContextName;
	return QuestComponent->NotifyGameplayEvent(Event);
}

bool AEGQuestRunActor::CompleteObjective(const FGuid ObjectiveGuid)
{
	return QuestComponent && QuestComponent->CompleteActiveObjective(QuestInstanceGuid, ObjectiveGuid);
}

bool AEGQuestRunActor::FailObjective(const FGuid ObjectiveGuid)
{
	return QuestComponent && QuestComponent->FailActiveObjective(QuestInstanceGuid, ObjectiveGuid);
}

bool AEGQuestRunActor::SetObjectiveRequiredCount(const FGuid ObjectiveGuid, const int32 RequiredCount)
{
	return QuestComponent && QuestComponent->SetObjectiveRequiredCount(QuestInstanceGuid, ObjectiveGuid, RequiredCount);
}

TArray<FGuid> AEGQuestRunActor::SetObjectiveRequiredCountByEventTag(const FGameplayTag EventTag, const int32 RequiredCount)
{
	if (!QuestComponent)
	{
		return {};
	}
	return QuestComponent->SetObjectiveRequiredCountByEventTag(QuestInstanceGuid, EventTag, RequiredCount).AffectedElementGuids;
}
