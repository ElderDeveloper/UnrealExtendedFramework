// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "UnrealExtendedGameplay/AI/StateMachine/EGBrainState.h"
#include "UnrealExtendedGameplay/AI/StateMachine/EGState.h"

#include "EGStateMachineTestTypes.generated.h"

/** Shared ordered event log. Lifecycle order is a contract, so the tests assert on the sequence. */
struct FEGStateMachineTestLog
{
	static TArray<FString> Events;

	static void Reset() { Events.Reset(); }

	static void Record(const UObject* Source, const TCHAR* Event)
	{
		Events.Add(FString::Printf(TEXT("%s:%s"), *GetNameSafe(Source ? Source->GetClass() : nullptr), Event));
	}

	/** Flattened log, e.g. "A:Enter|A:Pause|B:Enter" — what the ordering assertions compare against. */
	static FString Join() { return FString::Join(Events, TEXT("|")); }

	static int32 CountOf(const FString& Entry)
	{
		int32 Count = 0;
		for (const FString& Event : Events)
		{
			Count += (Event == Entry) ? 1 : 0;
		}
		return Count;
	}
};

/** Records every lifecycle event and counts them. All test states derive from this. */
UCLASS()
class UEGTestState : public UEGState
{
	GENERATED_BODY()

public:
	virtual void OnGiven_Implementation() override { ++GivenCount; FEGStateMachineTestLog::Record(this, TEXT("Given")); }
	virtual void OnEnter_Implementation() override { ++EnterCount; FEGStateMachineTestLog::Record(this, TEXT("Enter")); }
	virtual void OnExit_Implementation() override { ++ExitCount; FEGStateMachineTestLog::Record(this, TEXT("Exit")); }
	virtual void OnPause_Implementation() override { ++PauseCount; FEGStateMachineTestLog::Record(this, TEXT("Pause")); }
	virtual void OnResume_Implementation() override { ++ResumeCount; FEGStateMachineTestLog::Record(this, TEXT("Resume")); }
	virtual void OnRemoved_Implementation() override { ++RemovedCount; FEGStateMachineTestLog::Record(this, TEXT("Removed")); }
	virtual void OnTick_Implementation(float DeltaTime) override { ++TickCount; AccumulatedTickTime += DeltaTime; }

	int32 GivenCount = 0;
	int32 EnterCount = 0;
	int32 ExitCount = 0;
	int32 PauseCount = 0;
	int32 ResumeCount = 0;
	int32 RemovedCount = 0;
	int32 TickCount = 0;
	float AccumulatedTickTime = 0.0f;
};

UCLASS()
class UEGTestState_A : public UEGTestState
{
	GENERATED_BODY()
};

UCLASS()
class UEGTestState_B : public UEGTestState
{
	GENERATED_BODY()
};

UCLASS()
class UEGTestState_C : public UEGTestState
{
	GENERATED_BODY()
};

/** Always refuses entry. */
UCLASS()
class UEGTestState_Veto : public UEGTestState
{
	GENERATED_BODY()

public:
	virtual bool CanEnterState_Implementation() const override { return bAllowEntry; }

	bool bAllowEntry = false;
};

/** Refuses to be displaced by a force-push while active. */
UCLASS()
class UEGTestState_Blocker : public UEGTestState
{
	GENERATED_BODY()

public:
	UEGTestState_Blocker() { bBlocksInterrupts = true; }
};

/** The interrupt idiom: giving it fires it, and it unregisters itself once it exits. */
UCLASS()
class UEGTestState_Interrupt : public UEGTestState
{
	GENERATED_BODY()

public:
	UEGTestState_Interrupt()
	{
		bPushWhenGiven = true;
		bRemoveWhenExited = true;
	}
};

/** Starts a state-scoped timer on enter. The timer must never fire after the state has exited. */
UCLASS()
class UEGTestState_Timer : public UEGTestState
{
	GENERATED_BODY()

public:
	virtual void OnEnter_Implementation() override
	{
		Super::OnEnter_Implementation();
		SetStateTimer(TimerDelay, FTimerDelegate::CreateUObject(this, &UEGTestState_Timer::HandleTimerFired));
	}

	void HandleTimerFired()
	{
		++TimerFiredCount;
		FEGStateMachineTestLog::Record(this, TEXT("TimerFired"));
	}

	float TimerDelay = 0.1f;
	int32 TimerFiredCount = 0;
};

/**
 * Recording brain. DesiredStateClass is what EvaluateBrain returns, so a test can steer the
 * decision loop directly; leaving it null exercises the "no change" path.
 */
UCLASS()
class UEGTestState_Brain : public UEGBrainState
{
	GENERATED_BODY()

public:
	virtual void OnGiven_Implementation() override { ++GivenCount; FEGStateMachineTestLog::Record(this, TEXT("Given")); }
	virtual void OnRemoved_Implementation() override { ++RemovedCount; FEGStateMachineTestLog::Record(this, TEXT("Removed")); }
	virtual void OnPause_Implementation() override { ++PauseCount; FEGStateMachineTestLog::Record(this, TEXT("Pause")); }
	virtual void OnExit_Implementation() override { ++ExitCount; FEGStateMachineTestLog::Record(this, TEXT("Exit")); }

	virtual void OnEnter_Implementation() override
	{
		++EnterCount;
		FEGStateMachineTestLog::Record(this, TEXT("Enter"));
		Super::OnEnter_Implementation();
	}

	virtual void OnResume_Implementation() override
	{
		++ResumeCount;
		FEGStateMachineTestLog::Record(this, TEXT("Resume"));
		Super::OnResume_Implementation();
	}

	virtual void OnTick_Implementation(float DeltaTime) override
	{
		++TickCount;
		Super::OnTick_Implementation(DeltaTime);
	}

	virtual TSubclassOf<UEGState> EvaluateBrain_Implementation() override
	{
		++EvaluateCount;
		return DesiredStateClass;
	}

	void SetExclusive(bool bExclusive) { bSwitchInsteadOfPush = bExclusive; }
	void SetEvaluateOnResume(bool bEvaluate) { bEvaluateOnResume = bEvaluate; }

	UPROPERTY()
	TSubclassOf<UEGState> DesiredStateClass;

	int32 GivenCount = 0;
	int32 EnterCount = 0;
	int32 ExitCount = 0;
	int32 PauseCount = 0;
	int32 ResumeCount = 0;
	int32 RemovedCount = 0;
	int32 TickCount = 0;
	int32 EvaluateCount = 0;
};

/** Opts into client mirroring: its lifecycle is replayed on simulated proxies. */
UCLASS()
class UEGTestState_Mirrored : public UEGTestState
{
	GENERATED_BODY()

public:
	UEGTestState_Mirrored()
	{
		bRunOnSimulatedProxy = true;
		bTickOnSimulatedProxy = true;
	}
};

/** A second mirrored class, so pause/resume ordering can be observed on the mirror. */
UCLASS()
class UEGTestState_MirroredSecond : public UEGTestState_Mirrored
{
	GENERATED_BODY()
};

/** Second brain class, so the duplicate-brain contract can be tested. */
UCLASS()
class UEGTestState_BrainSecond : public UEGTestState_Brain
{
	GENERATED_BODY()
};
