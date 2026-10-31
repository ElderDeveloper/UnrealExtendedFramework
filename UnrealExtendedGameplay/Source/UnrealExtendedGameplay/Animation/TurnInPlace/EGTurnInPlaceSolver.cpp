// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGTurnInPlaceSolver.h"

#include "Algo/BinarySearch.h"
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

FVector FEGTurnInPlaceSolver::HeadingAroundUp(const FRotator& Rotation, const FVector& Up)
{
	return HeadingInPlane(Rotation, Up);
}

FRotator FEGTurnInPlaceSolver::MakeUprightRotation(const FRotator& Base, float AddYaw, const FVector& Up)
{
	if (Up.Equals(FVector::UpVector))
	{
		return FRotator(0.0, FRotator::NormalizeAxis(Base.Yaw + AddYaw), 0.0);
	}

	FVector Heading = HeadingInPlane(Base, Up);
	if (Heading.IsNearlyZero())
	{
		// A degenerate base has no heading: any direction across Up will do.
		Heading = FVector::VectorPlaneProject(FVector::ForwardVector, Up).GetSafeNormal();
		if (Heading.IsNearlyZero())
		{
			Heading = FVector::VectorPlaneProject(FVector::RightVector, Up).GetSafeNormal();
		}
	}

	// A positive angle about Up is a positive (rightward) yaw, as under normal gravity.
	Heading = FQuat(Up, FMath::DegreesToRadians(static_cast<double>(AddYaw))).RotateVector(Heading);
	return FRotationMatrix::MakeFromZX(Up, Heading).Rotator();
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
	Output.bWantsToTurn = Input.bWantsToTurn || Input.bWantsSettle;
	Output.bAbortTurn = Input.bAbortTurn;

	// Recovery needs the turn to have been seen turning in this entry (the first frame still carries
	// curves from the previous state), or the turn to have played out (a weight that never
	// registered would otherwise park the character at the end of the turn for good).
	Output.bWantsRecovery = (bTurnedThisEntry || bReachedAnimEnd) && !Input.bIsTurning && !Input.bAbortTurn;

	Output.bStartToCycleFromTurn = Input.bIsStrafing && Input.bHasAngles && FMath::Abs(Input.TurnOffset) > Input.Angles.MinTurnAngle;
	Output.bStopToIdleForTurn = Input.bIsTurning || Output.bWantsToTurn;
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
	// A procedural turn plays no clip, even where its step has one (a settle turn).
	if (Node.bProcedural)
	{
		return nullptr;
	}
	return Entry.GetTurnSequence(Node.StepIndex, bRecovery ? Node.bRecoveryTurningRight : Node.bTurningRight);
}

void FEGTurnInPlaceSolver::BeginTurn(FEGTurnInPlaceNodeState& Node, const FEGTurnInPlaceGraphInput& Input)
{
	const FEGTurnInPlaceEntry& Entry = Input.GetEntry();
	Node.StepIndex = Input.StepIndex;
	Node.bTurningRight = Input.bTurnRight;
	Node.AnimTime = 0.0;
	Node.bReachedMaxAngle = false;
	Node.bReachedAnimEnd = false;
	Node.bRecoveryFinished = false;

	// A settle turn always closes the offset procedurally; a regular turn when its clip is missing.
	const bool bSettle = Input.bWantsSettle && !Input.bWantsToTurn;
	const bool bHasClip = Entry.GetTurnSequence(Node.StepIndex, Node.bTurningRight) != nullptr;
	Node.bProcedural = bSettle || (!bHasClip && Entry.Procedural.bTurnWithoutAnimation);
	Node.ProceduralYaw = Node.bProcedural ? Input.TurnOffset : 0.0f;

	UpdateNodePlayRate(Node, Input);
}

double FEGTurnInPlaceSolver::GetTurnLength(const FEGTurnInPlaceEntry& Entry, const FEGTurnInPlaceNodeState& Node, bool bRecovery)
{
	if (Node.bProcedural)
	{
		return GetProceduralDuration(Node.ProceduralYaw, Entry.Procedural);
	}
	const UAnimSequence* const Sequence = GetTurnAnimation(Entry, Node, bRecovery);
	return Sequence ? static_cast<double>(Sequence->GetPlayLength()) : 0.0;
}

bool FEGTurnInPlaceSolver::AdvanceTurnTime(const FEGTurnInPlaceEntry& Entry, FEGTurnInPlaceNodeState& Node, float DeltaTime, float PlayRate, bool bRecovery)
{
	const double Length = GetTurnLength(Entry, Node, bRecovery);
	if (Length <= 0.0)
	{
		// Nothing to play: played out.
		return true;
	}

	const UAnimSequence* const Sequence = GetTurnAnimation(Entry, Node, bRecovery);
	const double RateScale = Sequence ? FMath::Max(static_cast<double>(Sequence->RateScale), 0.0) : 1.0;
	Node.AnimTime = FMath::Clamp(Node.AnimTime + static_cast<double>(DeltaTime) * PlayRate * RateScale, 0.0, Length);
	return Node.AnimTime >= Length - UE_KINDA_SMALL_NUMBER;
}

FEGTurnInPlaceCurves FEGTurnInPlaceSolver::EvaluateTurnCurves(const FEGTurnInPlaceEntry& Entry, const FEGTurnInPlaceNodeState& Node, bool bRecovery, const FEGTurnInPlaceCurveNames& Names)
{
	if (Node.bProcedural)
	{
		return EvaluateProceduralTurn(Node.ProceduralYaw, Node.AnimTime, Entry.Procedural);
	}

	FEGTurnInPlaceCurves Curves;
	if (const UAnimSequenceBase* const Sequence = GetTurnAnimation(Entry, Node, bRecovery))
	{
		// Through the base class: UAnimSequence hides the overloads it does not override.
		const FAnimExtractContext Context(Node.AnimTime);
		Curves.RemainingTurnYaw = Sequence->EvaluateCurveData(Names.RemainingTurnYaw, Context, false);
		Curves.TurnYawWeight = Sequence->EvaluateCurveData(Names.TurnYawWeight, Context, false);
		Curves.PauseTurnInPlace = Sequence->EvaluateCurveData(Names.PauseTurnInPlace, Context, false);
		Curves.LockTurnInPlace = Sequence->EvaluateCurveData(Names.LockTurnInPlace, Context, false);
	}
	return Curves;
}

// ---------------------------------------------------------------------------------------------
// Procedural turns
// ---------------------------------------------------------------------------------------------

float FEGTurnInPlaceSolver::GetProceduralDuration(float TotalYaw, const FEGTurnInPlaceProcedural& Settings)
{
	return FMath::Max(Settings.MinDuration, FMath::Abs(TotalYaw) / FMath::Max(Settings.TurnRate, 1.0f));
}

FEGTurnInPlaceCurves FEGTurnInPlaceSolver::EvaluateProceduralTurn(float TotalYaw, double Elapsed, const FEGTurnInPlaceProcedural& Settings)
{
	const double Duration = GetProceduralDuration(TotalYaw, Settings);
	const double Alpha = Duration > 0.0 ? FMath::Clamp(Elapsed / Duration, 0.0, 1.0) : 1.0;

	// Smoothstep: no jolt where the capsule starts or stops.
	const double Eased = Alpha * Alpha * (3.0 - 2.0 * Alpha);

	FEGTurnInPlaceCurves Curves;
	Curves.RemainingTurnYaw = static_cast<float>(TotalYaw * (1.0 - Eased));
	Curves.TurnYawWeight = Alpha < 1.0 ? 1.0f : 0.0f;
	return Curves;
}

bool FEGTurnInPlaceSolver::UpdateSettleTimer(float& InOutTimer, const FEGTurnInPlaceProcedural& Settings, float Offset, float MinTurnAngle, bool bCanSettle, float DeltaTime)
{
	const float AbsOffset = FMath::Abs(Offset);
	if (!Settings.bSettle || !bCanSettle || AbsOffset < Settings.SettleAngle || AbsOffset >= MinTurnAngle)
	{
		InOutTimer = 0.0f;
		return false;
	}
	InOutTimer += FMath::Max(DeltaTime, 0.0f);
	return InOutTimer >= Settings.SettleDelay;
}

// ---------------------------------------------------------------------------------------------
// Root yaw tracks
// ---------------------------------------------------------------------------------------------

bool FEGTurnInPlaceSolver::BuildYawTrack(const TArray<double>& Times, const TArray<double>& RawYaws, float FinishedTolerance, float RecoveryBlendTime, FEGTurnYawTrack& OutTrack)
{
	OutTrack = FEGTurnYawTrack();
	const int32 Num = Times.Num();
	if (Num < 2 || RawYaws.Num() != Num || Times.Last() <= 0.0)
	{
		return false;
	}

	OutTrack.Times.Reserve(Num);
	OutTrack.CumulativeYaw.Reserve(Num);

	// Frame-to-frame deltas, so a 180-degree turn is not ambiguous at its ends.
	double Cumulative = 0.0;
	for (int32 Index = 0; Index < Num; ++Index)
	{
		if (Index > 0)
		{
			Cumulative += FRotator::NormalizeAxis(RawYaws[Index] - RawYaws[Index - 1]);
		}
		OutTrack.Times.Add(static_cast<float>(Times[Index]));
		OutTrack.CumulativeYaw.Add(static_cast<float>(Cumulative));
	}

	OutTrack.Length = static_cast<float>(Times.Last());
	OutTrack.TotalYaw = static_cast<float>(Cumulative);

	// Where the remaining yaw is gone, or early enough to blend into recovery. Never the first
	// sample: the weight must be seen at 1 for the turn to register. The last sample always ends it.
	const double LatestEnd = FMath::Max(0.0, Times.Last() - RecoveryBlendTime);
	OutTrack.WeightEndTime = OutTrack.Length;
	for (int32 Index = 1; Index < Num; ++Index)
	{
		const double Remaining = Cumulative - OutTrack.CumulativeYaw[Index];
		if (FMath::Abs(Remaining) <= FinishedTolerance || Times[Index] >= LatestEnd)
		{
			OutTrack.WeightEndTime = static_cast<float>(Times[Index]);
			break;
		}
	}
	return OutTrack.IsValid();
}

FEGTurnInPlaceCurves FEGTurnInPlaceSolver::EvaluateYawTrack(const FEGTurnYawTrack& Track, double Time)
{
	FEGTurnInPlaceCurves Curves;
	if (!Track.IsValid())
	{
		return Curves;
	}

	const float ClampedTime = FMath::Clamp(static_cast<float>(Time), 0.0f, Track.Length);
	const int32 Upper = FMath::Clamp(Algo::UpperBound(Track.Times, ClampedTime), 1, Track.Times.Num() - 1);
	const int32 Lower = Upper - 1;
	const float Span = Track.Times[Upper] - Track.Times[Lower];
	const float Alpha = Span > UE_KINDA_SMALL_NUMBER ? FMath::Clamp((ClampedTime - Track.Times[Lower]) / Span, 0.0f, 1.0f) : 1.0f;
	const float Cumulative = FMath::Lerp(Track.CumulativeYaw[Lower], Track.CumulativeYaw[Upper], Alpha);

	Curves.RemainingTurnYaw = Track.TotalYaw - Cumulative;
	Curves.TurnYawWeight = ClampedTime < Track.WeightEndTime ? 1.0f : 0.0f;
	return Curves;
}

void FEGTurnInPlaceSolver::AdvancePseudoState(EEGTurnPseudoState& State, FEGTurnInPlaceNodeState& Node, const FEGTurnInPlaceGraphInput& Input,
	const FEGTurnInPlaceGraphOutput& Output, float DeltaTime)
{
	const FEGTurnInPlaceEntry& Entry = Input.GetEntry();

	auto EnterIdle = [&Node]()
	{
		Node.PlayRate = 1.0f;
		Node.bReachedMaxAngle = false;
		Node.bReachedAnimEnd = false;
		Node.bProcedural = false;
	};

	switch (State)
	{
	case EEGTurnPseudoState::Idle:
		if (Output.bWantsToTurn)
		{
			State = EEGTurnPseudoState::Turn;
			BeginTurn(Node, Input);
		}
		break;

	case EEGTurnPseudoState::Turn:
		if (Output.bAbortTurn)
		{
			State = EEGTurnPseudoState::Idle;
			EnterIdle();
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
			Node.bReachedAnimEnd = AdvanceTurnTime(Entry, Node, DeltaTime, Node.PlayRate, false);
			UpdateNodePlayRate(Node, Input);
		}
		break;

	case EEGTurnPseudoState::Recovery:
	{
		if (Output.bAbortTurn)
		{
			// The graph's Abort alias covers Recovery too.
			State = EEGTurnPseudoState::Idle;
			EnterIdle();
			break;
		}
		const bool bFinished = AdvanceTurnTime(Entry, Node, DeltaTime, 1.0f, true);
		if (Output.bWantsToTurn)
		{
			// Recovery -> Turn, as in the graph.
			State = EEGTurnPseudoState::Turn;
			BeginTurn(Node, Input);
		}
		else if (bFinished)
		{
			State = EEGTurnPseudoState::Idle;
			EnterIdle();
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
