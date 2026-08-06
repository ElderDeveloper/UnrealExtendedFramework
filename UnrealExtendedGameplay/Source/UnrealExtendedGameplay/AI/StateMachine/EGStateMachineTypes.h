// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EGState.h"

#include "EGStateMachineTypes.generated.h"

class AActor;

DECLARE_LOG_CATEGORY_EXTERN(LogEGStateMachine, Log, All);

/**
 * The whole observable machine, replicated as one property.
 *
 * One struct rather than several properties so a client can never observe a torn state — the
 * current state, the stack under it and the context actor always arrive together.
 *
 * Classes rather than object references: one instance per class per component means a class
 * reference is a stable identity that each client resolves against its own registry.
 */
USTRUCT()
struct FEGStateMachineNetState
{
	GENERATED_BODY()

	UPROPERTY()
	TSubclassOf<UEGState> CurrentStateClass;

	/** Paused states, bottom to top, excluding the current one. */
	UPROPERTY()
	TArray<TSubclassOf<UEGState>> StackClasses;

	UPROPERTY()
	TObjectPtr<AActor> ContextActor = nullptr;

	/**
	 * Bumped on every transition. Without it, leaving a state and returning to it (A→B→A) would
	 * produce an identical struct and never replicate as a change.
	 */
	UPROPERTY()
	uint8 Serial = 0;
};

/** Broadcast on every transition, on both authority and clients. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEGStateChanged, TSubclassOf<UEGState>, NewStateClass, TSubclassOf<UEGState>, OldStateClass);

/** Why a requested transition was refused. Surfaced in the debug log and the verbose channel. */
UENUM(BlueprintType)
enum class EEGStateTransitionResult : uint8
{
	Succeeded,
	/** No instance of that class is registered with this machine. */
	NotRegistered,
	/** The class is already current, or already sitting in the stack (one-instance invariant). */
	AlreadyPresent,
	/** The destination state vetoed entry via CanEnterState(). */
	VetoedByState,
	/** The push would exceed MaxStackDepth. */
	StackFull,
	/** The active state has bBlocksInterrupts set and refused the force-push. */
	BlockedByActiveState,
	/** Called without authority, or the machine is not running. */
	NoAuthority,
	/** Null class, or the machine has no owner. */
	Invalid
};

/** Human-readable form of a transition result, for logs and the debug overlay. */
UNREALEXTENDEDGAMEPLAY_API const TCHAR* EGLexToString(EEGStateTransitionResult Result);
