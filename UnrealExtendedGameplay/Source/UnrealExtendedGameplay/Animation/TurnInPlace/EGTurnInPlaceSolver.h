// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EGTurnInPlaceTypes.h"

class UAnimMontage;
class UAnimSequence;

/**
 * FEGTurnInPlaceSolver
 *
 * Every turn-in-place decision as plain functions over plain data: no world, no actors. The
 * component, the anim library and the pseudo server states only gather inputs and apply results,
 * so the behaviour lives here and is covered by automation tests.
 */
struct UNREALEXTENDEDGAMEPLAY_API FEGTurnInPlaceSolver
{
	// ---- Rotation ------------------------------------------------------------------------------

	/**
	 * Yaw from From to To as a heading difference about Up, -180..180. Flat ground with no roll uses
	 * plain yaw subtraction; roll or a custom up axis projects headings, which avoids the Euler
	 * cross-talk that would otherwise start turns on slopes and rotating bases.
	 */
	static float YawDeltaAroundUp(const FRotator& From, const FRotator& To, const FVector& Up = FVector::UpVector);

	struct FOffsetInputs
	{
		FRotator Current = FRotator::ZeroRotator;
		FRotator Desired = FRotator::ZeroRotator;
		FVector Up = FVector::UpVector;
		EEGTurnInPlaceState State = EEGTurnInPlaceState::Enabled;
		FEGTurnInPlaceCurves Curves;
		/** 0: no clamp. */
		float MaxTurnAngle = 0.0f;
		/**
		 * A simulated proxy: the offset is replicated, so it is never reset or re-measured, only
		 * advanced by the local curves, and no rotation is produced.
		 */
		bool bSimulatedProxy = false;
	};

	/**
	 * One movement step while standing. Updates the offset (measure, curve delta, clamp) and returns
	 * the yaw the actor should turn by this step. Locked returns 0 and clears the state (except on
	 * simulated proxies, which keep the replicated offset).
	 */
	static float UpdateOffset(const FOffsetInputs& Inputs, FEGTurnInPlaceMoveState& State);

	// ---- Selection -----------------------------------------------------------------------------

	/** The step index for an offset, and its side. 0 with no steps. */
	static int32 SelectStep(const FEGTurnInPlaceEntry& Entry, float Offset, bool& bOutTurnRight);

	/**
	 * Play rate for the playing turn: faster at the max angle (latched for the rest of the turn when
	 * bForceMaxAngle), faster when the playing turn goes the opposite way to the offset.
	 */
	static float ComputePlayRate(const FEGTurnInPlaceEntry& Entry, float Offset, const FEGTurnAngles& Angles, bool bHasAngles,
		bool bIsTurning, bool bAnimTurningRight, bool bForceMaxAngle, bool& bOutReachedMaxAngle);

	// ---- State ---------------------------------------------------------------------------------

	struct FStateInputs
	{
		EEGTurnInPlaceState EntryState = EEGTurnInPlaceState::Enabled;
		/** eg.TurnInPlace.Override: 0 none, 1 enabled, 2 locked, 3 paused. */
		int32 ConsoleOverride = 0;
		bool bPauseRequested = false;
		bool bLockRequested = false;
		FEGTurnInPlaceCurves Curves;
		bool bPauseWindowOpen = false;
		bool bLockWindowOpen = false;
		/** From EvaluateRootMotionMontage. */
		EEGTurnInPlaceOverride MontageOverride = EEGTurnInPlaceOverride::Default;
		bool bHasRootMotionSources = false;
		bool bRotationLockedByModifier = false;
	};

	/** Enabled / Locked / Paused with the reason, in the documented priority order. */
	static EEGTurnInPlaceState ResolveState(const FStateInputs& Inputs, EEGTurnStateReason& OutReason);

	/**
	 * How a playing networked root-motion montage affects turn in place: a per-montage override,
	 * Default when the montage is ignored (listed, additive, or on an ignored slot), otherwise
	 * ForcePaused. Default for no montage.
	 */
	static EEGTurnInPlaceOverride EvaluateRootMotionMontage(const FEGTurnInPlaceMontageRules& Rules, const UAnimMontage* Montage);

	// ---- Anim graph ----------------------------------------------------------------------------

	/** Transitions and locomotion helpers from an input whose curves are fresh. */
	static void ComputeGraphOutput(const FEGTurnInPlaceGraphInput& Input, bool bTurnedThisEntry, bool bReachedAnimEnd, FEGTurnInPlaceGraphOutput& Output);

	/** Advance a turn's explicit time, clamped to the sequence length. */
	static double AdvanceAnimTime(const UAnimSequence* Sequence, double Time, float DeltaTime, float PlayRate);

	/** Refresh the node's play rate and max-angle latch. */
	static void UpdateNodePlayRate(FEGTurnInPlaceNodeState& Node, const FEGTurnInPlaceGraphInput& Input);

	/** The turn (or recovery) sequence for the node's step and side. */
	static UAnimSequence* GetTurnAnimation(const FEGTurnInPlaceEntry& Entry, const FEGTurnInPlaceNodeState& Node, bool bRecovery);

	/**
	 * One step of the dedicated server's pseudo states, mirroring the graph recipe: Idle -> Turn on
	 * bWantsToTurn, Turn -> Idle on bAbortTurn, Turn -> Recovery on bWantsRecovery, Recovery -> Idle
	 * when the sequence ends (or is missing).
	 */
	static void AdvancePseudoState(EEGTurnPseudoState& State, FEGTurnInPlaceNodeState& Node, const FEGTurnInPlaceGraphInput& Input,
		const FEGTurnInPlaceGraphOutput& Output, float DeltaTime);

	// ---- Last input direction --------------------------------------------------------------------

	struct FLastInputInputs
	{
		bool bOrientToMovement = true;
		bool bRootMotion = false;
		FVector Forward = FVector::ForwardVector;
		FVector Velocity = FVector::ZeroVector;
		FVector Acceleration = FVector::ZeroVector;
		bool bMovingOnGround = true;
		/** Acceleration / max acceleration, 0..1. */
		float AnalogInput = 0.0f;
		float MaxSpeed = 600.0f;
		double TimeSinceRootMotion = 1000.0;
		bool bBotControlled = false;
		FVector ControlForward = FVector::ForwardVector;
	};

	/** The direction OrientToMovement turns towards once input is released. */
	static void UpdateLastInputDirection(FVector& InOutDirection, const FLastInputInputs& Inputs);

	/** Analog input above this counts as intent; below it a released stick bouncing past centre is ignored. */
	static constexpr float AnalogInputThreshold = 0.5f;

	/** After root motion ends, velocity it caused is not taken as input for this long. */
	static constexpr double RootMotionGraceSeconds = 0.25;
};
