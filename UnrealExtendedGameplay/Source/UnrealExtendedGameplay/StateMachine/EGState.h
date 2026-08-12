// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "EGState.generated.h"

class UEGStateMachineComponent;
class AActor;
class AController;
class AAIController;

/**
 * UEGState
 *
 * Abstract base for character states. Subclasses override the lifecycle events
 * (OnEnter, OnTick, OnExit, OnPause, OnResume) to implement behavior.
 *
 * States are tickable UObjects owned by the UEGStateMachineComponent.
 * They can be "large" — a single state can manage multi-phase behavior
 * (e.g., a ShopVisit state handles move → wait → serve → leave).
 *
 * States access shared runtime data through their owning actor/component.
 */
UCLASS(Abstract, Blueprintable, BlueprintType, EditInlineNew, DefaultToInstanced)
class UNREALEXTENDEDGAMEPLAY_API UEGState : public UObject
{
	GENERATED_BODY()

protected:
	// -----------------------------------------------------------------
	// Transition Policy
	// -----------------------------------------------------------------

	/** If true, this state cannot exist more than once across the current state and stack. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "State|Transitions")
	bool bSingleStackEntry = false;

	/** If true and this state is registered while the machine is already running, it force-pushes immediately. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "State|Transitions")
	bool bForcePushWhenRegistered = false;

	/** If true, this state unregisters itself from the state machine after it exits. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "State|Transitions")
	bool bUnregisterWhenExited = false;

	/** If true, this state cannot be displaced by ForcePushState while it is active. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "State|Transitions")
	bool bBlocksForcedPushesWhileActive = false;

public:
	// -----------------------------------------------------------------
	// Lifecycle — Override These in Subclasses
	// -----------------------------------------------------------------

	/** Called when this state becomes the active state. */
	UFUNCTION(BlueprintNativeEvent, Category = "EG|AI")
	void OnEnter();

	/** Called every frame while this state is active. */
	UFUNCTION(BlueprintNativeEvent, Category = "EG|AI")
	void OnTick(float DeltaTime);

	/** Called when this state is about to be deactivated (before the next state enters). */
	UFUNCTION(BlueprintNativeEvent, Category = "EG|AI")
	void OnExit();

	/** Called when a higher-priority state is pushed onto the stack (state pauses). */
	UFUNCTION(BlueprintNativeEvent, Category = "EG|AI")
	void OnPause();

	/** Called when the pushed state pops and this state resumes. */
	UFUNCTION(BlueprintNativeEvent, Category = "EG|AI")
	void OnResume();

	/** Whether this state accepts entry for the current request/context. */
	UFUNCTION(BlueprintNativeEvent, Category = "EG|AI")
	bool CanEnterState() const;

	/** Whether this state is the root brain state the state machine should start automatically. */
	virtual bool IsBrainState() const { return false; }

	// -----------------------------------------------------------------
	// State Machine Control
	// -----------------------------------------------------------------

	/** Request the state machine to switch to a different state class (replaces current). */
	UFUNCTION(BlueprintCallable, Category = "EG|AI")
	void RequestStateChangeByClass(TSubclassOf<UEGState> StateClass);

	/** Request the state machine to push an interrupt state class on top of this one. */
	UFUNCTION(BlueprintCallable, Category = "EG|AI")
	void RequestPushStateByClass(TSubclassOf<UEGState> StateClass);

	/** Request the state machine to pop this state (current must be a pushed state). */
	UFUNCTION(BlueprintCallable, Category = "EG|AI")
	void RequestPopState();

	// -----------------------------------------------------------------
	// Accessors — Owning Actor
	// -----------------------------------------------------------------

	/** Get the actor that owns the state machine component this state belongs to. */
	UFUNCTION(BlueprintPure, Category = "EG|AI")
	AActor* GetOwnerActor() const;

	/** Get the owning pawn controller. */
	UFUNCTION(BlueprintPure, Category = "EG|AI")
	AController* GetController() const;

	/** Get the owner's AI controller. */
	UFUNCTION(BlueprintPure, Category = "EG|AI")
	AAIController* GetAIController() const;

	/** Get the owning state machine component. */
	UFUNCTION(BlueprintPure, Category = "EG|AI")
	UEGStateMachineComponent* GetOwningComponent() const { return OwningComponent; }

	/** Get the actor currently associated with this state-transition request. */
	UFUNCTION(BlueprintPure, Category = "EG|AI")
	AActor* GetStateContextActor() const;

	/** Clear the actor currently associated with this state-transition request. */
	UFUNCTION(BlueprintCallable, Category = "EG|AI")
	void ClearStateContextActor();

protected:
	// -----------------------------------------------------------------
	// Debug — per-owner visual debug log
	// -----------------------------------------------------------------

	/**
	 * Emit a debug line to the owning state machine's visual log.
	 * Shows up as world text above the owner when bDebugDraw is on.
	 * Use this instead of UE_LOG for state-internal diagnostics.
	 */
	UFUNCTION(BlueprintCallable, Category = "EG|AI|Debug")
	void DebugLog(const FString& Message) const;

	// -----------------------------------------------------------------
	// World Access (for timers, etc.)
	// -----------------------------------------------------------------

	virtual UWorld* GetWorld() const override;

private:
	friend class UEGStateMachineComponent;

	UPROPERTY()
	TObjectPtr<UEGStateMachineComponent> OwningComponent;
};
