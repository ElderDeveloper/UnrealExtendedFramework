// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGBrainState.h"

#include "EGStateMachineComponent.h"

UEGBrainState::UEGBrainState()
{
	// The brain is the stack floor, so it must never remove itself or be displaced silently.
	bRemoveWhenExited = false;
	bPushWhenGiven = false;

	// bTickWhilePaused stays off here: the default loop is event-driven, deciding on enter and on
	// resume. A brain that must preempt its own child — swap a chase for a stun reaction while the
	// chase is still running — turns it on, as UDOPBrainState does.
}

void UEGBrainState::OnEnter_Implementation()
{
	// Reuse the base tick accumulator rather than running a second timer: the brain only needs to
	// be woken once per decision interval.
	TickInterval = DecisionInterval;

	if (bEvaluateOnEnter)
	{
		EvaluateNow();
	}
}

void UEGBrainState::OnResume_Implementation()
{
	// A child finished. This is the event-driven half of the loop — the interval tick below only
	// exists to retry after a refused transition.
	if (bEvaluateOnResume)
	{
		EvaluateNow();
	}
}

void UEGBrainState::OnTick_Implementation(float DeltaTime)
{
	EvaluateNow();
}

bool UEGBrainState::EvaluateNow()
{
	UEGStateMachineComponent* Machine = GetOwningMachine();
	if (!Machine || !HasAuthority())
	{
		return false;
	}

	// PopToBrain below resumes this state, which would re-enter here through bEvaluateOnResume.
	if (bEvaluating)
	{
		return false;
	}
	TGuardValue<bool> EvaluationGuard(bEvaluating, true);

	const TSubclassOf<UEGState> RequestedClass = EvaluateBrain();
	LastRequestedStateClass = RequestedClass;

	// Null means "nothing to change" — the common case, and not worth logging.
	if (!RequestedClass)
	{
		LastTransitionResult = EEGStateTransitionResult::Succeeded;
		return false;
	}

	// Asking for what is already running is normal for a polling brain, not a refusal. Compared
	// against the brain's own child rather than the current state, so a decision that has not
	// changed leaves a grandchild — an attack mid-swing — alone.
	if (RequestedClass == GetActiveChildClass() || Machine->IsCurrentState(RequestedClass))
	{
		LastTransitionResult = EEGStateTransitionResult::Succeeded;
		return false;
	}

	// Exclusive children: drop whatever is running before pushing the new one. Note this is
	// PopToBrain and not SwitchState — a switch would unwind the brain itself off the stack and
	// leave the machine with no floor to return to.
	if (bSwitchInsteadOfPush)
	{
		Machine->PopToBrain();
	}

	LastTransitionResult = Machine->TryPushState(RequestedClass, ResolveContextActor());

	const bool bTransitioned = LastTransitionResult == EEGStateTransitionResult::Succeeded;

	if (!bTransitioned)
	{
		DebugLog(FString::Printf(TEXT("%s refused (%s)"),
			*GetNameSafe(RequestedClass.Get()), EGLexToString(LastTransitionResult)));
	}

	return bTransitioned;
}

TSubclassOf<UEGState> UEGBrainState::GetActiveChildClass() const
{
	const UEGStateMachineComponent* Machine = GetOwningMachine();
	if (!Machine || Machine->GetCurrentState() == this)
	{
		return nullptr;
	}

	// Stack runs bottom to top and excludes the current state, so the brain's child is either the
	// entry directly above the brain, or — when the brain is the top of the stack — the current state.
	const TArray<TSubclassOf<UEGState>> Stack = Machine->GetStateStack();
	const int32 BrainIndex = Stack.IndexOfByKey(TSubclassOf<UEGState>(GetClass()));
	if (BrainIndex == INDEX_NONE)
	{
		return nullptr;
	}

	return Stack.IsValidIndex(BrainIndex + 1) ? Stack[BrainIndex + 1] : Machine->GetCurrentStateClass();
}

FString UEGBrainState::GetStateDebugString_Implementation() const
{
	if (!LastRequestedStateClass)
	{
		return TEXT("idle");
	}

	return FString::Printf(TEXT("want %s (%s)"),
		*GetNameSafe(LastRequestedStateClass.Get()), EGLexToString(LastTransitionResult));
}
