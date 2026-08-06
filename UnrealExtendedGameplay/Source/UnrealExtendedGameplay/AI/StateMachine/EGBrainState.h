// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EGStateMachineTypes.h"

#include "EGBrainState.generated.h"

/**
 * UEGBrainState
 *
 * The single root decision state. A machine requires exactly one, and it sits at the bottom of the
 * stack: PopState refuses to pop it, PopToBrain unwinds back to it, and Start() enters it.
 *
 * Subclasses override EvaluateBrain() and nothing else. The base owns the decision loop — the
 * interval timer, the already-active check, the CanEnterState re-check, push-versus-switch, and
 * the debug logging — because that boilerplate is otherwise rewritten identically in every game
 * (four copies in DevilOfPlague, thirteen in TalesOfTrade, all the same shape).
 *
 * The loop is event-driven: when a child state pops, the brain resumes and re-evaluates
 * immediately. The interval tick is the fallback for when a requested transition was refused.
 */
UCLASS(Abstract, Blueprintable, BlueprintType)
class UNREALEXTENDEDGAMEPLAY_API UEGBrainState : public UEGState
{
	GENERATED_BODY()

public:
	UEGBrainState();

	virtual bool IsBrainState() const override { return true; }

	/**
	 * The only thing a game brain implements: which state should be running now.
	 * Return null for "no change".
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Extended|State Machine|Brain")
	TSubclassOf<UEGState> EvaluateBrain();
	virtual TSubclassOf<UEGState> EvaluateBrain_Implementation() { return nullptr; }

	/** Runs a decision now, outside the interval. Returns true if a transition actually happened. */
	UFUNCTION(BlueprintCallable, Category = "Extended|State Machine|Brain")
	bool EvaluateNow();

	/**
	 * The state this brain put on the stack, ignoring anything that state pushed on top of itself.
	 *
	 * A brain decides between its own children — chase, flee, stunned — while the child decides
	 * everything finer grained. Comparing a decision against the *current* state would see the
	 * grandchild (an attack mid-swing) and tear it down to re-enter a chase that is already
	 * running; comparing against this does not.
	 */
	UFUNCTION(BlueprintPure, Category = "Extended|State Machine|Brain")
	TSubclassOf<UEGState> GetActiveChildClass() const;

	virtual void OnEnter_Implementation() override;
	virtual void OnResume_Implementation() override;
	virtual void OnTick_Implementation(float DeltaTime) override;
	virtual FString GetStateDebugString_Implementation() const override;

protected:
	/** Seconds between automatic evaluations while the brain is the active state. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Extended|State Machine|Brain", meta = (ClampMin = "0.0"))
	float DecisionInterval = 0.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Extended|State Machine|Brain")
	bool bEvaluateOnEnter = true;

	/** Re-evaluate the moment a child state pops. This is what makes the loop event-driven. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Extended|State Machine|Brain")
	bool bEvaluateOnResume = true;

	/**
	 * Exclusive children: clear the stack and switch instead of stacking on top of the brain.
	 * Matches DevilOfPlague's boss brain, which clears before every push.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Extended|State Machine|Brain")
	bool bSwitchInsteadOfPush = false;

	/** Actor handed to the child state as its transition context. Null by default. */
	UFUNCTION(BlueprintNativeEvent, Category = "Extended|State Machine|Brain")
	AActor* ResolveContextActor() const;
	virtual AActor* ResolveContextActor_Implementation() const { return nullptr; }

private:
	/** Last class EvaluateBrain asked for, for the debug overlay. */
	UPROPERTY(Transient)
	TSubclassOf<UEGState> LastRequestedStateClass;

	EEGStateTransitionResult LastTransitionResult = EEGStateTransitionResult::Succeeded;

	/** Re-entrancy guard: PopToBrain resumes this state mid-evaluation. */
	bool bEvaluating = false;
};
