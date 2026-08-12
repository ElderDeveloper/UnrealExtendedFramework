// Fill out your copyright notice in the Description page of Project Settings.

#include "EGStateMachineComponent.h"

#include "EGState.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogEGStateMachine, Log, All);

UEGStateMachineComponent::UEGStateMachineComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	SetIsReplicatedByDefault(true);
}

// -------------------------------------------------------------------------
// UActorComponent Overrides
// -------------------------------------------------------------------------

void UEGStateMachineComponent::BeginPlay()
{
	Super::BeginPlay();

	// Auto-register states from editor-configured instanced definitions
	for (UEGState* StateInstance : StateDefinitions)
	{
		RegisterInstancedState(StateInstance);
	}

	if (bAutoStart && (RegisteredStateInstances.Num() > 0 || HasDefaultState()))
	{
		Start();
	}
}

void UEGStateMachineComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bIsRunning)
	{
		if (CurrentState)
		{
			CurrentState->OnTick(DeltaTime);
		}
	}

	if (bDebugDraw)
	{
		DrawDebugStateInfo();
	}
}

void UEGStateMachineComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bIsRunning)
	{
		Stop();
	}
	Super::EndPlay(EndPlayReason);
}

void UEGStateMachineComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UEGStateMachineComponent, ReplicatedCurrentStateClass);
	DOREPLIFETIME(UEGStateMachineComponent, ReplicatedStateStack);
	DOREPLIFETIME(UEGStateMachineComponent, ReplicatedStateContextActor);
	DOREPLIFETIME(UEGStateMachineComponent, bReplicatedIsRunning);
}

// -------------------------------------------------------------------------
// Replication
// -------------------------------------------------------------------------

bool UEGStateMachineComponent::HasStateMachineAuthority() const
{
	const AActor* OwnerActor = GetOwner();
	return !OwnerActor || OwnerActor->HasAuthority();
}

void UEGStateMachineComponent::UpdateReplicatedState()
{
	if (!HasStateMachineAuthority())
	{
		return;
	}

	ReplicatedCurrentStateClass = CurrentState ? CurrentState->GetClass() : nullptr;

	ReplicatedStateStack.Reset(StateStack.Num());
	for (const UEGState* State : StateStack)
	{
		ReplicatedStateStack.Add(State ? State->GetClass() : nullptr);
	}

	ReplicatedStateContextActor = StateContextActor.Get();
	bReplicatedIsRunning = bIsRunning;
}

void UEGStateMachineComponent::OnRep_ReplicatedCurrentStateClass()
{
	// CurrentStateClassId is only ever written by this handler on a simulated
	// proxy, so it still holds the previously replicated class.
	const FName OldStateClassId = CurrentStateClassId;
	const FName NewStateClassId = ReplicatedCurrentStateClass ? ReplicatedCurrentStateClass->GetFName() : NAME_None;

	if (OldStateClassId == NewStateClassId)
	{
		return;
	}

	CurrentStateClassId = NewStateClassId;

	if (bClearDebugLogOnStateChange) { DebugLogEntries.Empty(); }

	OnStateChanged.Broadcast(OldStateClassId, NewStateClassId);

	if (bDebugLogStateChanges)
	{
		UE_LOG(LogEGStateMachine, Verbose, TEXT("[SM] %s: (replicated) %s -> %s"),
			GetOwner() ? *GetOwner()->GetName() : TEXT("???"),
			*OldStateClassId.ToString(), *NewStateClassId.ToString());
	}
}

// -------------------------------------------------------------------------
// State Registration
// -------------------------------------------------------------------------

UEGState* UEGStateMachineComponent::AddStateDefinition(UEGState* StateInstance)
{
	if (!StateInstance)
	{
		return nullptr;
	}

	if (StateInstance && !StateDefinitions.Contains(StateInstance))
	{
		StateDefinitions.Add(StateInstance);
	}

	if (StateInstance && GetOwner() && GetOwner()->HasActorBegunPlay())
	{
		RegisterInstancedState(StateInstance);
	}

	return StateInstance;
}

UEGState* UEGStateMachineComponent::RegisterState(TSubclassOf<UEGState> StateClass)
{
	if (!StateClass)
	{
		return nullptr;
	}

	if (UEGState** FoundState = RegisteredStatesByClass.Find(StateClass.Get()))
	{
		return *FoundState;
	}

	UEGState* NewState = NewObject<UEGState>(this, StateClass);
	return RegisterInstancedState(NewState);
}

UEGState* UEGStateMachineComponent::RegisterInstancedState(UEGState* StateInstance)
{
	if (!StateInstance) return nullptr;

	UClass* StateClass = StateInstance->GetClass();
	if (!StateClass)
	{
		return nullptr;
	}

	if (UEGState** FoundState = RegisteredStatesByClass.Find(StateClass))
	{
		UE_LOG(LogEGStateMachine, Warning, TEXT("EGStateMachine: State class '%s' already registered. Skipping duplicate instance."),
			*StateClass->GetName());
		return *FoundState;
	}

	const FName Name = GetStateClassId(StateInstance);
	if (Name.IsNone()) return nullptr;

	StateInstance->OwningComponent = this;
	RegisteredStateInstances.AddUnique(StateInstance);
	RegisteredStatesByClass.Add(StateClass, StateInstance);
	if (!RegisteredStates.Contains(Name))
	{
		RegisteredStates.Add(Name, StateInstance);
	}
	else
	{
		UE_LOG(LogEGStateMachine, Warning, TEXT("EGStateMachine: State registry name '%s' is already used. Class '%s' is still available by class only."),
			*Name.ToString(), *StateClass->GetName());
	}

	if (bIsRunning && StateInstance->bForcePushWhenRegistered && !ForcePushStateByClass(StateClass))
	{
		RegisteredStatesByClass.Remove(StateClass);
		RegisteredStateInstances.Remove(StateInstance);
		if (UEGState** FoundByName = RegisteredStates.Find(Name); FoundByName && *FoundByName == StateInstance)
		{
			RegisteredStates.Remove(Name);
		}
		StateDefinitions.Remove(StateInstance);
		StateInstance->OwningComponent = nullptr;
		return nullptr;
	}

	return StateInstance;
}

UEGState* UEGStateMachineComponent::GetStateByClass(TSubclassOf<UEGState> StateClass) const
{
	if (!StateClass)
	{
		return nullptr;
	}

	UEGState* const* FoundState = RegisteredStatesByClass.Find(StateClass.Get());
	return FoundState ? *FoundState : nullptr;
}

TSubclassOf<UEGState> UEGStateMachineComponent::GetCurrentStateClass() const
{
	if (!HasStateMachineAuthority())
	{
		return ReplicatedCurrentStateClass;
	}

	return CurrentState ? CurrentState->GetClass() : nullptr;
}

void UEGStateMachineComponent::SetDefaultStateByClass(TSubclassOf<UEGState> StateClass)
{
	DefaultStateClass = StateClass.Get();
}

bool UEGStateMachineComponent::HasBrainState() const
{
	return FindBrainState() != nullptr;
}

// -------------------------------------------------------------------------
// Lifecycle Control
// -------------------------------------------------------------------------

void UEGStateMachineComponent::Start()
{
	if (bIsRunning) return;

	// AI brains are host-authoritative: the host is the only machine that runs
	// AI. Clients receive these actors as ordinary replicated pawns
	// (CharacterMovement, montages) and must NOT run a second, divergent AI
	// simulation — doing so desyncs enemy position/attack timing from the
	// server, which is why client-fired projectiles miss and remote players can
	// neither deal nor take damage. Bail on non-authority; tick is only ever
	// enabled from here, so this single gate keeps the SM host-only. Clients
	// observe the result through the replicated mirror instead.
	if (!HasStateMachineAuthority())
	{
		return;
	}

	UEGState* BrainState = FindBrainState();
	if (!BrainState && !DefaultStateClass)
	{
		UE_LOG(LogEGStateMachine, Warning, TEXT("EGStateMachine: No brain or default state registered on '%s'. Startup aborted."),
			GetOwner() ? *GetOwner()->GetName() : TEXT("???"));
		return;
	}

	bIsRunning = true;
	SetComponentTickEnabled(true);
	// An explicitly-set DefaultStateClass is a deliberate runtime override and
	// takes priority over the implicit brain base. When no default is set, fall
	// back to the brain so ambient owners run their schedule.
	const bool bStartedState = DefaultStateClass
		? SwitchStateByClass(DefaultStateClass.Get())
		: (BrainState ? SwitchStateByClass(BrainState->GetClass()) : true);
	if (!bStartedState)
	{
		bIsRunning = false;
		SetComponentTickEnabled(false);
	}

	UpdateReplicatedState();
}

void UEGStateMachineComponent::Stop()
{
	if (!bIsRunning) return;

	if (CurrentState)
	{
		CurrentState->OnExit();
		CurrentState = nullptr;
		CurrentStateClassId = NAME_None;
	}

	bIsRunning = false;
	StateStack.Empty();
	StateContextActor.Reset();
	SetComponentTickEnabled(false);

	UpdateReplicatedState();
}

bool UEGStateMachineComponent::IsRunning() const
{
	return HasStateMachineAuthority() ? bIsRunning : bReplicatedIsRunning;
}

// -------------------------------------------------------------------------
// State Transitions
// -------------------------------------------------------------------------

bool UEGStateMachineComponent::SwitchStateByClass(TSubclassOf<UEGState> StateClass)
{
	UEGState* NextState = GetStateByClass(StateClass);
	if (!NextState)
	{
		UE_LOG(LogEGStateMachine, Warning, TEXT("EGStateMachine: Cannot switch to unregistered state class '%s'."),
			*GetNameSafe(StateClass.Get()));
		return false;
	}

	const FName StateClassId = GetStateClassId(NextState);
	if (!NextState->CanEnterState())
	{
		if (bDebugLogStateChanges)
		{
			UE_LOG(LogEGStateMachine, Verbose, TEXT("[SM] %s: rejected switch to class '%s' from '%s' by CanEnterState (context='%s')."),
				GetOwner() ? *GetOwner()->GetName() : TEXT("???"),
				*GetNameSafe(StateClass.Get()),
				*CurrentStateClassId.ToString(),
				*GetNameSafe(GetStateContextActor()));
		}
		return false;
	}

	UEGState* ExitingState = CurrentState;
	FName OldStateClassId = CurrentStateClassId;

	if (CurrentState)
	{
		CurrentState->OnExit();
	}

	StateStack.Empty();

	CurrentState = NextState;
	CurrentStateClassId = StateClassId;
	CurrentState->OnEnter();

	if (bClearDebugLogOnStateChange) { DebugLogEntries.Empty(); }

	UpdateReplicatedState();

	OnStateChanged.Broadcast(OldStateClassId, CurrentStateClassId);

	if (bDebugLogStateChanges)
	{
		UE_LOG(LogEGStateMachine, Verbose, TEXT("[SM] %s: %s -> %s"),
			GetOwner() ? *GetOwner()->GetName() : TEXT("???"),
			*OldStateClassId.ToString(), *StateClassId.ToString());
	}

	if (ExitingState && ExitingState != CurrentState && ExitingState->bUnregisterWhenExited)
	{
		UnregisterStateByClassId(OldStateClassId);
	}

	return true;
}

bool UEGStateMachineComponent::PushStateByClass(TSubclassOf<UEGState> StateClass)
{
	UEGState* NextState = GetStateByClass(StateClass);
	if (!NextState)
	{
		return false;
	}

	const FName StateClassId = GetStateClassId(NextState);
	if (NextState->bSingleStackEntry && IsStateCurrentOrStacked(NextState))
	{
		return false;
	}

	if (!NextState->CanEnterState())
	{
		if (bDebugLogStateChanges)
		{
			UE_LOG(LogEGStateMachine, Verbose, TEXT("[SM] %s: rejected push to class '%s' from '%s' by CanEnterState (context='%s')."),
				GetOwner() ? *GetOwner()->GetName() : TEXT("???"),
				*GetNameSafe(StateClass.Get()),
				*CurrentStateClassId.ToString(),
				*GetNameSafe(GetStateContextActor()));
		}
		return false;
	}

	if (StateStack.Num() >= MaxStackDepth)
	{
		UE_LOG(LogEGStateMachine, Warning, TEXT("EGStateMachine: Push stack limit (%d) reached."), MaxStackDepth);
		return false;
	}

	FName OldStateClassId = CurrentStateClassId;

	if (CurrentState)
	{
		CurrentState->OnPause();
		if (!CurrentStateClassId.IsNone())
		{
			StateStack.Push(CurrentState);
		}
	}

	CurrentState = NextState;
	CurrentStateClassId = StateClassId;
	CurrentState->OnEnter();

	if (bClearDebugLogOnStateChange) { DebugLogEntries.Empty(); }

	UpdateReplicatedState();

	OnStateChanged.Broadcast(OldStateClassId, CurrentStateClassId);

	if (bDebugLogStateChanges)
	{
		UE_LOG(LogEGStateMachine, Verbose, TEXT("[SM] %s: Push '%s' (depth: %d)"),
			GetOwner() ? *GetOwner()->GetName() : TEXT("???"),
			*StateClassId.ToString(), StateStack.Num());
	}

	return true;
}

bool UEGStateMachineComponent::ForcePushStateByClass(TSubclassOf<UEGState> StateClass)
{
	UEGState* NextState = GetStateByClass(StateClass);
	if (!NextState)
	{
		return false;
	}

	const FName StateClassId = GetStateClassId(NextState);
	if (NextState->bSingleStackEntry && IsStateCurrentOrStacked(NextState))
	{
		return false;
	}

	if (!NextState->CanEnterState())
	{
		if (bDebugLogStateChanges)
		{
			UE_LOG(LogEGStateMachine, Verbose, TEXT("[SM] %s: rejected force-push to class '%s' by CanEnterState (context='%s')."),
				GetOwner() ? *GetOwner()->GetName() : TEXT("???"),
				*GetNameSafe(StateClass.Get()),
				*GetNameSafe(GetStateContextActor()));
		}
		return false;
	}

	if (CurrentState && CurrentState->bBlocksForcedPushesWhileActive)
	{
		if (bDebugLogStateChanges)
		{
			UE_LOG(LogEGStateMachine, Verbose, TEXT("[SM] %s: rejected force-push to class '%s' because '%s' blocks forced pushes."),
				GetOwner() ? *GetOwner()->GetName() : TEXT("???"),
				*GetNameSafe(StateClass.Get()),
				*CurrentStateClassId.ToString());
		}
		return false;
	}

	FName OldStateClassId = CurrentStateClassId;
	if (CurrentState)
	{
		CurrentState->OnPause();
		if (!CurrentStateClassId.IsNone())
		{
			StateStack.Push(CurrentState);
		}
	}

	CurrentState = NextState;
	CurrentStateClassId = StateClassId;
	CurrentState->OnEnter();

	if (bClearDebugLogOnStateChange) { DebugLogEntries.Empty(); }

	UpdateReplicatedState();

	OnStateChanged.Broadcast(OldStateClassId, CurrentStateClassId);

	if (bDebugLogStateChanges)
	{
		UE_LOG(LogEGStateMachine, Verbose, TEXT("[SM] %s: ForcePush '%s' (depth: %d)"),
			GetOwner() ? *GetOwner()->GetName() : TEXT("???"),
			*StateClassId.ToString(), StateStack.Num());
	}

	return true;
}

bool UEGStateMachineComponent::IsCurrentStateClass(TSubclassOf<UEGState> StateClass) const
{
	if (!HasStateMachineAuthority())
	{
		return StateClass && ReplicatedCurrentStateClass.Get() == StateClass.Get();
	}

	return StateClass && CurrentState && CurrentState->GetClass() == StateClass.Get();
}

void UEGStateMachineComponent::PopState()
{
	if (!CurrentState && StateStack.Num() == 0)
	{
		return;
	}

	UEGState* ExitingState = CurrentState;
	FName OldStateClassId = CurrentStateClassId;

	if (CurrentState)
	{
		CurrentState->OnExit();
	}

	if (StateStack.Num() > 0)
	{
		UEGState* PreviousState = StateStack.Pop();
		if (PreviousState)
		{
			CurrentState = PreviousState;
			CurrentStateClassId = FindStateClassId(CurrentState);
			CurrentState->OnResume();

			if (bClearDebugLogOnStateChange) { DebugLogEntries.Empty(); }
		}
		else
		{
			CurrentState = nullptr;
			CurrentStateClassId = NAME_None;
		}
	}
	else
	{
		CurrentState = nullptr;
		CurrentStateClassId = NAME_None;
	}

	UpdateReplicatedState();

	OnStateChanged.Broadcast(OldStateClassId, CurrentStateClassId);

	if (bDebugLogStateChanges)
	{
		UE_LOG(LogEGStateMachine, Verbose, TEXT("[SM] %s: Pop -> '%s' (depth: %d)"),
			GetOwner() ? *GetOwner()->GetName() : TEXT("???"),
			*CurrentStateClassId.ToString(), StateStack.Num());
	}

	if (ExitingState && ExitingState != CurrentState && ExitingState->bUnregisterWhenExited)
	{
		UnregisterStateByClassId(OldStateClassId);
	}
}

bool UEGStateMachineComponent::UnregisterStateByClassId(FName StateClassId)
{
	if (StateClassId.IsNone() || CurrentStateClassId == StateClassId)
	{
		return false;
	}

	UEGState** FoundState = RegisteredStates.Find(StateClassId);
	if (!FoundState || !(*FoundState))
	{
		return false;
	}

	UEGState* StateInstance = *FoundState;
	if (StateStack.Contains(StateInstance))
	{
		return false;
	}

	RegisteredStates.Remove(StateClassId);
	if (StateInstance)
	{
		RegisteredStatesByClass.Remove(StateInstance->GetClass());
		RegisteredStateInstances.Remove(StateInstance);
	}
	StateDefinitions.Remove(StateInstance);
	if (DefaultStateClass == (StateInstance ? StateInstance->GetClass() : nullptr))
	{
		DefaultStateClass = nullptr;
	}
	if (StateInstance)
	{
		StateInstance->OwningComponent = nullptr;
	}

	return true;
}

void UEGStateMachineComponent::SetStateContextActor(AActor* Actor)
{
	StateContextActor = Actor;
	UpdateReplicatedState();
}

AActor* UEGStateMachineComponent::GetStateContextActor() const
{
	if (!HasStateMachineAuthority())
	{
		return ReplicatedStateContextActor;
	}

	return StateContextActor.Get();
}

void UEGStateMachineComponent::ClearStateContextActor()
{
	StateContextActor.Reset();
	UpdateReplicatedState();
}

// -------------------------------------------------------------------------
// Queries
// -------------------------------------------------------------------------

int32 UEGStateMachineComponent::GetStackDepth() const
{
	return HasStateMachineAuthority() ? StateStack.Num() : ReplicatedStateStack.Num();
}

TMap<FName, UEGState*> UEGStateMachineComponent::GetRegisteredStates() const
{
	return RegisteredStates;
}

TArray<FName> UEGStateMachineComponent::GetStateStack() const
{
	TArray<FName> StackNames;

	if (!HasStateMachineAuthority())
	{
		StackNames.Reserve(ReplicatedStateStack.Num());
		for (const TSubclassOf<UEGState>& StackedStateClass : ReplicatedStateStack)
		{
			StackNames.Add(StackedStateClass ? StackedStateClass->GetFName() : NAME_None);
		}
		return StackNames;
	}

	StackNames.Reserve(StateStack.Num());
	for (const UEGState* State : StateStack)
	{
		StackNames.Add(FindStateClassId(State));
	}
	return StackNames;
}

FName UEGStateMachineComponent::GetStateClassId(const UEGState* State) const
{
	if (!State)
	{
		return NAME_None;
	}

	return State->GetClass() ? State->GetClass()->GetFName() : NAME_None;
}

bool UEGStateMachineComponent::IsStateCurrentOrStacked(const UEGState* State) const
{
	return State && (CurrentState == State || StateStack.Contains(State));
}

FName UEGStateMachineComponent::FindStateClassId(const UEGState* State) const
{
	for (const auto& Pair : RegisteredStates)
	{
		if (Pair.Value == State)
		{
			return Pair.Key;
		}
	}
	return GetStateClassId(State);
}

UEGState* UEGStateMachineComponent::FindBrainState() const
{
	UEGState* BrainState = nullptr;
	for (UEGState* State : RegisteredStateInstances)
	{
		if (!State || !State->IsBrainState())
		{
			continue;
		}

		if (BrainState)
		{
			UE_LOG(LogEGStateMachine, Error, TEXT("EGStateMachine: Multiple brain states registered on '%s' ('%s' and '%s'). Startup aborted."),
				GetOwner() ? *GetOwner()->GetName() : TEXT("???"),
				*GetNameSafe(BrainState->GetClass()),
				*GetNameSafe(State->GetClass()));
			return nullptr;
		}

		BrainState = State;
	}

	return BrainState;
}

// -------------------------------------------------------------------------
// Debug Log
// -------------------------------------------------------------------------

void UEGStateMachineComponent::DebugLog(const FString& Message)
{
	if (!bDebugDraw) { return; }

	DebugLogEntries.Add(Message);

	// Ring-buffer eviction
	while (DebugLogEntries.Num() > DebugLogMaxEntries)
	{
		DebugLogEntries.RemoveAt(0, 1, EAllowShrinking::No);
	}

	if (bDebugLogStateChanges)
	{
		UE_LOG(LogEGStateMachine, Log, TEXT("[SM-Debug] %s: %s"),
			GetOwner() ? *GetOwner()->GetName() : TEXT("???"), *Message);
	}
}

void UEGStateMachineComponent::DrawDebugStateInfo() const
{
#if ENABLE_DRAW_DEBUG
	const AActor* Owner = GetOwner();
	if (!Owner || CurrentStateClassId.IsNone()) return;

	const FVector DrawLocation = Owner->GetActorLocation() + FVector(0.f, 25.f, DebugTextOffset);

	FString DebugText = CurrentStateClassId.IsNone() ? TEXT("None") : CurrentStateClassId.ToString();
	if (StateStack.Num() > 0)
	{
		DebugText += FString::Printf(TEXT(" [Stack: %d]"), StateStack.Num());
		for (int32 i = StateStack.Num() - 1; i >= 0; --i)
		{
			DebugText += FString::Printf(TEXT("\n  (%s)"), *FindStateClassId(StateStack[i]).ToString());
		}
	}

	DrawDebugString(GetWorld(), DrawLocation, DebugText, nullptr, DebugActiveColor, 0.f, true, 1.2f);

	// Render debug log entries below the state name
	if (DebugLogEntries.Num() > 0)
	{
		FString LogText;
		for (int32 i = 0; i < DebugLogEntries.Num(); ++i)
		{
			if (i > 0) { LogText += TEXT("\n"); }
			LogText += DebugLogEntries[i];
		}

		const FVector LogLocation = DrawLocation - FVector(0.f, 0.f, 18.f);
		DrawDebugString(GetWorld(), LogLocation, LogText, nullptr, FColor::Cyan, 0.f, true, 0.85f);
	}
#endif
}
