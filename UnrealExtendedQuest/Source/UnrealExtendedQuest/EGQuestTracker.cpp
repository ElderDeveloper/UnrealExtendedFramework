// Copyright Devil of the Plague. All Rights Reserved.
#include "EGQuestTracker.h"

#include "EGQuestComponent.h"
#include "EGQuestFactsSubsystem.h"
#include "Nodes/EGQuestNode_Objective.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"

namespace
{
	bool MatchesEvent(const FGameplayTag& Tag, const FGameplayTagQuery& Query, const FGameplayTag& Exact)
	{
		if (!Query.IsEmpty())
		{
			FGameplayTagContainer Container(Tag);
			return Query.Matches(Container);
		}
		return Exact.IsValid() && Tag.MatchesTagExact(Exact);
	}
}

void UEGQuestTracker::Activate(UEGQuestNode_Objective& InBinding, EEGQuestActivationReason Reason)
{
	Binding = &InBinding;
	bSatisfied = false;
	OnActivated(Reason);
}

void UEGQuestTracker::Deactivate(EEGQuestDeactivationReason Reason)
{
	if (!Binding.IsValid()) return;
	OnDeactivated(Reason);
	Binding.Reset();
	ParentTracker.Reset();
}

void UEGQuestTracker::HandleEvent(const FEGQuestGameplayEvent& Event, int32 ProgressDelta)
{
	if (Binding.IsValid() && !bSatisfied) OnGameplayEvent(Event, ProgressDelta);
}

void UEGQuestTracker::Pulse()
{
	if (Binding.IsValid() && !bSatisfied) OnPulse();
}

void UEGQuestTracker::Satisfy()
{
	if (bSatisfied || !Binding.IsValid()) return;
	bSatisfied = true;
	if (UEGQuestTracker* Parent = ParentTracker.Get()) Parent->ChildSatisfied(*this);
	else Binding->CompleteObjective(OutcomeOnSatisfied != EEGQuestObjectiveOutcome::Failed);
}

void UEGQuestTracker::ChildSatisfied(UEGQuestTracker& Child) {}

FEGQuestPredicateContext UEGQuestTracker::MakePredicateContext() const
{
	FEGQuestPredicateContext Context;
	if (const UEGQuestNode_Objective* Objective = Binding.Get())
	{
		Context.Component = Objective->GetQuestComponent();
		Context.RunId = Objective->GetQuestInstanceGuid();
		Context.ObjectiveGuid = Objective->GetGUID();
		Context.Player = ResolvePlayerScope();
	}
	return Context;
}

APlayerState* UEGQuestTracker::ResolvePlayerScope() const
{
	const UEGQuestNode_Objective* Objective = Binding.Get();
	const UEGQuestComponent* Component = Objective ? Objective->GetQuestComponent() : nullptr;
	return Component ? Cast<APlayerState>(Component->GetOwner()) : nullptr;
}

void UEGQuestTracker_Fact::OnActivated_Implementation(EEGQuestActivationReason Reason)
{
	if (const FEGQuestPredicateContext Context = MakePredicateContext(); Context.Component)
	{
		if (UWorld* World = Context.Component->GetWorld())
		{
			if (UEGQuestFactsSubsystem* Facts = World->GetSubsystem<UEGQuestFactsSubsystem>())
			{
				Facts->OnFactChanged.AddDynamic(this, &UEGQuestTracker_Fact::HandleFactChanged);
			}
		}
	}
	OnPulse_Implementation();
}

void UEGQuestTracker_Fact::OnDeactivated_Implementation(EEGQuestDeactivationReason Reason)
{
	if (const FEGQuestPredicateContext Context = MakePredicateContext(); Context.Component)
		if (UWorld* World = Context.Component->GetWorld())
			if (UEGQuestFactsSubsystem* Facts = World->GetSubsystem<UEGQuestFactsSubsystem>())
				Facts->OnFactChanged.RemoveDynamic(this, &UEGQuestTracker_Fact::HandleFactChanged);
}

void UEGQuestTracker_Fact::OnPulse_Implementation()
{
	const FEGQuestPredicateContext Context = MakePredicateContext();
	const UWorld* World = Context.Component ? Context.Component->GetWorld() : nullptr;
	const UEGQuestFactsSubsystem* Facts = World ? World->GetSubsystem<UEGQuestFactsSubsystem>() : nullptr;
	if (Facts && FactTag.IsValid() && EGQuestCompareNumber(Facts->GetFact(FactTag, Scope, Context.Player), Comparison, Value, MaxValue)) Satisfy();
}

void UEGQuestTracker_Fact::HandleFactChanged(FGameplayTag Tag, int32 OldValue, int32 NewValue,
	EEGQuestFactScope ChangedScope, APlayerState* Player, AActor* Instigator)
{
	if (!Tag.MatchesTagExact(FactTag) || ChangedScope != Scope) return;
	if (Scope == EEGQuestFactScope::World)
	{
		OnPulse_Implementation();
		return;
	}
	APlayerState* Expected = ResolvePlayerScope();
	if (!Expected) return;
	// Restore/replication may broadcast with a null Player after writing by ExplicitPlayerId.
	// OnPulse reads through Expected, so a no-op pulse is safe when the fact belongs to someone else.
	if (Player == Expected || Player == nullptr)
	{
		OnPulse_Implementation();
	}
}

void UEGQuestTracker_EventCount::OnGameplayEvent_Implementation(const FEGQuestGameplayEvent& Event, int32 ProgressDelta)
{
	if (!MatchesEvent(Event.EventTag, EventQuery, ExactEventTag)) return;
	if (ProgressDelta == 0) return;
	if (bUseMagnitudePredicate && !EGQuestCompareNumber(Event.Magnitude, MagnitudeComparison, MagnitudeValue, MaxMagnitudeValue)) return;
	UEGQuestNode_Objective* Objective = GetBinding();
	if (!Objective) return;
	// Failure trackers own FailCount / FailRequiredCount. Success trackers may use a per-run override
	// of RequiredCount through the component (GetRequiredProgress).
	if (OutcomeOnSatisfied == EEGQuestObjectiveOutcome::Failed)
	{
		if (Objective->AddFailProgress(ProgressDelta) &&
			Objective->GetFailProgress() >= FMath::Max(1, RequiredCount))
		{
			Satisfy();
		}
		return;
	}
	if (Objective->AddProgress(ProgressDelta) &&
		Objective->GetProgress() >= Objective->GetRequiredProgress())
	{
		Satisfy();
	}
}

void UEGQuestTracker_Timer::OnActivated_Implementation(EEGQuestActivationReason Reason)
{
	UEGQuestNode_Objective* Objective = GetBinding();
	if (!Objective) return;
	if (Objective->GetTrackerEndServerTime() <= 0.0)
		Objective->SetTrackerEndServerTime(Objective->GetQuestComponent()->GetQuestServerTime() + FMath::Max(0.0, DurationSeconds));
	OnPulse_Implementation();
}

void UEGQuestTracker_Timer::OnPulse_Implementation()
{
	UEGQuestNode_Objective* Objective = GetBinding();
	if (Objective && Objective->GetTrackerEndServerTime() > 0.0 &&
		Objective->GetQuestComponent()->GetQuestServerTime() >= Objective->GetTrackerEndServerTime()) Satisfy();
}

void UEGQuestTracker_Sequence::OnGameplayEvent_Implementation(const FEGQuestGameplayEvent& Event, int32 ProgressDelta)
{
	UEGQuestNode_Objective* Objective = GetBinding();
	if (!Objective || OrderedTags.IsEmpty()) return;
	const int32 Index = FMath::Clamp(Objective->GetTrackerSequenceIndex(), 0, OrderedTags.Num());
	if (Index < OrderedTags.Num() && Event.EventTag.MatchesTagExact(OrderedTags[Index]))
	{
		Objective->SetTrackerSequenceIndex(Index + 1);
		if (Index + 1 >= OrderedTags.Num()) Satisfy();
		return;
	}
	if (MismatchPolicy == EEGQuestSequenceMismatchPolicy::Reset)
		Objective->SetTrackerSequenceIndex(Event.EventTag.MatchesTagExact(OrderedTags[0]) ? 1 : 0);
	else if (MismatchPolicy == EEGQuestSequenceMismatchPolicy::Fail)
	{
		OutcomeOnSatisfied = EEGQuestObjectiveOutcome::Failed;
		Satisfy();
	}
}

void UEGQuestTracker_Distinct::OnGameplayEvent_Implementation(const FEGQuestGameplayEvent& Event, int32 ProgressDelta)
{
	if (!MatchesEvent(Event.EventTag, EventQuery, ExactEventTag)) return;
	FName Key = Event.ContextName;
	if (Key.IsNone() && Event.Target) Key = Event.Target->GetFName();
	if (Key.IsNone()) return;
	UEGQuestNode_Objective* Objective = GetBinding();
	if (Objective && Objective->AddTrackerDistinctKey(Key) >= Objective->GetRequiredProgress()) Satisfy();
}

void UEGQuestTracker_Composite::OnActivated_Implementation(EEGQuestActivationReason Reason)
{
	for (UEGQuestTracker* Child : Children)
	{
		if (!Child || !GetBinding()) continue;
		Child->ParentTracker = this;
		Child->Activate(*GetBinding(), Reason);
	}
	EvaluateChildren();
}

void UEGQuestTracker_Composite::OnDeactivated_Implementation(EEGQuestDeactivationReason Reason)
{
	for (UEGQuestTracker* Child : Children) if (Child) Child->Deactivate(Reason);
}

void UEGQuestTracker_Composite::OnGameplayEvent_Implementation(const FEGQuestGameplayEvent& Event, int32 ProgressDelta)
{
	for (UEGQuestTracker* Child : Children) if (Child) Child->HandleEvent(Event, ProgressDelta);
	EvaluateChildren();
}

void UEGQuestTracker_Composite::OnPulse_Implementation()
{
	for (UEGQuestTracker* Child : Children) if (Child) Child->Pulse();
	EvaluateChildren();
}

void UEGQuestTracker_Composite::ChildSatisfied(UEGQuestTracker& Child) { EvaluateChildren(); }

void UEGQuestTracker_Composite::EvaluateChildren()
{
	int32 Valid = 0, Satisfied = 0;
	for (const UEGQuestTracker* Child : Children) if (Child) { ++Valid; if (Child->IsSatisfied()) ++Satisfied; }
	const int32 Needed = Mode == EEGQuestCompositeMode::Any ? 1 : Mode == EEGQuestCompositeMode::All ? Valid : FMath::Clamp(RequiredChildren, 1, FMath::Max(1, Valid));
	if (Valid > 0 && Satisfied >= Needed) Satisfy();
}

void UEGQuestTracker_Predicate::OnActivated_Implementation(EEGQuestActivationReason Reason)
{
	Dependencies.Reset();
	if (Predicate) Predicate->GetFactDependencies(Dependencies);
	const FEGQuestPredicateContext Context = MakePredicateContext();
	if (!Dependencies.IsEmpty() && Context.Component)
		if (UWorld* World = Context.Component->GetWorld())
			if (UEGQuestFactsSubsystem* Facts = World->GetSubsystem<UEGQuestFactsSubsystem>())
				Facts->OnFactChanged.AddDynamic(this, &UEGQuestTracker_Predicate::HandleFactChanged);
	OnPulse_Implementation();
}

void UEGQuestTracker_Predicate::OnDeactivated_Implementation(EEGQuestDeactivationReason Reason)
{
	const FEGQuestPredicateContext Context = MakePredicateContext();
	if (Context.Component)
		if (UWorld* World = Context.Component->GetWorld())
			if (UEGQuestFactsSubsystem* Facts = World->GetSubsystem<UEGQuestFactsSubsystem>())
				Facts->OnFactChanged.RemoveDynamic(this, &UEGQuestTracker_Predicate::HandleFactChanged);
}

void UEGQuestTracker_Predicate::OnPulse_Implementation()
{
	if (Predicate && Predicate->Evaluate(MakePredicateContext())) Satisfy();
}

void UEGQuestTracker_Predicate::HandleFactChanged(FGameplayTag Tag, int32 OldValue, int32 NewValue,
	EEGQuestFactScope ChangedScope, APlayerState* Player, AActor* Instigator)
{
	if (Dependencies.HasTagExact(Tag)) OnPulse_Implementation();
}
