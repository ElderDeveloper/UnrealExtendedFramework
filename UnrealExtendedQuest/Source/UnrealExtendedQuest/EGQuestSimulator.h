// Copyright Devil of the Plague. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "EGQuestClock.h"
#include "EGQuestFactsSubsystem.h"
#include "EGQuestTypes.h"

class APlayerState;
class UEGQuestComponent;
class UEGQuestGraph;

/** Deterministic fake time source used by FEGQuestSimulator and runtime tests. */
class UNREALEXTENDEDQUEST_API FEGQuestSimulationClock final : public IEGQuestClock
{
public:
	explicit FEGQuestSimulationClock(double InTime = 0.0) : Time(InTime) {}
	double GetServerTime() const override { return Time; }
	void Set(double InTime) { Time = FMath::Max(Time, InTime); }
	void Advance(double Seconds) { Time += FMath::Max(0.0, Seconds); }
private:
	double Time = 0.0;
};

/**
 * Headless driver around the production component, facts subsystem, and executor. It owns no
 * alternate quest logic: tests supply normal runtime services and this class only injects time and inputs.
 */
class UNREALEXTENDEDQUEST_API FEGQuestSimulator
{
public:
	FEGQuestSimulator(UEGQuestComponent& InComponent, UEGQuestFactsSubsystem& InFacts,
		double InitialServerTime = 0.0);

	FEGQuestOperationResult Start(UEGQuestGraph& Graph, bool bPrivate = false);
	FEGQuestOperationResult Resume(const FEGQuestSaveEnvelope& SaveData);
	bool Set(FGameplayTag Fact, int32 Value, EEGQuestFactScope Scope = EEGQuestFactScope::World,
		APlayerState* Player = nullptr);
	FEGQuestOperationResult Drive(const FEGQuestGameplayEvent& Event);
	FEGQuestOperationResult Drive(FGameplayTag EventTag, float Magnitude = 1.0f,
		FName ContextName = NAME_None);
	FEGQuestOperationResult Advance(double Seconds);

	FGuid GetRunId() const { return RunId; }
	double GetServerTime() const { return Clock->GetServerTime(); }
	bool GetSnapshot(FEGQuestViewSnapshot& OutSnapshot) const;
	bool AssertTrack(FGuid ObjectiveGuid, EEGQuestObjectiveOutcome Expected, FString& OutFailure) const;
	bool AssertResult(EEGQuestLifecycleState Expected, FString& OutFailure) const;

private:
	TWeakObjectPtr<UEGQuestComponent> Component;
	TWeakObjectPtr<UEGQuestFactsSubsystem> Facts;
	TSharedPtr<FEGQuestSimulationClock> Clock;
	FGuid RunId;
};
