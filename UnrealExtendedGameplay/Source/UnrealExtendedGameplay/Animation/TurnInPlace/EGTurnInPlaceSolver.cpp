// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGTurnInPlaceSolver.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"

namespace
{
	/** The rotation's heading in the plane normal to Up; stable even when looking almost along Up. */
	FVector HeadingInPlane(const FRotator& Rotation, const FVector& Up)
	{
		const FQuat Quat = Rotation.Quaternion();
		const FVector Forward = FVector::VectorPlaneProject(Quat.GetForwardVector(), Up);
		const FVector Right = FVector::VectorPlaneProject(Quat.GetRightVector(), Up);

		// Forward collapses when looking along Up; right stays in the plane then. Use whichever
		// projects more strongly (both give the same heading for a roll-free rotation).
		if (Forward.SizeSquared() >= Right.SizeSquared())
		{
			return Forward.GetSafeNormal();
		}
		return FVector::CrossProduct(Right, Up).GetSafeNormal();
	}

	constexpr float CurveThreshold = 0.95f;
}

// ---------------------------------------------------------------------------------------------
// Rotation
// ---------------------------------------------------------------------------------------------

float FEGTurnInPlaceSolver::YawDeltaAroundUp(const FRotator& From, const FRotator& To, const FVector& Up)
{
	const bool bWorldUp = Up.Equals(FVector::UpVector);
	if (bWorldUp && FMath::IsNearlyZero(From.Roll) && FMath::IsNearlyZero(To.Roll))
	{
		// Forward's horizontal heading is exactly the yaw when there is no roll, whatever the pitch.
		return static_cast<float>(FRotator::NormalizeAxis(To.Yaw - From.Yaw));
	}

	const FVector FromHeading = HeadingInPlane(From, Up);
	const FVector ToHeading = HeadingInPlane(To, Up);
	if (FromHeading.IsNearlyZero() || ToHeading.IsNearlyZero())
	{
		return static_cast<float>(FRotator::NormalizeAxis(To.Yaw - From.Yaw));
	}

	const double Cross = FVector::DotProduct(FVector::CrossProduct(FromHeading, ToHeading), Up);
	const double Dot = FVector::DotProduct(FromHeading, ToHeading);
	return static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(Cross, Dot)));
}

float FEGTurnInPlaceSolver::UpdateOffset(const FOffsetInputs& Inputs, FEGTurnInPlaceMoveState& State)
{
	if (Inputs.State == EEGTurnInPlaceState::Locked)
	{
		// A proxy's offset comes from the server, which only resends on change: never discard it
		// because of a lock seen only locally.
		if (!Inputs.bSimulatedProxy)
		{
			State = FEGTurnInPlaceMoveState();
		}
		return 0.0f;
	}

	if (Inputs.State == EEGTurnInPlaceState::Paused)
	{
		// As if turn in place did not exist: nothing builds up, the actor turns all the way.
		if (Inputs.bSimulatedProxy)
		{
			return 0.0f;
		}
		const float FullOffset = YawDeltaAroundUp(Inputs.Current, Inputs.Desired, Inputs.Up);
		State = FEGTurnInPlaceMoveState();
		return FullOffset;
	}

	float FullOffset = 0.0f;
	if (!Inputs.bSimulatedProxy)
	{
		// Measured afresh every step (not accumulated), so velocity and other rotation is accounted for.
		FullOffset = YawDeltaAroundUp(Inputs.Current, Inputs.Desired, Inputs.Up);
		State.InterpOutAlpha = 0.0f;
		State.TurnOffset = FullOffset;
	}

	// Let the turn animation consume the offset.
	const float LastCurveValue = State.CurveValue;
	if (!Inputs.Curves.IsTurning())
	{
		State.CurveValue = 0.0f;
		State.bLastCurveValid = false;
	}
	else
	{
		const float NewCurveValue = Inputs.Curves.RemainingTurnYaw * Inputs.Curves.TurnYawWeight;
		const bool bHadValue = State.bLastCurveValid;
		State.bLastCurveValid = true;

		// The first frame with weight only records the value: a delta from 0 or a stale value jumps.
		if (bHadValue)
		{
			// A sign flip is a direction change; applying it would snap. 0 counts as either side.
			const bool bSameDirection = LastCurveValue == 0.0f || NewCurveValue == 0.0f || FMath::Sign(NewCurveValue) == FMath::Sign(LastCurveValue);
			if (bSameDirection)
			{
				// Past 180 the yaw would wrap and snap the other way: hold until the animation brings it back.
				const float NewOffset = State.TurnOffset + (NewCurveValue - LastCurveValue);
				if (FMath::Abs(NewOffset) <= 180.0f)
				{
					State.TurnOffset = NewOffset;
				}
			}
		}
		State.CurveValue = NewCurveValue;
	}

	// Never fall further behind than the max angle: the character is dragged along instead.
	if (Inputs.MaxTurnAngle > 0.0f && FMath::Abs(State.TurnOffset) > Inputs.MaxTurnAngle)
	{
		State.TurnOffset = FMath::Clamp(State.TurnOffset, -Inputs.MaxTurnAngle, Inputs.MaxTurnAngle);
	}

	if (Inputs.bSimulatedProxy)
	{
		return 0.0f;
	}

	// The actor closes the gap between the full offset and what remains.
	return static_cast<float>(FRotator::NormalizeAxis(FullOffset - State.TurnOffset));
}

// ---------------------------------------------------------------------------------------------
// Selection
// ---------------------------------------------------------------------------------------------

int32 FEGTurnInPlaceSolver::SelectStep(const FEGTurnInPlaceEntry& Entry, float Offset, bool& bOutTurnRight)
{
	bOutTurnRight = Offset > 0.0f;
	if (Entry.StepSizes.Num() == 0)
	{
		return 0;
	}

	const float StepAngle = FMath::Abs(Offset) + Entry.SelectOffset;
	int32 Step = 0;

	switch (Entry.SelectMode)
	{
	case EEGTurnSelectMode::Nearest:
	{
		float BestDiff = TNumericLimits<float>::Max();
		for (int32 Index = 0; Index < Entry.StepSizes.Num(); ++Index)
		{
			const float Diff = FMath::Abs(StepAngle - static_cast<float>(Entry.StepSizes[Index]));
			if (Diff < BestDiff)
			{
				BestDiff = Diff;
				Step = Index;
			}
		}
		break;
	}
	case EEGTurnSelectMode::Greater:
	default:
		for (int32 Index = 0; Index < Entry.StepSizes.Num(); ++Index)
		{
			if (FMath::FloorToInt(StepAngle) >= Entry.StepSizes[Index])
			{
				Step = Index;
			}
		}
		break;
	}
	return Step;
}

float FEGTurnInPlaceSolver::ComputePlayRate(const FEGTurnInPlaceEntry& Entry, float Offset, const FEGTurnAngles& Angles, bool bHasAngles,
	bool bIsTurning, bool bAnimTurningRight, bool bForceMaxAngle, bool& bOutReachedMaxAngle)
{
	bOutReachedMaxAngle = bForceMaxAngle
		|| (bHasAngles && Angles.MaxTurnAngle > 0.0f && FMath::Abs(Offset) >= Angles.MaxTurnAngle - 0.01f);
	const float MaxAngleRate = bOutReachedMaxAngle ? Entry.PlayRateAtMaxAngle : 1.0f;

	// The playing turn goes one way while the offset asks for the other: finish it quickly.
	const bool bDirectionChange = bIsTurning && !FMath::IsNearlyZero(Offset, 1.0f) && (Offset > 0.0f) != bAnimTurningRight;
	const float DirectionRate = bDirectionChange ? Entry.PlayRateOnDirectionChange : 1.0f;

	return FMath::Max3(MaxAngleRate, DirectionRate, 1.0f);
}

// ---------------------------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------------------------

EEGTurnInPlaceState FEGTurnInPlaceSolver::ResolveState(const FStateInputs& In, EEGTurnStateReason& OutReason)
{
	switch (In.ConsoleOverride)
	{
	case 1: OutReason = EEGTurnStateReason::ConsoleOverride; return EEGTurnInPlaceState::Enabled;
	case 2: OutReason = EEGTurnStateReason::ConsoleOverride; return EEGTurnInPlaceState::Locked;
	case 3: OutReason = EEGTurnStateReason::ConsoleOverride; return EEGTurnInPlaceState::Paused;
	default: break;
	}

	if (In.bPauseRequested) { OutReason = EEGTurnStateReason::PauseRequest; return EEGTurnInPlaceState::Paused; }
	if (In.bLockRequested) { OutReason = EEGTurnStateReason::LockRequest; return EEGTurnInPlaceState::Locked; }

	// One-sided thresholds: inertialization can overshoot 1, and a band around 1 would flicker.
	if (In.Curves.PauseTurnInPlace >= CurveThreshold) { OutReason = EEGTurnStateReason::PauseCurve; return EEGTurnInPlaceState::Paused; }
	if (In.Curves.LockTurnInPlace >= CurveThreshold) { OutReason = EEGTurnStateReason::LockCurve; return EEGTurnInPlaceState::Locked; }

	if (In.bPauseWindowOpen) { OutReason = EEGTurnStateReason::PauseWindow; return EEGTurnInPlaceState::Paused; }
	if (In.bLockWindowOpen) { OutReason = EEGTurnStateReason::LockWindow; return EEGTurnInPlaceState::Locked; }

	switch (In.MontageOverride)
	{
	case EEGTurnInPlaceOverride::ForceEnabled: OutReason = EEGTurnStateReason::Montage; return EEGTurnInPlaceState::Enabled;
	case EEGTurnInPlaceOverride::ForceLocked: OutReason = EEGTurnStateReason::Montage; return EEGTurnInPlaceState::Locked;
	case EEGTurnInPlaceOverride::ForcePaused: OutReason = EEGTurnStateReason::Montage; return EEGTurnInPlaceState::Paused;
	default: break;
	}

	if (In.bHasRootMotionSources) { OutReason = EEGTurnStateReason::RootMotionSource; return EEGTurnInPlaceState::Paused; }
	if (In.bRotationLockedByModifier) { OutReason = EEGTurnStateReason::MovementModifier; return EEGTurnInPlaceState::Locked; }

	OutReason = EEGTurnStateReason::Entry;
	return In.EntryState;
}

EEGTurnInPlaceOverride FEGTurnInPlaceSolver::EvaluateRootMotionMontage(const FEGTurnInPlaceMontageRules& Rules, const UAnimMontage* Montage)
{
	if (!Montage)
	{
		return EEGTurnInPlaceOverride::Default;
	}

	if (const EEGTurnInPlaceOverride* Override = Rules.Overrides.Find(const_cast<UAnimMontage*>(Montage)))
	{
		if (*Override != EEGTurnInPlaceOverride::Default)
		{
			return *Override;
		}
	}

	if (Rules.IgnoredMontages.Contains(Montage))
	{
		return EEGTurnInPlaceOverride::Default;
	}
	if (Rules.bIgnoreAdditive && Montage->IsValidAdditive())
	{
		return EEGTurnInPlaceOverride::Default;
	}
	for (const FName& Slot : Rules.IgnoredSlots)
	{
		if (Montage->IsValidSlot(Slot))
		{
			return EEGTurnInPlaceOverride::Default;
		}
	}

	// A root-motion montage drives rotation itself.
	return EEGTurnInPlaceOverride::ForcePaused;
}

// ---------------------------------------------------------------------------------------------
// Anim graph
// ---------------------------------------------------------------------------------------------

void FEGTurnInPlaceSolver::ComputeGraphOutput(const FEGTurnInPlaceGraphInput& Input, bool bTurnedThisEntry, bool bReachedAnimEnd, FEGTurnInPlaceGraphOutput& Output)
{
	if (!Input.bValid)
	{
		Output = FEGTurnInPlaceGraphOutput();
		return;
	}

	Output.TurnOffset = Input.TurnOffset;
	Output.bWantsToTurn = Input.bWantsToTurn;
	Output.bAbortTurn = Input.bAbortTurn;

	// Recovery needs the turn to have been seen turning in this entry (the first frame still carries
	// curves from the previous state), or the turn to have played out (a weight that never
	// registered would otherwise park the character at the end of the turn for good).
	Output.bWantsRecovery = (bTurnedThisEntry || bReachedAnimEnd) && !Input.bIsTurning && !Input.bAbortTurn;

	Output.bStartToCycleFromTurn = Input.bIsStrafing && Input.bHasAngles && FMath::Abs(Input.TurnOffset) > Input.Angles.MinTurnAngle;
	Output.bStopToIdleForTurn = Input.bIsTurning || Input.bWantsToTurn;
	Output.bPlayTurnAnim = Output.bWantsToTurn && !Input.bPseudo;
}

double FEGTurnInPlaceSolver::AdvanceAnimTime(const UAnimSequence* Sequence, double Time, float DeltaTime, float PlayRate)
{
	if (!Sequence)
	{
		return Time;
	}
	const double Next = Time + static_cast<double>(DeltaTime) * PlayRate * Sequence->RateScale;
	return FMath::Clamp(Next, 0.0, static_cast<double>(Sequence->GetPlayLength()));
}

void FEGTurnInPlaceSolver::UpdateNodePlayRate(FEGTurnInPlaceNodeState& Node, const FEGTurnInPlaceGraphInput& Input)
{
	const FEGTurnInPlaceEntry& Entry = Input.GetEntry();
	bool bReachedMax = false;
	Node.PlayRate = ComputePlayRate(Entry, Input.TurnOffset, Input.Angles, Input.bHasAngles, Input.bIsTurning, Node.bTurningRight, Node.bReachedMaxAngle, bReachedMax);
	Node.bReachedMaxAngle = Entry.bKeepMaxAnglePlayRate && bReachedMax;
}

UAnimSequence* FEGTurnInPlaceSolver::GetTurnAnimation(const FEGTurnInPlaceEntry& Entry, const FEGTurnInPlaceNodeState& Node, bool bRecovery)
{
	return Entry.GetTurnSequence(Node.StepIndex, bRecovery ? Node.bRecoveryTurningRight : Node.bTurningRight);
}

void FEGTurnInPlaceSolver::AdvancePseudoState(EEGTurnPseudoState& State, FEGTurnInPlaceNodeState& Node, const FEGTurnInPlaceGraphInput& Input,
	const FEGTurnInPlaceGraphOutput& Output, float DeltaTime)
{
	const FEGTurnInPlaceEntry& Entry = Input.GetEntry();

	switch (State)
	{
	case EEGTurnPseudoState::Idle:
		if (Output.bWantsToTurn)
		{
			State = EEGTurnPseudoState::Turn;
			Node.StepIndex = Input.StepIndex;
			Node.bTurningRight = Input.bTurnRight;
			Node.AnimTime = 0.0;
			Node.bReachedMaxAngle = false;
			Node.bReachedAnimEnd = false;
			UpdateNodePlayRate(Node, Input);
		}
		break;

	case EEGTurnPseudoState::Turn:
		if (Output.bAbortTurn)
		{
			State = EEGTurnPseudoState::Idle;
			Node.PlayRate = 1.0f;
			Node.bReachedMaxAngle = false;
		}
		else if (Output.bWantsRecovery)
		{
			// The recovery continues from where the turn is (as UpdateRecoveryAnim does in the graph).
			State = EEGTurnPseudoState::Recovery;
			Node.bRecoveryTurningRight = Node.bTurningRight;
			Node.bReachedAnimEnd = false;
			Node.bRecoveryFinished = false;
		}
		else
		{
			const UAnimSequence* Sequence = GetTurnAnimation(Entry, Node, false);
			Node.AnimTime = AdvanceAnimTime(Sequence, Node.AnimTime, DeltaTime, Node.PlayRate);
			Node.bReachedAnimEnd = !Sequence || Node.AnimTime >= Sequence->GetPlayLength() - UE_KINDA_SMALL_NUMBER;
			UpdateNodePlayRate(Node, Input);
		}
		break;

	case EEGTurnPseudoState::Recovery:
	{
		const UAnimSequence* Sequence = GetTurnAnimation(Entry, Node, true);
		Node.AnimTime = AdvanceAnimTime(Sequence, Node.AnimTime, DeltaTime, 1.0f);
		if (Output.bWantsToTurn)
		{
			// Recovery -> Turn, as in the graph.
			State = EEGTurnPseudoState::Turn;
			Node.StepIndex = Input.StepIndex;
			Node.bTurningRight = Input.bTurnRight;
			Node.AnimTime = 0.0;
			Node.bReachedMaxAngle = false;
			Node.bReachedAnimEnd = false;
			UpdateNodePlayRate(Node, Input);
		}
		else if (!Sequence || Node.AnimTime >= Sequence->GetPlayLength() - UE_KINDA_SMALL_NUMBER)
		{
			State = EEGTurnPseudoState::Idle;
			Node.PlayRate = 1.0f;
			Node.bReachedMaxAngle = false;
			Node.bReachedAnimEnd = false;
			Node.bRecoveryFinished = true;
		}
		break;
	}
	}
}

// ---------------------------------------------------------------------------------------------
// Last input direction
// ---------------------------------------------------------------------------------------------

void FEGTurnInPlaceSolver::UpdateLastInputDirection(FVector& InOutDirection, const FLastInputInputs& In)
{
	// Strafing, or root motion driving the body: the character's own facing is the intent.
	if (!In.bOrientToMovement || In.bRootMotion)
	{
		InOutDirection = In.Forward;
		return;
	}

	// In the air only the part across gravity counts, so jumping does not change the direction.
	const FVector GroundVelocity = In.bMovingOnGround ? In.Velocity : FVector::VectorPlaneProject(In.Velocity, In.Up);
	const bool bFromInput = In.AnalogInput > AnalogInputThreshold && !In.Acceleration.IsNearlyZero();
	const bool bFromVelocity = In.TimeSinceRootMotion >= RootMotionGraceSeconds && !GroundVelocity.IsNearlyZero(In.MaxSpeed * 0.05f);

	if (bFromInput)
	{
		InOutDirection = In.Acceleration.GetSafeNormal();
	}
	else if (bFromVelocity)
	{
		InOutDirection = GroundVelocity.GetSafeNormal();
	}
	else if (In.bBotControlled)
	{
		InOutDirection = In.ControlForward;
	}
}
