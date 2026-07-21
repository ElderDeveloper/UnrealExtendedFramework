// Copyright Devil of the Plague. All Rights Reserved.
#include "EGQuestSimulator.h"

#include "EGQuestComponent.h"
#include "EGQuestGraph.h"

FEGQuestSimulator::FEGQuestSimulator(UEGQuestComponent& InComponent, UEGQuestFactsSubsystem& InFacts,
	double InitialServerTime)
	: Component(&InComponent), Facts(&InFacts), Clock(MakeShared<FEGQuestSimulationClock>(InitialServerTime))
{
	InComponent.SetQuestClock(Clock);
}

FEGQuestOperationResult FEGQuestSimulator::Start(UEGQuestGraph& Graph, bool bPrivate)
{
	if (!Component.IsValid()) return FEGQuestOperationResult::Rejected({}, 0, TEXT("MissingComponent"));
	FEGQuestOperationResult Result = bPrivate ? Component->StartPrivateQuest(&Graph) : Component->StartSharedQuest(&Graph);
	if (Result.IsSuccess()) RunId = Result.RunId;
	return Result;
}

FEGQuestOperationResult FEGQuestSimulator::Resume(const FEGQuestSaveEnvelope& SaveData)
{
	if (!Component.IsValid()) return FEGQuestOperationResult::Rejected({}, 0, TEXT("MissingComponent"));
	FEGQuestOperationResult Result = Component->ResumeQuest(SaveData);
	if (Result.IsSuccess()) RunId = Result.RunId;
	return Result;
}

bool FEGQuestSimulator::Set(FGameplayTag Fact, int32 Value, EEGQuestFactScope Scope, APlayerState* Player)
{
	return Facts.IsValid() && Facts->SetFact(Fact, Value, Scope, Player, Component.IsValid() ? Component->GetOwner() : nullptr);
}

FEGQuestOperationResult FEGQuestSimulator::Drive(const FEGQuestGameplayEvent& Event)
{
	return Component.IsValid() ? Component->NotifyGameplayEvent(Event)
		: FEGQuestOperationResult::Rejected(RunId, 0, TEXT("MissingComponent"));
}

FEGQuestOperationResult FEGQuestSimulator::Drive(FGameplayTag EventTag, float Magnitude, FName ContextName)
{
	FEGQuestGameplayEvent Event;
	Event.EventTag = EventTag;
	Event.Magnitude = Magnitude;
	Event.ContextName = ContextName;
	Event.ServerTime = Clock->GetServerTime();
	return Drive(Event);
}

FEGQuestOperationResult FEGQuestSimulator::Advance(double Seconds)
{
	Clock->Advance(Seconds);
	return Component.IsValid() ? Component->PulseActiveTrackers()
		: FEGQuestOperationResult::Rejected(RunId, 0, TEXT("MissingComponent"));
}

bool FEGQuestSimulator::GetSnapshot(FEGQuestViewSnapshot& OutSnapshot) const
{
	return Component.IsValid() && RunId.IsValid() && Component->FindQuestSnapshot(RunId, OutSnapshot);
}

bool FEGQuestSimulator::AssertTrack(FGuid ObjectiveGuid, EEGQuestObjectiveOutcome Expected, FString& OutFailure) const
{
	FEGQuestViewSnapshot Snapshot;
	if (!GetSnapshot(Snapshot)) { OutFailure = TEXT("Run snapshot is unavailable"); return false; }
	const FEGQuestSnapshotObjective* Line = Snapshot.ActiveObjectives.FindByPredicate(
		[ObjectiveGuid](const FEGQuestSnapshotObjective& Item){ return Item.Guid == ObjectiveGuid; });
	if (!Line) { OutFailure = FString::Printf(TEXT("Objective %s is not projected"), *ObjectiveGuid.ToString()); return false; }
	if (Line->Outcome != Expected)
	{
		OutFailure = FString::Printf(TEXT("Objective %s outcome is %d, expected %d"), *ObjectiveGuid.ToString(),
			static_cast<int32>(Line->Outcome), static_cast<int32>(Expected));
		return false;
	}
	OutFailure.Reset();
	return true;
}

bool FEGQuestSimulator::AssertResult(EEGQuestLifecycleState Expected, FString& OutFailure) const
{
	FEGQuestViewSnapshot Snapshot;
	if (!GetSnapshot(Snapshot)) { OutFailure = TEXT("Run snapshot is unavailable"); return false; }
	if (Snapshot.LifecycleState != Expected)
	{
		OutFailure = FString::Printf(TEXT("Lifecycle is %d, expected %d"),
			static_cast<int32>(Snapshot.LifecycleState), static_cast<int32>(Expected));
		return false;
	}
	OutFailure.Reset();
	return true;
}
