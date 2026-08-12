// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EGStateMachineTypes.h"

#include "EGStateMachineComponent.generated.h"

/**
 * UEGStateMachineComponent
 *
 * Owns a set of UEGState instances and runs one of them at a time, with a stack for interrupts.
 *
 * Core invariant: at most one instance of any given state class exists per component, so a state
 * class appears at most once in the machine at a time. That makes a class reference a stable
 * identity for an instance (used by replication), makes duplicate stack entries impossible, and
 * lets state instances keep their data — and their editor-tuned property values — across
 * activations.
 *
 * Simulation is authority-only: every transition entry point refuses to run without authority.
 *
 * Scope: registration, stack, transitions, lifecycle, timers, debug ring log, replication
 * (FEGStateMachineNetState plus client mirroring) and the brain requirement. All of it has
 * shipped — an earlier version of this comment described replication and the brain as landing in
 * later phases, which stopped being true and misled anyone reading the header to decide whether
 * they could rely on them.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class UNREALEXTENDEDGAMEPLAY_API UEGStateMachineComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEGStateMachineComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void ReadyForReplication() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** The replicated snapshot: current state, the stack under it, the context actor, and a change counter. */
	const FEGStateMachineNetState& GetNetState() const { return NetState; }

	/**
	 * Installs a replicated snapshot and runs the client mirroring pass.
	 *
	 * This is the seam OnRep_NetState goes through, exposed so the mirroring algorithm can be
	 * driven directly without standing up a net driver.
	 */
	void ApplyNetStateSnapshot(const FEGStateMachineNetState& InNetState);

	// -----------------------------------------------------------------
	// Registration
	// -----------------------------------------------------------------

	/**
	 * Adds an instanced state definition. Safe to call from a native constructor with
	 * CreateDefaultSubobject, which is the intended authoring route for C++ owners.
	 * If the owner has already begun play the instance is registered immediately.
	 */
	UEGState* AddStateDefinition(UEGState* StateInstance);

	/** Registers a new instance of StateClass at runtime. Returns the existing one if already registered. */
	UFUNCTION(BlueprintCallable, Category = "Extended|State Machine", meta = (DeterminesOutputType = "StateClass"))
	UEGState* GiveState(TSubclassOf<UEGState> StateClass);

	/** Unregisters a state. Pops it first if it is active. Refuses if it is paused in the stack, or is the brain. */
	UFUNCTION(BlueprintCallable, Category = "Extended|State Machine")
	bool RemoveState(TSubclassOf<UEGState> StateClass);

	UFUNCTION(BlueprintPure, Category = "Extended|State Machine")
	bool HasState(TSubclassOf<UEGState> StateClass) const;

	UFUNCTION(BlueprintPure, Category = "Extended|State Machine", meta = (DeterminesOutputType = "StateClass"))
	UEGState* GetState(TSubclassOf<UEGState> StateClass) const;

	UFUNCTION(BlueprintPure, Category = "Extended|State Machine")
	TArray<UEGState*> GetRegisteredStates() const;

	// -----------------------------------------------------------------
	// Runtime control
	// -----------------------------------------------------------------

	/** Enters the brain (or DefaultStateClass if one was set) and enables tick. Authority only. */
	UFUNCTION(BlueprintCallable, Category = "Extended|State Machine")
	void Start();

	/** Unwinds the whole stack top-down and disables tick. */
	UFUNCTION(BlueprintCallable, Category = "Extended|State Machine")
	void Stop();

	UFUNCTION(BlueprintPure, Category = "Extended|State Machine")
	bool IsRunning() const { return bIsRunning; }

	/** Overrides the start state. When unset, Start() uses the registered brain. */
	UFUNCTION(BlueprintCallable, Category = "Extended|State Machine")
	void SetDefaultStateByClass(TSubclassOf<UEGState> StateClass);

	/** Must be set before the component is registered to have any effect on the automatic start. */
	UFUNCTION(BlueprintCallable, Category = "Extended|State Machine")
	void SetAutoStart(bool bInAutoStart) { bAutoStart = bInAutoStart; }

	// -----------------------------------------------------------------
	// Transitions — authority only
	// -----------------------------------------------------------------

	/** Exits the current state, unwinds the stack, and enters StateClass. */
	UFUNCTION(BlueprintCallable, Category = "Extended|State Machine")
	bool SwitchState(TSubclassOf<UEGState> StateClass, AActor* InContextActor = nullptr);

	/** Pauses the current state and enters StateClass on top of it. */
	UFUNCTION(BlueprintCallable, Category = "Extended|State Machine")
	bool PushState(TSubclassOf<UEGState> StateClass, AActor* InContextActor = nullptr);

	/** As PushState, but refused when the active state has bBlocksInterrupts. */
	UFUNCTION(BlueprintCallable, Category = "Extended|State Machine")
	bool ForcePushState(TSubclassOf<UEGState> StateClass, AActor* InContextActor = nullptr);

	/** Exits the current state and resumes the one beneath it. */
	UFUNCTION(BlueprintCallable, Category = "Extended|State Machine")
	bool PopState();

	/** Pops until the brain is the active state. No-op when no brain is registered. */
	UFUNCTION(BlueprintCallable, Category = "Extended|State Machine")
	void PopToBrain();

	/** Transition entry points with the refusal reason, for callers that want to react to it. */
	EEGStateTransitionResult TrySwitchState(TSubclassOf<UEGState> StateClass, AActor* InContextActor = nullptr);
	EEGStateTransitionResult TryPushState(TSubclassOf<UEGState> StateClass, AActor* InContextActor = nullptr, bool bForce = false);

	// -----------------------------------------------------------------
	// Queries
	// -----------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Extended|State Machine")
	UEGState* GetCurrentState() const { return CurrentState; }

	UFUNCTION(BlueprintPure, Category = "Extended|State Machine")
	TSubclassOf<UEGState> GetCurrentStateClass() const;

	UFUNCTION(BlueprintPure, Category = "Extended|State Machine")
	bool IsCurrentState(TSubclassOf<UEGState> StateClass) const;

	/** Paused states, bottom to top. Excludes the current state. */
	UFUNCTION(BlueprintPure, Category = "Extended|State Machine")
	TArray<TSubclassOf<UEGState>> GetStateStack() const;

	UFUNCTION(BlueprintPure, Category = "Extended|State Machine")
	int32 GetStackDepth() const { return StateStack.Num(); }

	/** True when StateClass is the current state or is paused in the stack. */
	UFUNCTION(BlueprintPure, Category = "Extended|State Machine")
	bool IsStatePresent(TSubclassOf<UEGState> StateClass) const;

	UFUNCTION(BlueprintPure, Category = "Extended|State Machine")
	FGameplayTag GetCurrentStateTag() const;

	/** Seconds since the active state last changed. 0 when nothing is active. */
	UFUNCTION(BlueprintPure, Category = "Extended|State Machine")
	float GetTimeInActiveState() const;

	UFUNCTION(BlueprintPure, Category = "Extended|State Machine")
	UEGState* GetBrainState() const;

	UFUNCTION(BlueprintPure, Category = "Extended|State Machine")
	AActor* GetContextActor() const { return ContextActor; }

	UFUNCTION(BlueprintCallable, Category = "Extended|State Machine")
	void SetContextActor(AActor* InContextActor);

	UFUNCTION(BlueprintCallable, Category = "Extended|State Machine")
	void ClearContextActor();

	UFUNCTION(BlueprintPure, Category = "Extended|State Machine")
	bool HasAuthority() const;

	// -----------------------------------------------------------------
	// Debug
	// -----------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Extended|State Machine|Debug")
	void DebugLog(const FString& Message);

	UFUNCTION(BlueprintPure, Category = "Extended|State Machine|Debug")
	TArray<FString> GetDebugLog() const { return DebugLogEntries; }

	/**
	 * One-line summary used by the world overlay and the gameplay debugger:
	 * side, current state, sub-phase, and the paused stack under it.
	 */
	UFUNCTION(BlueprintPure, Category = "Extended|State Machine|Debug")
	FString BuildDebugSummary() const;

	UPROPERTY(BlueprintAssignable, Category = "Extended|State Machine")
	FOnEGStateChanged OnStateChanged;

protected:
	/** Per-actor state instances, edited inline in the details panel. */
	UPROPERTY(EditAnywhere, Instanced, Category = "Extended|State Machine")
	TArray<TObjectPtr<UEGState>> StateDefinitions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extended|State Machine")
	bool bAutoStart = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extended|State Machine", meta = (ClampMin = "1", ClampMax = "16"))
	int32 MaxStackDepth = 5;

	/** Draws the current state, the stack and the debug log above the owner. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extended|State Machine|Debug")
	bool bDebugDraw = false;

	/** Logs every transition, including refusals and their reason, to LogEGStateMachine. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extended|State Machine|Debug")
	bool bDebugLogStateChanges = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extended|State Machine|Debug", meta = (EditCondition = "bDebugDraw"))
	FColor DebugActiveColor = FColor::Green;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extended|State Machine|Debug", meta = (EditCondition = "bDebugDraw"))
	FColor DebugPausedColor = FColor::Yellow;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extended|State Machine|Debug", meta = (EditCondition = "bDebugDraw", ClampMin = "0"))
	float DebugTextOffset = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extended|State Machine|Debug", meta = (ClampMin = "1", ClampMax = "50"))
	int32 DebugLogMaxEntries = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extended|State Machine|Debug")
	bool bClearDebugLogOnStateChange = true;

private:
	UEGState* RegisterStateInstance(UEGState* StateInstance);
	void UnregisterStateInstance(UEGState* StateInstance);
	void EnsureStatesRegistered();

	/** Authority: rebuild the replicated snapshot from the live stack and bump the change counter. */
	void PublishNetState();

	/** Client: bring the local mirror in line with the snapshot, emitting the minimum lifecycle. */
	void MirrorNetState();

	/** Resolves a replicated class to this machine's own instance, registering it if unseen. */
	UEGState* ResolveOrRegisterMirroredState(TSubclassOf<UEGState> StateClass);

	// Mirror-side lifecycle: states that did not opt in stay silent placeholders (see bRunOnSimulatedProxy).
	void MirrorEnter(UEGState* State);
	void MirrorExit(UEGState* State);
	void MirrorPause(UEGState* State);
	void MirrorResume(UEGState* State);

	UFUNCTION()
	void OnRep_NetState();

	/** Runs the shared transition gate. Returns Succeeded and fills OutState when the move is legal. */
	EEGStateTransitionResult EvaluateTransition(TSubclassOf<UEGState> StateClass, bool bIsPush, bool bIsForce, UEGState*& OutState) const;

	void EnterState(UEGState* NewState, AActor* InContextActor);
	void ExitState(UEGState* State);
	void UnwindStack();
	void HandleStateChanged(TSubclassOf<UEGState> OldStateClass);
	void HandleAutoRemoval(UEGState* ExitedState);
	void LogTransition(const TCHAR* Verb, TSubclassOf<UEGState> StateClass, EEGStateTransitionResult Result) const;
	UEGState* FindBrainState() const;
	void DrawDebugStateInfo() const;

	/** Tick is needed while simulating, while a mirrored state asked for it, or to draw the overlay. */
	void UpdateTickEnabled();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UEGState>> RegisteredStates;

	UPROPERTY(Transient)
	TObjectPtr<UEGState> CurrentState;

	/** Paused states, bottom to top. The current state is not in here. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UEGState>> StateStack;

	UPROPERTY(Transient)
	TObjectPtr<AActor> ContextActor;

	UPROPERTY(Transient)
	TSubclassOf<UEGState> DefaultStateClass;

	UPROPERTY(ReplicatedUsing = OnRep_NetState)
	FEGStateMachineNetState NetState;

	/** Non-reflected fast lookup; ownership is held by RegisteredStates. */
	TMap<UClass*, UEGState*> StatesByClass;

	TArray<FString> DebugLogEntries;

	bool bIsRunning = false;
	bool bStatesRegistered = false;

	/** World seconds at which the active state last changed; feeds GetTimeInActiveState. */
	double ActiveStateEnteredAt = 0.0;
};
