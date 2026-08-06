// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGStateMachineComponent.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY(LogEGStateMachine);

const TCHAR* EGLexToString(EEGStateTransitionResult Result)
{
	switch (Result)
	{
	case EEGStateTransitionResult::Succeeded:            return TEXT("Succeeded");
	case EEGStateTransitionResult::NotRegistered:        return TEXT("NotRegistered");
	case EEGStateTransitionResult::AlreadyPresent:       return TEXT("AlreadyPresent");
	case EEGStateTransitionResult::VetoedByState:        return TEXT("VetoedByState");
	case EEGStateTransitionResult::StackFull:            return TEXT("StackFull");
	case EEGStateTransitionResult::BlockedByActiveState: return TEXT("BlockedByActiveState");
	case EEGStateTransitionResult::NoAuthority:          return TEXT("NoAuthority");
	default:                                             return TEXT("Invalid");
	}
}

UEGStateMachineComponent::UEGStateMachineComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	SetIsReplicatedByDefault(true);
	bReplicateUsingRegisteredSubObjectList = true;
}

void UEGStateMachineComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UEGStateMachineComponent, NetState);
}

// -----------------------------------------------------------------------------
// UActorComponent
// -----------------------------------------------------------------------------

void UEGStateMachineComponent::ReadyForReplication()
{
	Super::ReadyForReplication();

	// Registering here rather than in BeginPlay means any state that opts into replication is
	// already a registered subobject before the component's first replication pass.
	EnsureStatesRegistered();
}

void UEGStateMachineComponent::BeginPlay()
{
	Super::BeginPlay();

	EnsureStatesRegistered();

	if (bAutoStart && RegisteredStates.Num() > 0)
	{
		Start();
	}
}

void UEGStateMachineComponent::EnsureStatesRegistered()
{
	if (bStatesRegistered)
	{
		return;
	}
	bStatesRegistered = true;

	// Registration runs on every machine, authority or not: clients need the same instances so a
	// replicated class reference can be resolved locally, with the same authored property values.
	for (UEGState* Definition : StateDefinitions)
	{
		RegisterStateInstance(Definition);
	}
}

void UEGStateMachineComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Stop();
	Super::EndPlay(EndPlayReason);
}

void UEGStateMachineComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bDebugDraw)
	{
		DrawDebugStateInfo();
	}

	if (!HasAuthority())
	{
		// Mirrors only tick states that asked to run client-side and to keep ticking there.
		if (CurrentState && CurrentState->TicksOnSimulatedProxy())
		{
			CurrentState->NotifyTick(DeltaTime);
		}
		return;
	}

	if (!bIsRunning)
	{
		return;
	}

	if (CurrentState)
	{
		CurrentState->NotifyTick(DeltaTime);
	}

	// Paused states that opted in still run — the brain uses this to preempt its own children.
	// Iterated bottom-up on a copy, because a decision taken here can rearrange the stack.
	if (StateStack.Num() > 0)
	{
		TArray<TObjectPtr<UEGState>> PausedSnapshot = StateStack;
		for (UEGState* Paused : PausedSnapshot)
		{
			if (Paused && Paused->TicksWhilePaused() && StateStack.Contains(Paused))
			{
				Paused->NotifyTick(DeltaTime);
			}
		}
	}
}

void UEGStateMachineComponent::UpdateTickEnabled()
{
	const bool bMirrorTicks = !HasAuthority() && CurrentState && CurrentState->TicksOnSimulatedProxy();
	const bool bSimulating = HasAuthority() && bIsRunning;

	SetComponentTickEnabled(bSimulating || bMirrorTicks || bDebugDraw);
}

// -----------------------------------------------------------------------------
// Registration
// -----------------------------------------------------------------------------

UEGState* UEGStateMachineComponent::AddStateDefinition(UEGState* StateInstance)
{
	if (!StateInstance)
	{
		return nullptr;
	}

	// Constructor-time calls are registered later by BeginPlay; anything added afterwards
	// registers immediately.
	if (bStatesRegistered)
	{
		UEGState* Registered = RegisterStateInstance(StateInstance);

		// Only keep the definition if it actually became the registered instance — a rejected
		// duplicate must not linger in the authored array.
		if (Registered == StateInstance)
		{
			StateDefinitions.AddUnique(StateInstance);
		}

		return Registered;
	}

	StateDefinitions.AddUnique(StateInstance);
	return StateInstance;
}

UEGState* UEGStateMachineComponent::RegisterStateInstance(UEGState* StateInstance)
{
	if (!StateInstance)
	{
		return nullptr;
	}

	UClass* StateClass = StateInstance->GetClass();
	if (!StateClass)
	{
		return nullptr;
	}

	// One instance per class. A duplicate never replaces the incumbent — that would silently
	// discard the registered instance's runtime data and its editor-tuned values.
	if (UEGState** Existing = StatesByClass.Find(StateClass))
	{
		if (*Existing != StateInstance)
		{
			UE_LOG(LogEGStateMachine, Warning, TEXT("[%s] State class '%s' is already registered; keeping the existing instance."),
				*GetNameSafe(GetOwner()), *StateClass->GetName());
		}
		return *Existing;
	}

	StateInstance->OwningMachine = this;
	RegisteredStates.Add(StateInstance);
	StatesByClass.Add(StateClass, StateInstance);

	if (StateInstance->ShouldReplicateState() && HasAuthority())
	{
		AddReplicatedSubObject(StateInstance);
	}

	StateInstance->NotifyGiven();

	return StateInstance;
}

void UEGStateMachineComponent::UnregisterStateInstance(UEGState* StateInstance)
{
	if (!StateInstance)
	{
		return;
	}

	if (StateInstance->ShouldReplicateState() && HasAuthority() && IsReplicatedSubObjectRegistered(StateInstance))
	{
		RemoveReplicatedSubObject(StateInstance);
	}

	StatesByClass.Remove(StateInstance->GetClass());
	RegisteredStates.Remove(StateInstance);
	StateDefinitions.Remove(StateInstance);

	StateInstance->NotifyRemoved();
	StateInstance->OwningMachine = nullptr;
}

UEGState* UEGStateMachineComponent::GiveState(TSubclassOf<UEGState> StateClass)
{
	if (!StateClass)
	{
		return nullptr;
	}

	if (UEGState* Existing = GetState(StateClass))
	{
		return Existing;
	}

	if (StateClass->HasAnyClassFlags(CLASS_Abstract))
	{
		UE_LOG(LogEGStateMachine, Warning, TEXT("[%s] Cannot give abstract state class '%s'."),
			*GetNameSafe(GetOwner()), *StateClass->GetName());
		return nullptr;
	}

	UEGState* NewState = NewObject<UEGState>(this, StateClass);
	UEGState* Registered = RegisterStateInstance(NewState);
	if (!Registered)
	{
		return nullptr;
	}

	// The interrupt idiom: give a state to fire it. If the push is refused, the registration is
	// rolled back — leaving it registered-but-never-entered would make every later give a no-op,
	// because the second give would just return this dormant instance.
	if (Registered->ShouldPushWhenGiven())
	{
		if (TryPushState(StateClass, nullptr, /*bForce*/ true) != EEGStateTransitionResult::Succeeded)
		{
			UnregisterStateInstance(Registered);
			return nullptr;
		}
	}

	return Registered;
}

bool UEGStateMachineComponent::RemoveState(TSubclassOf<UEGState> StateClass)
{
	UEGState* State = GetState(StateClass);
	if (!State)
	{
		return false;
	}

	if (State->IsBrainState())
	{
		UE_LOG(LogEGStateMachine, Warning, TEXT("[%s] Refused to remove brain state '%s'."),
			*GetNameSafe(GetOwner()), *GetNameSafe(StateClass.Get()));
		return false;
	}

	if (State == CurrentState)
	{
		PopState();
	}
	else if (StateStack.Contains(State))
	{
		UE_LOG(LogEGStateMachine, Warning, TEXT("[%s] Refused to remove '%s': it is paused in the stack."),
			*GetNameSafe(GetOwner()), *GetNameSafe(StateClass.Get()));
		return false;
	}

	// PopState may already have unregistered it via bRemoveWhenExited.
	if (StatesByClass.Contains(State->GetClass()))
	{
		UnregisterStateInstance(State);
	}

	return true;
}

bool UEGStateMachineComponent::HasState(TSubclassOf<UEGState> StateClass) const
{
	return GetState(StateClass) != nullptr;
}

UEGState* UEGStateMachineComponent::GetState(TSubclassOf<UEGState> StateClass) const
{
	if (!StateClass)
	{
		return nullptr;
	}

	UEGState* const* Found = StatesByClass.Find(StateClass.Get());
	return Found ? *Found : nullptr;
}

TArray<UEGState*> UEGStateMachineComponent::GetRegisteredStates() const
{
	TArray<UEGState*> Result;
	Result.Reserve(RegisteredStates.Num());
	for (const TObjectPtr<UEGState>& State : RegisteredStates)
	{
		Result.Add(State);
	}
	return Result;
}

// -----------------------------------------------------------------------------
// Runtime control
// -----------------------------------------------------------------------------

void UEGStateMachineComponent::Start()
{
	if (bIsRunning)
	{
		return;
	}

	// AI simulation is host-authoritative. Tick is only ever enabled from here, so this single
	// gate keeps the whole machine server-side.
	if (!HasAuthority())
	{
		return;
	}

	// Exactly one brain is required. FindBrainState already errors on duplicates; zero is the
	// other half of the same contract. Refusing loudly beats a machine that silently does nothing.
	UEGState* Brain = FindBrainState();
	if (!Brain)
	{
		UE_LOG(LogEGStateMachine, Error, TEXT("[%s] No brain state registered. A machine needs exactly one UEGBrainState. Startup aborted."),
			*GetNameSafe(GetOwner()));
		return;
	}

	bIsRunning = true;
	UpdateTickEnabled();

	const EEGStateTransitionResult Result = TrySwitchState(Brain->GetClass());
	if (Result != EEGStateTransitionResult::Succeeded)
	{
		UE_LOG(LogEGStateMachine, Error, TEXT("[%s] Failed to enter brain state '%s' (%s). Machine stopped."),
			*GetNameSafe(GetOwner()), *GetNameSafe(Brain->GetClass()), EGLexToString(Result));

		bIsRunning = false;
		UpdateTickEnabled();
		return;
	}

	// An explicitly set DefaultStateClass is a deliberate override — a spawned actor forced into a
	// specific state. It is pushed *on top of* the brain rather than replacing it, so the brain
	// stays the floor and the machine still has somewhere to return to.
	if (DefaultStateClass && DefaultStateClass != Brain->GetClass())
	{
		const EEGStateTransitionResult DefaultResult = TryPushState(DefaultStateClass);
		if (DefaultResult != EEGStateTransitionResult::Succeeded)
		{
			UE_LOG(LogEGStateMachine, Warning, TEXT("[%s] Default state '%s' was refused (%s). Running on the brain alone."),
				*GetNameSafe(GetOwner()), *GetNameSafe(DefaultStateClass.Get()), EGLexToString(DefaultResult));
		}
	}
}

void UEGStateMachineComponent::Stop()
{
	if (!bIsRunning)
	{
		return;
	}

	const TSubclassOf<UEGState> OldStateClass = GetCurrentStateClass();

	UEGState* Exiting = CurrentState;
	CurrentState = nullptr;
	ExitState(Exiting);
	UnwindStack();

	bIsRunning = false;
	ContextActor = nullptr;
	UpdateTickEnabled();

	HandleStateChanged(OldStateClass);
}

void UEGStateMachineComponent::SetDefaultStateByClass(TSubclassOf<UEGState> StateClass)
{
	DefaultStateClass = StateClass;
}

// -----------------------------------------------------------------------------
// Transitions
// -----------------------------------------------------------------------------

EEGStateTransitionResult UEGStateMachineComponent::EvaluateTransition(TSubclassOf<UEGState> StateClass, bool bIsPush, bool bIsForce, UEGState*& OutState) const
{
	OutState = nullptr;

	if (!StateClass || !GetOwner())
	{
		return EEGStateTransitionResult::Invalid;
	}

	if (!HasAuthority())
	{
		return EEGStateTransitionResult::NoAuthority;
	}

	UEGState* State = GetState(StateClass);
	if (!State)
	{
		return EEGStateTransitionResult::NotRegistered;
	}

	// One instance per class means a class can occupy at most one slot in the machine. This is
	// what makes redundant re-entry structurally impossible instead of a flag someone must set.
	if (IsStatePresent(StateClass))
	{
		return EEGStateTransitionResult::AlreadyPresent;
	}

	if (!State->CanEnterState())
	{
		return EEGStateTransitionResult::VetoedByState;
	}

	if (bIsPush && StateStack.Num() >= MaxStackDepth)
	{
		return EEGStateTransitionResult::StackFull;
	}

	if (bIsForce && CurrentState && CurrentState->BlocksInterrupts())
	{
		return EEGStateTransitionResult::BlockedByActiveState;
	}

	OutState = State;
	return EEGStateTransitionResult::Succeeded;
}

EEGStateTransitionResult UEGStateMachineComponent::TrySwitchState(TSubclassOf<UEGState> StateClass, AActor* InContextActor)
{
	UEGState* NextState = nullptr;
	const EEGStateTransitionResult Result = EvaluateTransition(StateClass, /*bIsPush*/ false, /*bIsForce*/ false, NextState);

	LogTransition(TEXT("Switch"), StateClass, Result);
	if (Result != EEGStateTransitionResult::Succeeded)
	{
		return Result;
	}

	const TSubclassOf<UEGState> OldStateClass = GetCurrentStateClass();

	UEGState* Exiting = CurrentState;
	CurrentState = nullptr;
	ExitState(Exiting);
	UnwindStack();

	EnterState(NextState, InContextActor);
	HandleStateChanged(OldStateClass);

	return EEGStateTransitionResult::Succeeded;
}

EEGStateTransitionResult UEGStateMachineComponent::TryPushState(TSubclassOf<UEGState> StateClass, AActor* InContextActor, bool bForce)
{
	UEGState* NextState = nullptr;
	const EEGStateTransitionResult Result = EvaluateTransition(StateClass, /*bIsPush*/ true, bForce, NextState);

	LogTransition(bForce ? TEXT("ForcePush") : TEXT("Push"), StateClass, Result);
	if (Result != EEGStateTransitionResult::Succeeded)
	{
		return Result;
	}

	const TSubclassOf<UEGState> OldStateClass = GetCurrentStateClass();

	if (CurrentState)
	{
		CurrentState->NotifyPause();
		StateStack.Push(CurrentState);
		CurrentState = nullptr;
	}

	EnterState(NextState, InContextActor);
	HandleStateChanged(OldStateClass);

	return EEGStateTransitionResult::Succeeded;
}

bool UEGStateMachineComponent::SwitchState(TSubclassOf<UEGState> StateClass, AActor* InContextActor)
{
	return TrySwitchState(StateClass, InContextActor) == EEGStateTransitionResult::Succeeded;
}

bool UEGStateMachineComponent::PushState(TSubclassOf<UEGState> StateClass, AActor* InContextActor)
{
	return TryPushState(StateClass, InContextActor, /*bForce*/ false) == EEGStateTransitionResult::Succeeded;
}

bool UEGStateMachineComponent::ForcePushState(TSubclassOf<UEGState> StateClass, AActor* InContextActor)
{
	return TryPushState(StateClass, InContextActor, /*bForce*/ true) == EEGStateTransitionResult::Succeeded;
}

bool UEGStateMachineComponent::PopState()
{
	if (!HasAuthority())
	{
		return false;
	}

	if (!CurrentState && StateStack.Num() == 0)
	{
		return false;
	}

	// The brain is the stack floor. Popping it would leave the machine with no decision maker and
	// nothing to resume into; Stop() is the only thing allowed to unwind it.
	if (CurrentState && CurrentState->IsBrainState())
	{
		return false;
	}

	const TSubclassOf<UEGState> OldStateClass = GetCurrentStateClass();

	UEGState* Exiting = CurrentState;
	CurrentState = nullptr;
	ExitState(Exiting);

	if (StateStack.Num() > 0)
	{
		CurrentState = StateStack.Pop();
		if (CurrentState)
		{
			CurrentState->NotifyResume();
		}
	}

	HandleStateChanged(OldStateClass);
	return true;
}

void UEGStateMachineComponent::PopToBrain()
{
	UEGState* Brain = FindBrainState();
	if (!Brain || !HasAuthority() || CurrentState == Brain)
	{
		return;
	}

	if (!StateStack.Contains(Brain))
	{
		UE_LOG(LogEGStateMachine, Warning, TEXT("[%s] PopToBrain: the brain is not on the stack; use Start() to re-establish it."),
			*GetNameSafe(GetOwner()));
		return;
	}

	const TSubclassOf<UEGState> OldStateClass = GetCurrentStateClass();

	// Unwind in one pass and resume the brain once at the end, rather than looping on PopState.
	// Popping one at a time would resume the brain between pops, and a brain that re-evaluates on
	// resume would immediately re-push whatever was just removed — an unwind that never finishes.
	UEGState* Exiting = CurrentState;
	CurrentState = nullptr;
	ExitState(Exiting);

	while (StateStack.Num() > 0 && StateStack.Last() != Brain)
	{
		UEGState* Paused = StateStack.Pop();
		ExitState(Paused);
	}

	if (StateStack.Num() > 0 && StateStack.Last() == Brain)
	{
		CurrentState = StateStack.Pop();
		CurrentState->NotifyResume();
	}

	HandleStateChanged(OldStateClass);
}

void UEGStateMachineComponent::EnterState(UEGState* NewState, AActor* InContextActor)
{
	if (!NewState)
	{
		return;
	}

	// A transition without an explicit context does not clear the existing one — context outlives
	// individual transitions by design (a hunt target survives a stun interrupt).
	if (InContextActor)
	{
		ContextActor = InContextActor;
	}

	CurrentState = NewState;
	NewState->NotifyEnter();
}

void UEGStateMachineComponent::ExitState(UEGState* State)
{
	if (!State)
	{
		return;
	}

	State->NotifyExit();
	HandleAutoRemoval(State);
}

void UEGStateMachineComponent::UnwindStack()
{
	// Paused states are leaving too, so they get OnExit rather than being silently dropped —
	// otherwise anything they acquired on enter (movement locks, immunity counters) leaks.
	while (StateStack.Num() > 0)
	{
		UEGState* Paused = StateStack.Pop();
		ExitState(Paused);
	}
}

void UEGStateMachineComponent::HandleAutoRemoval(UEGState* ExitedState)
{
	if (!ExitedState || !ExitedState->ShouldRemoveWhenExited())
	{
		return;
	}

	if (ExitedState == CurrentState || StateStack.Contains(ExitedState))
	{
		return;
	}

	UnregisterStateInstance(ExitedState);
}

void UEGStateMachineComponent::HandleStateChanged(TSubclassOf<UEGState> OldStateClass)
{
	const TSubclassOf<UEGState> NewStateClass = GetCurrentStateClass();

	if (NewStateClass != OldStateClass)
	{
		const UWorld* World = GetWorld();
		ActiveStateEnteredAt = World ? World->GetTimeSeconds() : 0.0;
	}

	if (HasAuthority())
	{
		PublishNetState();
	}

	if (bClearDebugLogOnStateChange)
	{
		DebugLogEntries.Reset();
	}

	DebugLog(FString::Printf(TEXT("%s -> %s (depth %d)"),
		*GetNameSafe(OldStateClass.Get()), *GetNameSafe(NewStateClass.Get()), StateStack.Num()));

	OnStateChanged.Broadcast(NewStateClass, OldStateClass);
}

// -----------------------------------------------------------------------------
// Replication
// -----------------------------------------------------------------------------

void UEGStateMachineComponent::PublishNetState()
{
	NetState.CurrentStateClass = GetCurrentStateClass();
	NetState.StackClasses = GetStateStack();
	NetState.ContextActor = ContextActor;

	// Wraps deliberately: only inequality matters, and it is what makes A -> B -> A visible.
	++NetState.Serial;
}

void UEGStateMachineComponent::OnRep_NetState()
{
	MirrorNetState();
}

void UEGStateMachineComponent::ApplyNetStateSnapshot(const FEGStateMachineNetState& InNetState)
{
	NetState = InNetState;
	MirrorNetState();
}

UEGState* UEGStateMachineComponent::ResolveOrRegisterMirroredState(TSubclassOf<UEGState> StateClass)
{
	if (!StateClass)
	{
		return nullptr;
	}

	if (UEGState* Existing = GetState(StateClass))
	{
		return Existing;
	}

	// A state given at runtime on the server arrives here as a class the mirror has never seen.
	// It is built from the CDO, so per-run data has to come from the state's own replication or
	// from the context actor — never from an assumption that both sides tuned it identically.
	if (StateClass->HasAnyClassFlags(CLASS_Abstract))
	{
		return nullptr;
	}

	return RegisterStateInstance(NewObject<UEGState>(this, StateClass));
}

void UEGStateMachineComponent::MirrorNetState()
{
	const TSubclassOf<UEGState> OldStateClass = GetCurrentStateClass();

	// Desired list, bottom to top, with the active state last.
	TArray<UEGState*> Desired;
	Desired.Reserve(NetState.StackClasses.Num() + 1);
	for (const TSubclassOf<UEGState>& StackClass : NetState.StackClasses)
	{
		if (UEGState* State = ResolveOrRegisterMirroredState(StackClass))
		{
			Desired.Add(State);
		}
	}
	if (UEGState* Current = ResolveOrRegisterMirroredState(NetState.CurrentStateClass))
	{
		Desired.Add(Current);
	}

	// Local list in the same shape.
	TArray<UEGState*> Local;
	Local.Reserve(StateStack.Num() + 1);
	for (const TObjectPtr<UEGState>& State : StateStack)
	{
		Local.Add(State);
	}
	if (CurrentState)
	{
		Local.Add(CurrentState);
	}

	int32 Common = 0;
	while (Common < Desired.Num() && Common < Local.Num() && Desired[Common] == Local[Common])
	{
		++Common;
	}

	// Unwind everything above the shared prefix, top-down.
	const int32 LocalCountBefore = Local.Num();
	for (int32 Index = LocalCountBefore - 1; Index >= Common; --Index)
	{
		MirrorExit(Local[Index]);
	}
	Local.SetNum(Common);

	const bool bRemovedAny = LocalCountBefore > Common;

	// Nothing new is going on top, so the state now exposed is resuming rather than re-entering.
	if (bRemovedAny && Local.Num() > 0 && Desired.Num() == Local.Num())
	{
		MirrorResume(Local.Last());
	}

	for (int32 Index = Common; Index < Desired.Num(); ++Index)
	{
		if (Local.Num() > 0)
		{
			MirrorPause(Local.Last());
		}

		MirrorEnter(Desired[Index]);
		Local.Add(Desired[Index]);
	}

	// Commit the mirror.
	CurrentState = Local.Num() > 0 ? Local.Last() : nullptr;
	StateStack.Reset(FMath::Max(Local.Num() - 1, 0));
	for (int32 Index = 0; Index < Local.Num() - 1; ++Index)
	{
		StateStack.Add(Local[Index]);
	}

	ContextActor = NetState.ContextActor;

	// A mirror only needs to tick while a mirrored state asked for it; otherwise a client machine
	// costs one OnRep per transition and nothing per frame.
	UpdateTickEnabled();

	if (bClearDebugLogOnStateChange)
	{
		DebugLogEntries.Reset();
	}

	DebugLog(FString::Printf(TEXT("[mirror] %s -> %s (depth %d)"),
		*GetNameSafe(OldStateClass.Get()), *GetNameSafe(GetCurrentStateClass().Get()), StateStack.Num()));

	OnStateChanged.Broadcast(GetCurrentStateClass(), OldStateClass);
}

void UEGStateMachineComponent::MirrorEnter(UEGState* State)
{
	if (State && State->RunsOnSimulatedProxy())
	{
		State->NotifyEnter();
	}
}

void UEGStateMachineComponent::MirrorExit(UEGState* State)
{
	if (State && State->RunsOnSimulatedProxy())
	{
		State->NotifyExit();
	}
}

void UEGStateMachineComponent::MirrorPause(UEGState* State)
{
	if (State && State->RunsOnSimulatedProxy())
	{
		State->NotifyPause();
	}
}

void UEGStateMachineComponent::MirrorResume(UEGState* State)
{
	if (State && State->RunsOnSimulatedProxy())
	{
		State->NotifyResume();
	}
}

// -----------------------------------------------------------------------------
// Queries
// -----------------------------------------------------------------------------

TSubclassOf<UEGState> UEGStateMachineComponent::GetCurrentStateClass() const
{
	return CurrentState ? CurrentState->GetClass() : nullptr;
}

bool UEGStateMachineComponent::IsCurrentState(TSubclassOf<UEGState> StateClass) const
{
	return StateClass && CurrentState && CurrentState->GetClass() == StateClass.Get();
}

TArray<TSubclassOf<UEGState>> UEGStateMachineComponent::GetStateStack() const
{
	TArray<TSubclassOf<UEGState>> Result;
	Result.Reserve(StateStack.Num());
	for (const TObjectPtr<UEGState>& State : StateStack)
	{
		if (State)
		{
			Result.Add(State->GetClass());
		}
	}
	return Result;
}

bool UEGStateMachineComponent::IsStatePresent(TSubclassOf<UEGState> StateClass) const
{
	if (!StateClass)
	{
		return false;
	}

	if (IsCurrentState(StateClass))
	{
		return true;
	}

	for (const TObjectPtr<UEGState>& State : StateStack)
	{
		if (State && State->GetClass() == StateClass.Get())
		{
			return true;
		}
	}

	return false;
}

FGameplayTag UEGStateMachineComponent::GetCurrentStateTag() const
{
	return CurrentState ? CurrentState->GetStateTag() : FGameplayTag();
}

float UEGStateMachineComponent::GetTimeInActiveState() const
{
	const UWorld* World = GetWorld();
	if (!World || !CurrentState)
	{
		return 0.0f;
	}

	return static_cast<float>(FMath::Max(0.0, World->GetTimeSeconds() - ActiveStateEnteredAt));
}

UEGState* UEGStateMachineComponent::GetBrainState() const
{
	return FindBrainState();
}

UEGState* UEGStateMachineComponent::FindBrainState() const
{
	UEGState* Brain = nullptr;

	for (const TObjectPtr<UEGState>& State : RegisteredStates)
	{
		if (!State || !State->IsBrainState())
		{
			continue;
		}

		if (Brain)
		{
			UE_LOG(LogEGStateMachine, Error, TEXT("[%s] Multiple brain states registered ('%s' and '%s'). The machine needs exactly one."),
				*GetNameSafe(GetOwner()), *GetNameSafe(Brain->GetClass()), *GetNameSafe(State->GetClass()));
			return nullptr;
		}

		Brain = State;
	}

	return Brain;
}

void UEGStateMachineComponent::SetContextActor(AActor* InContextActor)
{
	ContextActor = InContextActor;
}

void UEGStateMachineComponent::ClearContextActor()
{
	ContextActor = nullptr;
}

bool UEGStateMachineComponent::HasAuthority() const
{
	const AActor* Owner = GetOwner();
	return Owner && Owner->HasAuthority();
}

// -----------------------------------------------------------------------------
// Debug
// -----------------------------------------------------------------------------

void UEGStateMachineComponent::DebugLog(const FString& Message)
{
	DebugLogEntries.Add(Message);

	while (DebugLogEntries.Num() > DebugLogMaxEntries)
	{
		DebugLogEntries.RemoveAt(0, 1, EAllowShrinking::No);
	}

	if (bDebugLogStateChanges)
	{
		UE_LOG(LogEGStateMachine, Verbose, TEXT("[%s] %s"), *GetNameSafe(GetOwner()), *Message);
	}
}

FString UEGStateMachineComponent::BuildDebugSummary() const
{
	// The side matters more than anything else here: mirroring desync is invisible otherwise.
	const TCHAR* Side = HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT");

	FString Summary = FString::Printf(TEXT("[%s] %s"), Side,
		CurrentState ? *CurrentState->GetClass()->GetName() : TEXT("None"));

	if (!bIsRunning && HasAuthority())
	{
		Summary += TEXT(" (stopped)");
	}

	// The stack view says which state is running; only the state itself knows what it is doing.
	if (CurrentState)
	{
		const FString SubPhase = CurrentState->GetStateDebugString();
		if (!SubPhase.IsEmpty())
		{
			Summary += FString::Printf(TEXT(" — %s"), *SubPhase);
		}
	}

	if (StateStack.Num() > 0)
	{
		Summary += FString::Printf(TEXT("\n[Stack: %d]"), StateStack.Num());
		for (int32 Index = StateStack.Num() - 1; Index >= 0; --Index)
		{
			Summary += FString::Printf(TEXT("\n  (%s)"), *GetNameSafe(StateStack[Index] ? StateStack[Index]->GetClass() : nullptr));
		}
	}

	return Summary;
}

void UEGStateMachineComponent::DrawDebugStateInfo() const
{
#if ENABLE_DRAW_DEBUG
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	const FVector DrawLocation = Owner->GetActorLocation() + FVector(0.f, 25.f, DebugTextOffset);
	const FColor SummaryColor = CurrentState ? DebugActiveColor : DebugPausedColor;

	DrawDebugString(GetWorld(), DrawLocation, BuildDebugSummary(), nullptr, SummaryColor, 0.f, true, 1.2f);

	if (DebugLogEntries.Num() > 0)
	{
		const FString LogText = FString::Join(DebugLogEntries, TEXT("\n"));
		DrawDebugString(GetWorld(), DrawLocation - FVector(0.f, 0.f, 18.f), LogText, nullptr, FColor::Cyan, 0.f, true, 0.85f);
	}
#endif
}

void UEGStateMachineComponent::LogTransition(const TCHAR* Verb, TSubclassOf<UEGState> StateClass, EEGStateTransitionResult Result) const
{
	if (!bDebugLogStateChanges || Result == EEGStateTransitionResult::Succeeded)
	{
		return;
	}

	// Refusals are the hard failure mode to diagnose — a silently rejected transition looks
	// identical to a brain that never asked.
	UE_LOG(LogEGStateMachine, Verbose, TEXT("[%s] %s '%s' refused: %s (current '%s', depth %d)"),
		*GetNameSafe(GetOwner()), Verb, *GetNameSafe(StateClass.Get()), EGLexToString(Result),
		*GetNameSafe(GetCurrentStateClass().Get()), StateStack.Num());
}
