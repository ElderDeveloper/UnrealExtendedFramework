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

	/** The rotation's heading in the plane normal to Up (unit length; zero only for a degenerate rotation). */
	static FVector HeadingAroundUp(const FRotator& Rotation, const FVector& Up = FVector::UpVector);

	/**
	 * Upright to Up, at Base's heading turned by AddYaw about Up. Under normal gravity this is
	 * (0, Base.Yaw + AddYaw, 0), which also clears pitch or roll left from a tilted base.
	 */
	static FRotator MakeUprightRotation(const FRotator& Base, float AddYaw, const FVector& Up = FVector::UpVector);

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

	/** The turn (or recovery) sequence for the node's step and side. Null for a procedural turn. */
	static UAnimSequence* GetTurnAnimation(const FEGTurnInPlaceEntry& Entry, const FEGTurnInPlaceNodeState& Node, bool bRecovery);

	/**
	 * Starts a turn from the input: step, side, time, play rate. A step without a clip for its side
	 * (or a settle turn) becomes procedural when the entry allows it.
	 */
	static void BeginTurn(FEGTurnInPlaceNodeState& Node, const FEGTurnInPlaceGraphInput& Input);

	/** The playing turn's length: its clip's, or the procedural duration. 0 without either. */
	static double GetTurnLength(const FEGTurnInPlaceEntry& Entry, const FEGTurnInPlaceNodeState& Node, bool bRecovery);

	/** Advances Node.AnimTime (clamped to the turn's length). True once it is at the end. */
	static bool AdvanceTurnTime(const FEGTurnInPlaceEntry& Entry, FEGTurnInPlaceNodeState& Node, float DeltaTime, float PlayRate, bool bRecovery);

	/**
	 * The turn curves at the node's time: computed for a procedural turn, read from the clip's
	 * authored curves otherwise. Pause and lock curves are read from the clip as well.
	 */
	static FEGTurnInPlaceCurves EvaluateTurnCurves(const FEGTurnInPlaceEntry& Entry, const FEGTurnInPlaceNodeState& Node, bool bRecovery, const FEGTurnInPlaceCurveNames& Names);

	// ---- Procedural turns ------------------------------------------------------------------------

	/** How long a procedural turn of TotalYaw takes. */
	static float GetProceduralDuration(float TotalYaw, const FEGTurnInPlaceProcedural& Settings);

	/** RemainingTurnYaw eases from TotalYaw to 0 over the duration; TurnYawWeight is 1 until it gets there. */
	static FEGTurnInPlaceCurves EvaluateProceduralTurn(float TotalYaw, double Elapsed, const FEGTurnInPlaceProcedural& Settings);

	/**
	 * Settle timing: counts while a small offset (SettleAngle up to MinTurnAngle) is held with
	 * nothing turning; true once it has been held for SettleDelay.
	 */
	static bool UpdateSettleTimer(float& InOutTimer, const FEGTurnInPlaceProcedural& Settings, float Offset, float MinTurnAngle, bool bCanSettle, float DeltaTime);

	// ---- Root yaw tracks (clips without curves) --------------------------------------------------

	/**
	 * A yaw track from root yaw samples (Times ascending, from 0 to the clip length). Unwraps across
	 * +-180 sample to sample. The weight ends where the remaining yaw is within FinishedTolerance,
	 * or RecoveryBlendTime before the end, never on the first sample: what the curve modifier writes.
	 */
	static bool BuildYawTrack(const TArray<double>& Times, const TArray<double>& RawYaws, float FinishedTolerance, float RecoveryBlendTime, FEGTurnYawTrack& OutTrack);

	/** RemainingTurnYaw and TurnYawWeight from a track at a time. */
	static FEGTurnInPlaceCurves EvaluateYawTrack(const FEGTurnYawTrack& Track, double Time);

	/**
	 * One step of the dedicated server's pseudo states, mirroring the node and the graph recipe:
	 * Idle -> Turn on bWantsToTurn, Turn or Recovery -> Idle on bAbortTurn, Turn -> Recovery on
	 * bWantsRecovery, Recovery -> Turn on bWantsToTurn, Recovery -> Idle when the turn ends.
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
		/** Gravity up; in the air the velocity along it is ignored. */
		FVector Up = FVector::UpVector;
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
