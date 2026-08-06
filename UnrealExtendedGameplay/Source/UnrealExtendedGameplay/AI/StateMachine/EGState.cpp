// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGState.h"

#include "AIController.h"
#include "EGStateMachineComponent.h"
#include "EGStateMachineTypes.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"

// -----------------------------------------------------------------------------
// State machine control
// -----------------------------------------------------------------------------

bool UEGState::RequestSwitchState(TSubclassOf<UEGState> StateClass, AActor* InContextActor)
{
	UEGStateMachineComponent* Machine = GetOwningMachine();
	return Machine ? Machine->SwitchState(StateClass, InContextActor) : false;
}

bool UEGState::RequestPushState(TSubclassOf<UEGState> StateClass, AActor* InContextActor)
{
	UEGStateMachineComponent* Machine = GetOwningMachine();
	return Machine ? Machine->PushState(StateClass, InContextActor) : false;
}

void UEGState::RequestPopState()
{
	if (UEGStateMachineComponent* Machine = GetOwningMachine())
	{
		Machine->PopState();
	}
}

// -----------------------------------------------------------------------------
// Accessors
// -----------------------------------------------------------------------------

UEGStateMachineComponent* UEGState::GetOwningMachine() const
{
	if (UEGStateMachineComponent* Registered = OwningMachine.Get())
	{
		return Registered;
	}

	// Not registered yet (a definition being inspected, or a state asked before BeginPlay): the
	// machine is always this state's outer, so fall back to that rather than returning null.
	return GetTypedOuter<UEGStateMachineComponent>();
}

AActor* UEGState::GetOwningActor() const
{
	const UEGStateMachineComponent* Machine = GetOwningMachine();
	return Machine ? Machine->GetOwner() : nullptr;
}

APawn* UEGState::GetOwningPawn() const
{
	return Cast<APawn>(GetOwningActor());
}

AController* UEGState::GetOwningController() const
{
	const APawn* OwnerPawn = GetOwningPawn();
	return OwnerPawn ? OwnerPawn->GetController() : nullptr;
}

AAIController* UEGState::GetOwningAIController() const
{
	return Cast<AAIController>(GetOwningController());
}

AActor* UEGState::GetContextActor() const
{
	const UEGStateMachineComponent* Machine = GetOwningMachine();
	return Machine ? Machine->GetContextActor() : nullptr;
}

void UEGState::SetContextActor(AActor* InContextActor)
{
	if (UEGStateMachineComponent* Machine = GetOwningMachine())
	{
		Machine->SetContextActor(InContextActor);
	}
}

bool UEGState::HasAuthority() const
{
	const UEGStateMachineComponent* Machine = GetOwningMachine();
	return Machine && Machine->HasAuthority();
}

UEGState* UEGState::GetSiblingState(TSubclassOf<UEGState> StateClass) const
{
	const UEGStateMachineComponent* Machine = GetOwningMachine();
	return Machine ? Machine->GetState(StateClass) : nullptr;
}

UWorld* UEGState::GetWorld() const
{
	// The CDO has no owner and must report no world, or the editor treats it as a world context.
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return nullptr;
	}

	const UEGStateMachineComponent* Machine = GetOwningMachine();
	return Machine ? Machine->GetWorld() : nullptr;
}

// -----------------------------------------------------------------------------
// Timers
// -----------------------------------------------------------------------------

FTimerHandle UEGState::SetStateTimer(float Delay, FTimerDelegate Delegate, bool bLoop)
{
	FTimerHandle Handle;

	UWorld* World = GetWorld();
	if (!World)
	{
		return Handle;
	}

	World->GetTimerManager().SetTimer(Handle, Delegate, FMath::Max(Delay, UE_KINDA_SMALL_NUMBER), bLoop);
	ActiveStateTimers.Add(Handle);
	return Handle;
}

void UEGState::ClearStateTimer(FTimerHandle& Handle)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(Handle);
	}

	ActiveStateTimers.RemoveAllSwap([&Handle](const FTimerHandle& Existing) { return Existing == Handle; });
	Handle.Invalidate();
}

void UEGState::ClearAllStateTimers()
{
	if (UWorld* World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		for (FTimerHandle& Handle : ActiveStateTimers)
		{
			TimerManager.ClearTimer(Handle);
		}
	}

	ActiveStateTimers.Reset();
}

// -----------------------------------------------------------------------------
// Debug
// -----------------------------------------------------------------------------

void UEGState::DebugLog(const FString& Message) const
{
	if (UEGStateMachineComponent* Machine = GetOwningMachine())
	{
		Machine->DebugLog(FString::Printf(TEXT("[%s] %s"), *GetClass()->GetName(), *Message));
	}
}

// -----------------------------------------------------------------------------
// Component-side entry points
//
// These own the bookkeeping a subclass must not be able to skip by forgetting Super::.
// -----------------------------------------------------------------------------

void UEGState::NotifyGiven()
{
	OnGiven();
}

void UEGState::NotifyEnter()
{
	bIsActive = true;
	TickAccumulator = 0.0f;
	OnEnter();
}

void UEGState::NotifyTick(float DeltaTime)
{
	if (TickInterval <= 0.0f)
	{
		OnTick(DeltaTime);
		return;
	}

	TickAccumulator += DeltaTime;
	if (TickAccumulator >= TickInterval)
	{
		const float Accumulated = TickAccumulator;
		TickAccumulator = 0.0f;
		OnTick(Accumulated);
	}
}

void UEGState::NotifyPause()
{
	OnPause();
}

void UEGState::NotifyResume()
{
	TickAccumulator = 0.0f;
	OnResume();
}

void UEGState::NotifyExit()
{
	OnExit();

	// Unconditional teardown: a handle from SetStateTimer can never fire against an inactive state,
	// whether or not the subclass cleaned up after itself.
	ClearAllStateTimers();

	bIsActive = false;
	TickAccumulator = 0.0f;
}

void UEGState::NotifyRemoved()
{
	ClearAllStateTimers();
	OnRemoved();
}
