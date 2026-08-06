// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"
#include "UObject/Object.h"

#include "EGState.generated.h"

class AAIController;
class AController;
class APawn;
class UEGStateMachineComponent;

/**
 * UEGState
 *
 * Base state object owned by a UEGStateMachineComponent.
 *
 * One instance exists per state class per component, created once and reused for the lifetime of
 * the component (see UEGStateMachineComponent). Instances are never shared between actors, so a
 * state may safely hold per-actor runtime data across activations.
 *
 * States are deliberately allowed to be large: a single state may drive a whole multi-phase
 * behaviour rather than being split into micro-states. Use GetStateDebugString() to surface the
 * internal phase, and SetStateTimer() for any timing so handles cannot outlive the state.
 *
 * EditInlineNew + DefaultToInstanced is what lets each owning actor edit this state's properties
 * inline in the component's StateDefinitions array.
 */
UCLASS(Abstract, Blueprintable, BlueprintType, EditInlineNew, DefaultToInstanced)
class UNREALEXTENDEDGAMEPLAY_API UEGState : public UObject
{
	GENERATED_BODY()

public:
	// -----------------------------------------------------------------
	// Lifecycle — override these in subclasses
	// -----------------------------------------------------------------

	/** Called when this instance is registered with the component (BeginPlay or runtime GiveState). */
	UFUNCTION(BlueprintNativeEvent, Category = "Extended|State Machine")
	void OnGiven();
	virtual void OnGiven_Implementation() {}

	/** Called when this state becomes the active state. */
	UFUNCTION(BlueprintNativeEvent, Category = "Extended|State Machine")
	void OnEnter();
	virtual void OnEnter_Implementation() {}

	/** Called every tick step while active. Honours TickInterval — DeltaTime is the accumulated time. */
	UFUNCTION(BlueprintNativeEvent, Category = "Extended|State Machine")
	void OnTick(float DeltaTime);
	virtual void OnTick_Implementation(float DeltaTime) {}

	/** Called when another state is pushed on top of this one. */
	UFUNCTION(BlueprintNativeEvent, Category = "Extended|State Machine")
	void OnPause();
	virtual void OnPause_Implementation() {}

	/** Called when the state above this one pops. */
	UFUNCTION(BlueprintNativeEvent, Category = "Extended|State Machine")
	void OnResume();
	virtual void OnResume_Implementation() {}

	/** Called when this state stops being active (switched away, popped, or machine stopped). */
	UFUNCTION(BlueprintNativeEvent, Category = "Extended|State Machine")
	void OnExit();
	virtual void OnExit_Implementation() {}

	/** Called when this instance is unregistered from the component. */
	UFUNCTION(BlueprintNativeEvent, Category = "Extended|State Machine")
	void OnRemoved();
	virtual void OnRemoved_Implementation() {}

	/** Destination-side veto. Checked by every transition entry point before this state is entered. */
	UFUNCTION(BlueprintNativeEvent, Category = "Extended|State Machine")
	bool CanEnterState() const;
	virtual bool CanEnterState_Implementation() const { return true; }

	/** True for the single root decision state the machine starts with. See UEGBrainState. */
	virtual bool IsBrainState() const { return false; }

	// -----------------------------------------------------------------
	// Transition policy — read by the component
	// -----------------------------------------------------------------

	bool ShouldPushWhenGiven() const { return bPushWhenGiven; }
	bool ShouldRemoveWhenExited() const { return bRemoveWhenExited; }
	bool BlocksInterrupts() const { return bBlocksInterrupts; }
	bool RunsOnSimulatedProxy() const { return bRunOnSimulatedProxy; }
	bool TicksOnSimulatedProxy() const { return bRunOnSimulatedProxy && bTickOnSimulatedProxy; }
	bool ShouldReplicateState() const { return bReplicateState; }
	bool TicksWhilePaused() const { return bTickWhilePaused; }

	/** Replicated states are registered as subobjects of their machine; see bReplicateState. */
	virtual bool IsSupportedForNetworking() const override { return bReplicateState; }

	UFUNCTION(BlueprintPure, Category = "Extended|State Machine")
	FGameplayTag GetStateTag() const { return StateTag; }

	/** True while this state is the active state of its machine. */
	UFUNCTION(BlueprintPure, Category = "Extended|State Machine")
	bool IsStateActive() const { return bIsActive; }

	// -----------------------------------------------------------------
	// State machine control
	// -----------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Extended|State Machine")
	bool RequestSwitchState(TSubclassOf<UEGState> StateClass, AActor* InContextActor = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Extended|State Machine")
	bool RequestPushState(TSubclassOf<UEGState> StateClass, AActor* InContextActor = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Extended|State Machine")
	void RequestPopState();

	// -----------------------------------------------------------------
	// Accessors
	// -----------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Extended|State Machine")
	UEGStateMachineComponent* GetOwningMachine() const;

	UFUNCTION(BlueprintPure, Category = "Extended|State Machine")
	AActor* GetOwningActor() const;

	UFUNCTION(BlueprintPure, Category = "Extended|State Machine")
	APawn* GetOwningPawn() const;

	UFUNCTION(BlueprintPure, Category = "Extended|State Machine")
	AController* GetOwningController() const;

	UFUNCTION(BlueprintPure, Category = "Extended|State Machine")
	AAIController* GetOwningAIController() const;

	/** The actor this transition is about (target, interactor, instigator — machine-agnostic). */
	UFUNCTION(BlueprintPure, Category = "Extended|State Machine")
	AActor* GetContextActor() const;

	UFUNCTION(BlueprintCallable, Category = "Extended|State Machine")
	void SetContextActor(AActor* InContextActor);

	UFUNCTION(BlueprintPure, Category = "Extended|State Machine")
	bool HasAuthority() const;

	/**
	 * Sibling instance lookup. This is the parameter-passing channel: configure the next state's
	 * properties before pushing it. Only possible because instances are registered once and reused.
	 */
	UFUNCTION(BlueprintPure, Category = "Extended|State Machine", meta = (DeterminesOutputType = "StateClass"))
	UEGState* GetSiblingState(TSubclassOf<UEGState> StateClass) const;

	template <typename StateType>
	StateType* GetSiblingState() const
	{
		return Cast<StateType>(GetSiblingState(StateType::StaticClass()));
	}

	// -----------------------------------------------------------------
	// Timers — cleared automatically when the state exits
	// -----------------------------------------------------------------

	/**
	 * Sets a timer scoped to this activation of the state. Every handle created here is cleared in
	 * the exit path before OnExit() returns, so a timer can never fire against an inactive state.
	 * Use the world timer manager directly for anything that must survive a pause or exit.
	 */
	FTimerHandle SetStateTimer(float Delay, FTimerDelegate Delegate, bool bLoop = false);

	template <typename UserClass>
	FTimerHandle SetStateTimer(float Delay, UserClass* Object, typename FTimerDelegate::TMethodPtr<UserClass> Method, bool bLoop = false)
	{
		return SetStateTimer(Delay, FTimerDelegate::CreateUObject(Object, Method), bLoop);
	}

	void ClearStateTimer(FTimerHandle& Handle);

	// -----------------------------------------------------------------
	// Debug
	// -----------------------------------------------------------------

	/** Appends a line to the owning machine's debug ring buffer, prefixed with this state's name. */
	UFUNCTION(BlueprintCallable, Category = "Extended|State Machine|Debug")
	void DebugLog(const FString& Message) const;

	/**
	 * Internal phase of this state, shown in the machine's debug overlay. The stack view can show
	 * which state is running but never what it is doing — override this when the state is large.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Extended|State Machine|Debug")
	FString GetStateDebugString() const;
	virtual FString GetStateDebugString_Implementation() const { return FString(); }

	virtual UWorld* GetWorld() const override;

protected:
	/** Being given at runtime immediately force-pushes this state (interrupt idiom). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Extended|State Machine|Transitions")
	bool bPushWhenGiven = false;

	/** Unregisters itself once it exits (transient one-shot states). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Extended|State Machine|Transitions")
	bool bRemoveWhenExited = false;

	/** While active, nothing may force-push over this state. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Extended|State Machine|Transitions")
	bool bBlocksInterrupts = false;

	/**
	 * Client mirrors replay this state's lifecycle locally (cosmetic states).
	 * Off by default: state code is authority-only unless it opts in.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Extended|State Machine|Network")
	bool bRunOnSimulatedProxy = false;

	/** Mirrored states also tick. Requires bRunOnSimulatedProxy. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Extended|State Machine|Network")
	bool bTickOnSimulatedProxy = false;

	/**
	 * Replicate this state's own properties. The machine registers it as a replicated subobject,
	 * and the subclass declares the properties in GetLifetimeReplicatedProps as usual.
	 *
	 * Only needed for per-run data on a state given at runtime: states authored in
	 * StateDefinitions already have identical values on both sides, because both sides build them
	 * from the same archetype.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Extended|State Machine|Network")
	bool bReplicateState = false;

	/** Seconds between OnTick calls. 0 = every frame. OnTick receives the accumulated delta. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Extended|State Machine", meta = (ClampMin = "0.0"))
	float TickInterval = 0.0f;

	/**
	 * Keep ticking while paused, instead of only while active.
	 *
	 * Off by default — normally the stack means exactly one state runs at a time. A brain that has
	 * to preempt its own children (react to a stun while an attack is playing) turns it on.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Extended|State Machine")
	bool bTickWhilePaused = false;

	/** Optional identity tag published by the machine while this state is active. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Extended|State Machine")
	FGameplayTag StateTag;

private:
	friend class UEGStateMachineComponent;

	/**
	 * Component-side entry points. These do the bookkeeping the state must not be able to skip
	 * (active flag, tick accumulator, timer teardown) and then raise the matching event, so a
	 * subclass that forgets to call Super:: cannot leak.
	 */
	void NotifyGiven();
	void NotifyEnter();
	void NotifyTick(float DeltaTime);
	void NotifyPause();
	void NotifyResume();
	void NotifyExit();
	void NotifyRemoved();

	void ClearAllStateTimers();

	/**
	 * Deliberately NOT a UPROPERTY.
	 *
	 * States are instanced subobjects of the machine's StateDefinitions, so a reflected pointer
	 * back to the machine closes a cycle in the property graph: the details panel walks
	 * component -> state -> component -> state and recurses until the stack overflows, and
	 * Blueprint reinstancing follows the same edge and remaps subobjects it should not touch.
	 *
	 * Nothing needs serializing here — the machine is this state's outer, and registration sets
	 * the pointer at runtime.
	 */
	TWeakObjectPtr<UEGStateMachineComponent> OwningMachine;

	TArray<FTimerHandle> ActiveStateTimers;

	float TickAccumulator = 0.0f;
	bool bIsActive = false;
};
