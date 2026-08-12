// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EGState.h"
#include "EGStateMachineComponent.generated.h"

class AActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEGStateChanged, FName, OldState, FName, NewState);

/**
 * UEGStateMachineComponent
 *
 * A tickable state-machine component that manages a pool of UEGState
 * objects and ticks the active one.
 *
 * Features:
 * - Data-driven state configuration via StateDefinitions (EditDefaultsOnly)
 * - Single active state with OnEnter / OnTick / OnExit lifecycle
 * - Push/Pop stack for interrupt states (dialogue, knockdown)
 * - Auto-registers and auto-starts from editor-configured data
 *
 * Networking:
 * The simulation is server-authoritative — only the authority ever runs states
 * (see Start()). The observable result of that simulation (running flag, current
 * state class, stack and context actor) is replicated so simulated proxies can
 * query the machine and react to OnStateChanged. Clients never run a second,
 * divergent simulation.
 */
UCLASS(ClassGroup=(Extended), meta = (BlueprintSpawnableComponent))
class UNREALEXTENDEDGAMEPLAY_API UEGStateMachineComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEGStateMachineComponent();

	// -----------------------------------------------------------------
	// State Registration
	// -----------------------------------------------------------------

	/** Add an instanced default state definition to StateDefinitions. Safe to call from native constructors. */
	UEGState* AddStateDefinition(UEGState* StateInstance);

	UFUNCTION(BlueprintCallable, Category = "StateMachine")
	UEGState* RegisterState(TSubclassOf<UEGState> StateClass);

	UFUNCTION(BlueprintCallable, Category = "StateMachine")
	UEGState* RegisterInstancedState(UEGState* StateInstance);

	UFUNCTION(BlueprintPure, Category = "StateMachine")
	UEGState* GetStateByClass(TSubclassOf<UEGState> StateClass) const;

	UFUNCTION(BlueprintCallable, Category = "StateMachine")
	void SetDefaultStateByClass(TSubclassOf<UEGState> StateClass);

	// -----------------------------------------------------------------
	// Lifecycle Control
	// -----------------------------------------------------------------

	/**
	 * Enable/disable the automatic Start() in BeginPlay.
	 *
	 * Owners that only know their state set after BeginPlay has begun turn this off in their
	 * constructor and call Start() themselves.
	 */
	UFUNCTION(BlueprintCallable, Category = "StateMachine")
	void SetAutoStart(bool bInAutoStart) { bAutoStart = bInAutoStart; }

	UFUNCTION(BlueprintCallable, Category = "StateMachine")
	void Start();

	UFUNCTION(BlueprintCallable, Category = "StateMachine")
	void Stop();

	UFUNCTION(BlueprintPure, Category = "StateMachine")
	bool IsRunning() const;

	UFUNCTION(BlueprintPure, Category = "StateMachine")
	bool HasDefaultState() const { return DefaultStateClass != nullptr; }

	/** True if a brain state is registered (the implicit base state used when no default is set). */
	UFUNCTION(BlueprintPure, Category = "StateMachine")
	bool HasBrainState() const;

	// -----------------------------------------------------------------
	// State Transitions
	// -----------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "StateMachine")
	bool SwitchStateByClass(TSubclassOf<UEGState> StateClass);

	UFUNCTION(BlueprintCallable, Category = "StateMachine")
	bool PushStateByClass(TSubclassOf<UEGState> StateClass);

	UFUNCTION(BlueprintCallable, Category = "StateMachine")
	bool ForcePushStateByClass(TSubclassOf<UEGState> StateClass);

	UFUNCTION(BlueprintPure, Category = "StateMachine")
	bool IsCurrentStateClass(TSubclassOf<UEGState> StateClass) const;

	UFUNCTION(BlueprintCallable, Category = "StateMachine")
	void PopState();

	UFUNCTION(BlueprintCallable, Category = "StateMachine")
	bool UnregisterStateByClassId(FName StateClassId);

	UFUNCTION(BlueprintCallable, Category = "StateMachine")
	void SetStateContextActor(AActor* Actor);

	UFUNCTION(BlueprintPure, Category = "StateMachine")
	AActor* GetStateContextActor() const;

	UFUNCTION(BlueprintCallable, Category = "StateMachine")
	void ClearStateContextActor();

	// -----------------------------------------------------------------
	// Queries
	// -----------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "StateMachine")
	TSubclassOf<UEGState> GetCurrentStateClass() const;

	UFUNCTION(BlueprintPure, Category = "StateMachine")
	int32 GetStackDepth() const;

	UFUNCTION(BlueprintPure, Category = "StateMachine")
	TMap<FName, UEGState*> GetRegisteredStates() const;

	UFUNCTION(BlueprintPure, Category = "StateMachine")
	TArray<FName> GetStateStack() const;

	// -----------------------------------------------------------------
	// Delegates
	// -----------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "StateMachine")
	FOnEGStateChanged OnStateChanged;

	// -----------------------------------------------------------------
	// UActorComponent Overrides
	// -----------------------------------------------------------------

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UPROPERTY(EditAnywhere, Instanced, Category = "StateMachine")
	TArray<TObjectPtr<UEGState>> StateDefinitions;

	UPROPERTY(EditAnywhere, Category = "StateMachine")
	bool bAutoStart = true;

	UPROPERTY(EditAnywhere, Category = "StateMachine", meta = (ClampMin = "1", ClampMax = "10"))
	int32 MaxStackDepth = 5;

	// =================================================================
	// Debug
	// =================================================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StateMachine|Debug")
	bool bDebugDraw = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StateMachine|Debug")
	bool bDebugLogStateChanges = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StateMachine|Debug", meta = (EditCondition = "bDebugDraw"))
	FColor DebugActiveColor = FColor::Green;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StateMachine|Debug", meta = (EditCondition = "bDebugDraw"))
	FColor DebugPausedColor = FColor::Yellow;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StateMachine|Debug", meta = (EditCondition = "bDebugDraw", ClampMin = "0"))
	float DebugTextOffset = 120.f;

	/** Max entries kept in the per-owner debug log ring buffer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StateMachine|Debug", meta = (EditCondition = "bDebugDraw", ClampMin = "1", ClampMax = "50"))
	int32 DebugLogMaxEntries = 8;

	/** If true, the debug log is cleared whenever the active state changes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StateMachine|Debug", meta = (EditCondition = "bDebugDraw"))
	bool bClearDebugLogOnStateChange = true;

public:
	/**
	 * Append a line to this owner's debug log ring buffer.
	 * Visible in the world debug overlay when bDebugDraw is true.
	 * States should call this instead of UE_LOG for per-owner debug output.
	 */
	void DebugLog(const FString& Message);

private:
	UPROPERTY()
	TMap<FName, UEGState*> RegisteredStates;

	UPROPERTY()
	TArray<TObjectPtr<UEGState>> RegisteredStateInstances;

	TMap<UClass*, UEGState*> RegisteredStatesByClass;

	UPROPERTY()
	TObjectPtr<UEGState> CurrentState;

	FName CurrentStateClassId = NAME_None;

	UPROPERTY()
	TArray<TObjectPtr<UEGState>> StateStack;

	UPROPERTY(Transient)
	TObjectPtr<UClass> DefaultStateClass;
	bool bIsRunning = false;

	TWeakObjectPtr<AActor> StateContextActor;

	// -----------------------------------------------------------------
	// Replicated mirror of the authority's simulation
	// -----------------------------------------------------------------

	UPROPERTY(ReplicatedUsing = OnRep_ReplicatedCurrentStateClass)
	TSubclassOf<UEGState> ReplicatedCurrentStateClass;

	UPROPERTY(Replicated)
	TArray<TSubclassOf<UEGState>> ReplicatedStateStack;

	UPROPERTY(Replicated)
	TObjectPtr<AActor> ReplicatedStateContextActor;

	UPROPERTY(Replicated)
	bool bReplicatedIsRunning = false;

	UFUNCTION()
	void OnRep_ReplicatedCurrentStateClass();

	/** True when this machine owns the simulation (server / standalone). */
	bool HasStateMachineAuthority() const;

	/** Push the authority's current/stack/running data into the replicated mirror. */
	void UpdateReplicatedState();

	FName GetStateClassId(const UEGState* State) const;
	FName FindStateClassId(const UEGState* State) const;
	UEGState* FindBrainState() const;
	bool IsStateCurrentOrStacked(const UEGState* State) const;
	void DrawDebugStateInfo() const;

	/** Circular debug-log entries fed by states. */
	TArray<FString> DebugLogEntries;
};
