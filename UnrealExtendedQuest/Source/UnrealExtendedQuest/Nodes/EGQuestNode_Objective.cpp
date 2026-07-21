// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#include "EGQuestNode_Objective.h"

#include "UnrealExtendedQuest/EGQuestComponent.h"
#include "UnrealExtendedQuest/EGQuestContext.h"
#include "UnrealExtendedQuest/EGQuestConstants.h"
#include "UnrealExtendedQuest/Logging/EGQuestLogger.h"
#include "UnrealExtendedQuest/EGQuestLocalizationHelper.h"
#include "UnrealExtendedQuest/EGQuestTracker.h"


void UEGQuestNode_Objective::PostLoad()
{
	Super::PostLoad();
	EnsureTrackers();
}

void UEGQuestNode_Objective::EnsureTrackers()
{
	// Legacy-asset migration: quests saved before tracker-only authoring store EventTag/RequiredCount
	// style fields rather than a Tracker subobject, and this turns them into ordinary tracker
	// templates. New authoring writes the Tracker slots directly and never touches the fields.
	//
	// An authored Tracker always wins, and running twice is a no-op, so both entry points can call it.
	if (!Tracker)
	{
		if (bCompleteOnStageEnter)
		{
			Tracker = NewObject<UEGQuestTracker_Immediate>(this, NAME_None, RF_Transactional);
		}
		else if (EventTag.IsValid())
		{
			UEGQuestTracker_EventCount* Counting = NewObject<UEGQuestTracker_EventCount>(this, NAME_None, RF_Transactional);
			Counting->ExactEventTag = EventTag;
			Counting->RequiredCount = FMath::Max(1, RequiredCount);
			Tracker = Counting;
		}
	}
	if (!FailureTracker && FailEventTag.IsValid())
	{
		UEGQuestTracker_EventCount* Counting = NewObject<UEGQuestTracker_EventCount>(this, NAME_None, RF_Transactional);
		Counting->ExactEventTag = FailEventTag;
		Counting->RequiredCount = FMath::Max(1, FailRequiredCount);
		Counting->OutcomeOnSatisfied = EEGQuestObjectiveOutcome::Failed;
		FailureTracker = Counting;
	}
}

bool UEGQuestNode_Objective::CanEverFail() const
{
	// A legacy asset's FailEventTag becomes a FailureTracker in EnsureTrackers before anything asks,
	// so the trackers are the whole answer.
	return FailureTracker != nullptr ||
		(Tracker && Tracker->OutcomeOnSatisfied == EEGQuestObjectiveOutcome::Failed);
}

int32 UEGQuestNode_Objective::GetPresentedRequiredCount() const
{
	if (const UEGQuestTracker_EventCount* Counting = Cast<UEGQuestTracker_EventCount>(Tracker))
		return FMath::Max(1, Counting->RequiredCount);
	if (const UEGQuestTracker_Distinct* Distinct = Cast<UEGQuestTracker_Distinct>(Tracker))
		return FMath::Max(1, Distinct->RequiredDistinctCount);
	if (const UEGQuestTracker_Sequence* Sequence = Cast<UEGQuestTracker_Sequence>(Tracker))
		return FMath::Max(1, Sequence->OrderedTags.Num());
	// Legacy fallback: an unmigrated node presents its authored count until EnsureTrackers runs.
	return FMath::Max(1, RequiredCount);
}

FGameplayTag UEGQuestNode_Objective::GetPresentedEventTag() const
{
	if (const UEGQuestTracker_EventCount* Counting = Cast<UEGQuestTracker_EventCount>(Tracker))
		return Counting->ExactEventTag;
	if (const UEGQuestTracker_Distinct* Distinct = Cast<UEGQuestTracker_Distinct>(Tracker))
		return Distinct->ExactEventTag;
	return EventTag;
}

#if WITH_EDITOR
void UEGQuestNode_Objective::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.Property != nullptr ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	const bool bTextChanged = PropertyName == GetMemberNameText();

	// rebuild text arguments
	if (bTextChanged || PropertyName == GetMemberNameTextArguments())
	{
		RebuildTextArguments();
	}
}
#endif

void UEGQuestNode_Objective::UpdateTextsValuesFromDefaultsAndRemappings(const UEGQuestPluginSettings& Settings, bool bUpdateGraphNode)
{
	FEGQuestLocalizationHelper::UpdateTextFromRemapping(Settings, Text);
	Super::UpdateTextsValuesFromDefaultsAndRemappings(Settings, bUpdateGraphNode);
}

void UEGQuestNode_Objective::UpdateTextsNamespacesAndKeys(const UEGQuestPluginSettings& Settings, bool bUpdateGraphNode)
{
	FEGQuestLocalizationHelper::UpdateTextNamespaceAndKey(GetOuter(), Settings, Text);
	Super::UpdateTextsNamespacesAndKeys(Settings, bUpdateGraphNode);
}

void UEGQuestNode_Objective::RebuildConstructedText(UEGQuestContext& Context) const
{
	if (TextArguments.Num() <= 0)
	{
		return;
	}

	FFormatNamedArguments OrderedArguments;
	for (const FEGQuestTextArgument& QuestArgument : TextArguments)
	{
		OrderedArguments.Add(QuestArgument.DisplayString, QuestArgument.ConstructFormatArgumentValue(Context));
	}
	// Stored per context - never on this (shared) node object.
	Context.SetConstructedNodeText(NodeGUID, FText::Format(Text, OrderedArguments));
}

//
// Editor support
//

namespace
{
	/** One-line card summary of a tracker template. */
	FString EGQuestDescribeTracker(const UEGQuestTracker* Tracker)
	{
		if (const UEGQuestTracker_EventCount* Counting = Cast<UEGQuestTracker_EventCount>(Tracker))
		{
			const FString Source = Counting->ExactEventTag.IsValid()
				? Counting->ExactEventTag.ToString()
				: (Counting->EventQuery.IsEmpty() ? TEXT("(no event)") : TEXT("(query)"));
			return Counting->RequiredCount > 1
				? FString::Printf(TEXT("%s x%d"), *Source, Counting->RequiredCount)
				: Source;
		}
		if (const UEGQuestTracker_Distinct* Distinct = Cast<UEGQuestTracker_Distinct>(Tracker))
		{
			return FString::Printf(TEXT("%d distinct: %s"), Distinct->RequiredDistinctCount,
				Distinct->ExactEventTag.IsValid() ? *Distinct->ExactEventTag.ToString() : TEXT("(query)"));
		}
		if (const UEGQuestTracker_Sequence* Sequence = Cast<UEGQuestTracker_Sequence>(Tracker))
		{
			return FString::Printf(TEXT("sequence of %d"), Sequence->OrderedTags.Num());
		}
		if (const UEGQuestTracker_Timer* Timer = Cast<UEGQuestTracker_Timer>(Tracker))
		{
			return FString::Printf(TEXT("%gs timer"), Timer->DurationSeconds);
		}
		if (const UEGQuestTracker_Composite* Composite = Cast<UEGQuestTracker_Composite>(Tracker))
		{
			switch (Composite->Mode)
			{
			case EEGQuestCompositeMode::All: return FString::Printf(TEXT("all of %d"), Composite->Children.Num());
			case EEGQuestCompositeMode::Any: return FString::Printf(TEXT("any of %d"), Composite->Children.Num());
			default: return FString::Printf(TEXT("%d of %d"), Composite->RequiredChildren, Composite->Children.Num());
			}
		}
		if (const UEGQuestTracker_Fact* Fact = Cast<UEGQuestTracker_Fact>(Tracker))
		{
			return FString::Printf(TEXT("fact: %s"), *Fact->FactTag.ToString());
		}
		if (Tracker->IsA<UEGQuestTracker_Predicate>())
		{
			return TEXT("predicate");
		}
		// Scripted and game-defined subclasses: the class name is the best summary we have.
		return Tracker->GetClass()->GetName().Replace(TEXT("EGQuestTracker_"), TEXT(""));
	}
}

FString UEGQuestNode_Objective::GetEditorDisplayString_Implementation(UEGQuestGraph* OwnerQuest)
{
	FString Summary;
	if (!Tracker)
	{
		Summary = TEXT("Completed by authority");
	}
	else if (Tracker->IsA<UEGQuestTracker_Immediate>())
	{
		Summary = TEXT("Completes immediately");
	}
	else
	{
		Summary = EGQuestDescribeTracker(Tracker);
	}

	if (FailureTracker)
	{
		Summary += FString::Printf(TEXT("  |  fail: %s"), *EGQuestDescribeTracker(FailureTracker));
	}
	return Summary;
}

bool UEGQuestNode_Objective::ValidateForCompile(FString& OutError) const
{
	// An immediate objective resolves the moment its stage is entered, before any fail path could
	// run, so a failure tracker on top of it can never do anything.
	if (Tracker && Tracker->IsA<UEGQuestTracker_Immediate>() &&
		Tracker->OutcomeOnSatisfied == EEGQuestObjectiveOutcome::Succeeded && CanEverFail())
	{
		OutError = TEXT("completes on stage enter, so its fail routing can never run");
		return false;
	}
	return true;
}

void UEGQuestNode_Objective::GetSearchTerms(TArray<FEGQuestSearchTerm>& OutTerms) const
{
	// Keyed the same as before tracker-only authoring so saved Find-in-Quests habits keep working.
	if (const UEGQuestTracker_EventCount* Counting = Cast<UEGQuestTracker_EventCount>(Tracker))
	{
		if (Counting->ExactEventTag.IsValid())
		{
			OutTerms.Emplace(TEXT("EventTag"), Counting->ExactEventTag.ToString());
		}
		OutTerms.Emplace(TEXT("RequiredCount"), FString::FromInt(FMath::Max(1, Counting->RequiredCount)), EEGQuestSearchTermKind::Number);
	}
	else if (const UEGQuestTracker_Distinct* Distinct = Cast<UEGQuestTracker_Distinct>(Tracker))
	{
		if (Distinct->ExactEventTag.IsValid())
		{
			OutTerms.Emplace(TEXT("EventTag"), Distinct->ExactEventTag.ToString());
		}
		OutTerms.Emplace(TEXT("RequiredCount"), FString::FromInt(FMath::Max(1, Distinct->RequiredDistinctCount)), EEGQuestSearchTermKind::Number);
	}
	if (const UEGQuestTracker_EventCount* FailCounting = Cast<UEGQuestTracker_EventCount>(FailureTracker))
	{
		if (FailCounting->ExactEventTag.IsValid())
		{
			OutTerms.Emplace(TEXT("FailEventTag"), FailCounting->ExactEventTag.ToString());
		}
		OutTerms.Emplace(TEXT("FailRequiredCount"), FString::FromInt(FMath::Max(1, FailCounting->RequiredCount)), EEGQuestSearchTermKind::Number);
	}
}

//
// Evaluator runtime
//

UEGQuestComponent* UEGQuestNode_Objective::GetQuestComponent() const
{
	return EvaluatorOwner.Get();
}

void UEGQuestNode_Objective::ActivateEvaluator(UEGQuestComponent& InOwner, const FGuid& InInstanceGuid,
	EEGQuestActivationReason Reason)
{
	EvaluatorOwner = &InOwner;
	EvaluatorInstanceGuid = InInstanceGuid;
	// Belt and braces. Evaluators are duplicated, and StaticDuplicateObject routes ConditionalPostLoad
	// on the game thread, so in practice the synthesis has already happened by here. That is an engine
	// detail with exceptions (bSkipPostLoad, off-thread duplication), and the emptiness of the two
	// hooks below depends on the invariant holding, so assert it where it is used rather than inherit it.
	EnsureTrackers();
	if (Tracker || FailureTracker)
	{
		if (Tracker) Tracker->Activate(*this, Reason);
		if (FailureTracker) FailureTracker->Activate(*this, Reason);
	}
	else
	{
		OnActivated();
	}
}

void UEGQuestNode_Objective::DeactivateEvaluator()
{
	if (!IsEvaluatorActive())
	{
		return;
	}
	EEGQuestDeactivationReason Reason = EEGQuestDeactivationReason::Cancelled;
	FEGQuestSnapshotObjective Line;
	if (EvaluatorOwner.IsValid() && EvaluatorOwner->FindObjectiveAuthorityState(EvaluatorInstanceGuid, GetGUID(), Line))
	{
		if (Line.Outcome == EEGQuestObjectiveOutcome::Succeeded) Reason = EEGQuestDeactivationReason::Completed;
		else if (Line.Outcome == EEGQuestObjectiveOutcome::Failed) Reason = EEGQuestDeactivationReason::Failed;
	}
	if (Tracker) Tracker->Deactivate(Reason);
	if (FailureTracker) FailureTracker->Deactivate(Reason);
	OnDeactivated();
	EvaluatorOwner.Reset();
	EvaluatorInstanceGuid.Invalidate();
}

void UEGQuestNode_Objective::HandleGameplayEvent(const FEGQuestGameplayEvent& Event, int32 ProgressDelta)
{
	if (!IsEvaluatorActive() || IsResolved())
	{
		return;
	}
	if (Tracker || FailureTracker)
	{
		if (Tracker) Tracker->HandleEvent(Event, ProgressDelta);
		if (!IsResolved() && FailureTracker) FailureTracker->HandleEvent(Event, ProgressDelta);
	}
	else
	{
		OnQuestGameplayEvent(Event, ProgressDelta);
	}
}

void UEGQuestNode_Objective::PulseEvaluator()
{
	if (!IsEvaluatorActive() || IsResolved()) return;
	if (Tracker) Tracker->Pulse();
	if (!IsResolved() && FailureTracker) FailureTracker->Pulse();
}

// The two hooks below only run for an objective with no tracker at all, which after EnsureTrackers
// means one that authors nothing to evaluate: the manual archetype, completed by authority code, or
// a Blueprint subclass that overrides them. Both native defaults are therefore empty by construction
// - every authored field they used to read now reaches evaluation as a tracker instead.

void UEGQuestNode_Objective::OnActivated_Implementation()
{
}

void UEGQuestNode_Objective::OnQuestGameplayEvent_Implementation(const FEGQuestGameplayEvent& Event, int32 ProgressDelta)
{
}

void UEGQuestNode_Objective::CompleteObjective(bool bSuccess)
{
	UEGQuestComponent* Owner = EvaluatorOwner.Get();
	if (!Owner || !EvaluatorInstanceGuid.IsValid())
	{
		FEGQuestLogger::Get().Errorf(
			TEXT("CompleteObjective - '%s' is not an active objective evaluator. Only the transient instances "
				 "UEGQuestComponent activates for the running stage may report an outcome."),
			*GetPathName());
		return;
	}
	Owner->CompleteObjectiveFromEvaluator(*this, bSuccess);
}

bool UEGQuestNode_Objective::AddProgress(int32 Delta)
{
	UEGQuestComponent* Owner = EvaluatorOwner.Get();
	return Owner ? Owner->ApplyObjectiveProgress(EvaluatorInstanceGuid, GetGUID(), Delta, false) : false;
}

bool UEGQuestNode_Objective::AddFailProgress(int32 Delta)
{
	UEGQuestComponent* Owner = EvaluatorOwner.Get();
	return Owner ? Owner->ApplyObjectiveProgress(EvaluatorInstanceGuid, GetGUID(), Delta, true) : false;
}

int32 UEGQuestNode_Objective::GetTrackerSequenceIndex() const
{
	const UEGQuestComponent* Owner = EvaluatorOwner.Get();
	FEGQuestSnapshotObjective Line;
	return Owner && Owner->FindObjectiveAuthorityState(EvaluatorInstanceGuid, GetGUID(), Line) ? Line.SequenceIndex : 0;
}

bool UEGQuestNode_Objective::SetTrackerSequenceIndex(int32 NewIndex)
{
	UEGQuestComponent* Owner = EvaluatorOwner.Get();
	return Owner && Owner->SetObjectiveSequenceIndex(EvaluatorInstanceGuid, GetGUID(), FMath::Max(0, NewIndex));
}

int32 UEGQuestNode_Objective::AddTrackerDistinctKey(FName Key)
{
	UEGQuestComponent* Owner = EvaluatorOwner.Get();
	return Owner ? Owner->AddObjectiveDistinctKey(EvaluatorInstanceGuid, GetGUID(), Key) : 0;
}

double UEGQuestNode_Objective::GetTrackerEndServerTime() const
{
	const UEGQuestComponent* Owner = EvaluatorOwner.Get();
	FEGQuestSnapshotObjective Line;
	return Owner && Owner->FindObjectiveAuthorityState(EvaluatorInstanceGuid, GetGUID(), Line) ? Line.TrackerEndServerTime : 0.0;
}

bool UEGQuestNode_Objective::SetTrackerEndServerTime(double EndServerTime)
{
	UEGQuestComponent* Owner = EvaluatorOwner.Get();
	return Owner && Owner->SetObjectiveTrackerEndTime(EvaluatorInstanceGuid, GetGUID(), EndServerTime);
}

int32 UEGQuestNode_Objective::GetProgress() const
{
	const UEGQuestComponent* Owner = EvaluatorOwner.Get();
	FEGQuestSnapshotObjective Line;
	return Owner && Owner->FindObjectiveAuthorityState(EvaluatorInstanceGuid, GetGUID(), Line) ? Line.Count : 0;
}

int32 UEGQuestNode_Objective::GetFailProgress() const
{
	const UEGQuestComponent* Owner = EvaluatorOwner.Get();
	FEGQuestSnapshotObjective Line;
	return Owner && Owner->FindObjectiveAuthorityState(EvaluatorInstanceGuid, GetGUID(), Line) ? Line.FailCount : 0;
}

int32 UEGQuestNode_Objective::GetRequiredProgress() const
{
	const UEGQuestComponent* Owner = EvaluatorOwner.Get();
	FEGQuestSnapshotObjective Line;
	return Owner && Owner->FindObjectiveAuthorityState(EvaluatorInstanceGuid, GetGUID(), Line)
		? Line.RequiredCount
		: GetPresentedRequiredCount();
}

bool UEGQuestNode_Objective::IsResolved() const
{
	const UEGQuestComponent* Owner = EvaluatorOwner.Get();
	FEGQuestSnapshotObjective Line;
	if (!Owner || !Owner->FindObjectiveAuthorityState(EvaluatorInstanceGuid, GetGUID(), Line))
	{
		// No line to resolve: treat as resolved so evaluation logic stops.
		return true;
	}
	return Line.IsResolved();
}
