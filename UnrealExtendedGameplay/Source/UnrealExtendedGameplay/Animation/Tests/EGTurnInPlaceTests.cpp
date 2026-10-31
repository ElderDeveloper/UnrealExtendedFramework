// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Animation/AnimMontage.h"
#include "Misc/AutomationTest.h"
#include "UnrealExtendedGameplay/Animation/EGAnimationTags.h"
#include "UnrealExtendedGameplay/Animation/TurnInPlace/EGTurnInPlaceSolver.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace EGTurnInPlaceTest
{
	using FSolver = FEGTurnInPlaceSolver;

	static FEGTurnInPlaceCurves Curves(float Remaining, float Weight = 1.0f)
	{
		FEGTurnInPlaceCurves Result;
		Result.RemainingTurnYaw = Remaining;
		Result.TurnYawWeight = Weight;
		return Result;
	}

	static FSolver::FOffsetInputs Step(float CurrentYaw, float DesiredYaw, const FEGTurnInPlaceCurves& InCurves = FEGTurnInPlaceCurves(), float MaxAngle = 0.0f)
	{
		FSolver::FOffsetInputs Inputs;
		Inputs.Current = FRotator(0.0, CurrentYaw, 0.0);
		Inputs.Desired = FRotator(0.0, DesiredYaw, 0.0);
		Inputs.Curves = InCurves;
		Inputs.MaxTurnAngle = MaxAngle;
		return Inputs;
	}
}

// -----------------------------------------------------------------------------
// Yaw measured about the up axis
// -----------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGTurnInPlaceYawDeltaTest,
	"UnrealExtendedGameplay.Animation.TurnInPlace.YawDelta",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGTurnInPlaceYawDeltaTest::RunTest(const FString& Parameters)
{
	using FSolver = FEGTurnInPlaceSolver;

	TestEqual(TEXT("Flat ground"), FSolver::YawDeltaAroundUp(FRotator(0, 0, 0), FRotator(0, 90, 0)), 90.0f, 0.01f);
	TestEqual(TEXT("Wraps the short way across 180"), FSolver::YawDeltaAroundUp(FRotator(0, 170, 0), FRotator(0, -170, 0)), 20.0f, 0.01f);
	TestEqual(TEXT("Pitch does not change the heading"), FSolver::YawDeltaAroundUp(FRotator(45, 0, 0), FRotator(45, 30, 0)), 30.0f, 0.01f);
	TestEqual(TEXT("Roll goes through heading projection"), FSolver::YawDeltaAroundUp(FRotator(0, 0, 30), FRotator(0, 45, 30)), 45.0f, 0.1f);
	TestEqual(TEXT("Upside-down gravity flips the sense"), FSolver::YawDeltaAroundUp(FRotator(0, 0, 0), FRotator(0, 90, 0), -FVector::UpVector), -90.0f, 0.1f);
	TestEqual(TEXT("Looking almost straight up stays stable"), FSolver::YawDeltaAroundUp(FRotator(89.9, 0, 0), FRotator(89.9, 90, 0)), 90.0f, 0.01f);
	return true;
}

// -----------------------------------------------------------------------------
// Offset: holding back, dragging, curve consumption and its guards
// -----------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGTurnInPlaceOffsetTest,
	"UnrealExtendedGameplay.Animation.TurnInPlace.Offset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGTurnInPlaceOffsetTest::RunTest(const FString& Parameters)
{
	using namespace EGTurnInPlaceTest;

	{
		FEGTurnInPlaceMoveState State;
		const float Turn = FSolver::UpdateOffset(Step(0, 30), State);
		TestEqual(TEXT("Standing still the actor is held back"), Turn, 0.0f, 0.01f);
		TestEqual(TEXT("...and the offset is the full difference"), State.TurnOffset, 30.0f, 0.01f);
	}
	{
		FEGTurnInPlaceMoveState State;
		const float Turn = FSolver::UpdateOffset(Step(0, 150, FEGTurnInPlaceCurves(), 135.0f), State);
		TestEqual(TEXT("Past the max angle the offset is clamped"), State.TurnOffset, 135.0f, 0.01f);
		TestEqual(TEXT("...and the actor is dragged by the rest"), Turn, 15.0f, 0.01f);
	}
	{
		FEGTurnInPlaceMoveState State;
		float Current = 0.0f;
		Current += FSolver::UpdateOffset(Step(Current, 90, Curves(90.0f)), State);
		TestEqual(TEXT("The first weighted frame only records the curve"), Current, 0.0f, 0.01f);
		TestTrue(TEXT("...and marks it valid"), State.bLastCurveValid);

		Current += FSolver::UpdateOffset(Step(Current, 90, Curves(60.0f)), State);
		TestEqual(TEXT("The actor turns exactly as far as the animation"), Current, 30.0f, 0.01f);
		TestEqual(TEXT("...and the offset shrinks by it"), State.TurnOffset, 60.0f, 0.01f);

		const float Before = Current;
		Current += FSolver::UpdateOffset(Step(Current, 90, Curves(-10.0f)), State);
		TestEqual(TEXT("A sign flip is skipped instead of snapping"), Current, Before, 0.01f);
	}
	{
		FEGTurnInPlaceMoveState State;
		State.CurveValue = 10.0f;
		State.bLastCurveValid = true;
		FSolver::UpdateOffset(Step(0, 170, Curves(30.0f)), State);
		TestEqual(TEXT("A delta past 180 is held"), State.TurnOffset, 170.0f, 0.01f);
	}
	{
		FEGTurnInPlaceMoveState State;
		FSolver::FOffsetInputs Inputs = Step(0, 40, Curves(20.0f));
		Inputs.State = EEGTurnInPlaceState::Paused;
		const float Turn = FSolver::UpdateOffset(Inputs, State);
		TestEqual(TEXT("Paused turns all the way"), Turn, 40.0f, 0.01f);
		TestEqual(TEXT("...without building up an offset"), State.TurnOffset, 0.0f, 0.01f);
		TestFalse(TEXT("...or remembering curves"), State.bLastCurveValid);
	}
	{
		FEGTurnInPlaceMoveState State;
		State.TurnOffset = 50.0f;
		FSolver::FOffsetInputs Inputs = Step(0, 40);
		Inputs.State = EEGTurnInPlaceState::Locked;
		TestEqual(TEXT("Locked does not rotate"), FSolver::UpdateOffset(Inputs, State), 0.0f, 0.01f);
		TestEqual(TEXT("...and clears the offset"), State.TurnOffset, 0.0f, 0.01f);

		FEGTurnInPlaceMoveState Proxy;
		Proxy.TurnOffset = 50.0f;
		Inputs.bSimulatedProxy = true;
		FSolver::UpdateOffset(Inputs, Proxy);
		TestEqual(TEXT("A simulated proxy keeps its replicated offset when locked locally"), Proxy.TurnOffset, 50.0f, 0.01f);
	}
	{
		FEGTurnInPlaceMoveState Proxy;
		Proxy.TurnOffset = 80.0f;
		Proxy.CurveValue = 80.0f;
		Proxy.bLastCurveValid = true;
		FSolver::FOffsetInputs Inputs = Step(0, 0, Curves(50.0f));
		Inputs.bSimulatedProxy = true;
		TestEqual(TEXT("A simulated proxy produces no rotation"), FSolver::UpdateOffset(Inputs, Proxy), 0.0f, 0.01f);
		TestEqual(TEXT("...but consumes its offset with the local curve between updates"), Proxy.TurnOffset, 50.0f, 0.01f);
	}
	return true;
}

// -----------------------------------------------------------------------------
// Combined moves: restoring the older move's start state keeps the full turn
// -----------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGTurnInPlaceCombinedMoveTest,
	"UnrealExtendedGameplay.Animation.TurnInPlace.CombinedMoves",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGTurnInPlaceCombinedMoveTest::RunTest(const FString& Parameters)
{
	using namespace EGTurnInPlaceTest;

	// Frame 1 records the curve; frames 2 and 3 each turn 30 degrees.
	FEGTurnInPlaceMoveState State;
	float Current = 0.0f;
	Current += FSolver::UpdateOffset(Step(Current, 90, Curves(90.0f)), State);
	const FEGTurnInPlaceMoveState StartOfMove = State;
	const float StartRotation = Current;

	FEGTurnInPlaceMoveState Sequential = State;
	float SequentialYaw = Current;
	SequentialYaw += FSolver::UpdateOffset(Step(SequentialYaw, 90, Curves(60.0f)), Sequential);
	const FEGTurnInPlaceMoveState AfterFrame2 = Sequential;
	SequentialYaw += FSolver::UpdateOffset(Step(SequentialYaw, 90, Curves(30.0f)), Sequential);

	// Frames 2 and 3 combined: rotation rewinds to the move's start, then one step with frame 3's curve.
	FEGTurnInPlaceMoveState Restored = StartOfMove;
	const float CombinedYaw = StartRotation + FSolver::UpdateOffset(Step(StartRotation, 90, Curves(30.0f)), Restored);
	TestEqual(TEXT("Restoring the start state re-simulates the full turn"), CombinedYaw, SequentialYaw, 0.01f);

	FEGTurnInPlaceMoveState NotRestored = AfterFrame2;
	const float BrokenYaw = StartRotation + FSolver::UpdateOffset(Step(StartRotation, 90, Curves(30.0f)), NotRestored);
	TestTrue(TEXT("Without the restore the client turns only part of the way"), BrokenYaw < SequentialYaw - 1.0f);
	return true;
}

// -----------------------------------------------------------------------------
// Step selection and play rate
// -----------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGTurnInPlaceSelectionTest,
	"UnrealExtendedGameplay.Animation.TurnInPlace.Selection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGTurnInPlaceSelectionTest::RunTest(const FString& Parameters)
{
	using FSolver = FEGTurnInPlaceSolver;

	FEGTurnInPlaceEntry Entry;
	Entry.StepSizes = { 60, 90, 180 };
	bool bRight = false;

	Entry.SelectMode = EEGTurnSelectMode::Greater;
	TestEqual(TEXT("Greater: 75 picks 60"), FSolver::SelectStep(Entry, 75.0f, bRight), 0);
	TestTrue(TEXT("Positive offset turns right"), bRight);
	TestEqual(TEXT("Greater: 179 picks 90"), FSolver::SelectStep(Entry, 179.0f, bRight), 1);
	TestEqual(TEXT("Greater: 180 picks 180"), FSolver::SelectStep(Entry, 180.0f, bRight), 2);
	TestEqual(TEXT("Greater: -100 picks 90"), FSolver::SelectStep(Entry, -100.0f, bRight), 1);
	TestFalse(TEXT("Negative offset turns left"), bRight);
	Entry.SelectOffset = 10.0f;
	TestEqual(TEXT("Select offset is added first"), FSolver::SelectStep(Entry, 85.0f, bRight), 1);
	Entry.SelectOffset = 0.0f;

	Entry.SelectMode = EEGTurnSelectMode::Nearest;
	TestEqual(TEXT("Nearest: 140 picks 180"), FSolver::SelectStep(Entry, 140.0f, bRight), 2);
	TestEqual(TEXT("Nearest: 70 picks 60"), FSolver::SelectStep(Entry, 70.0f, bRight), 0);

	FEGTurnInPlaceEntry NoSteps;
	NoSteps.StepSizes.Reset();
	TestEqual(TEXT("No steps is step 0"), FSolver::SelectStep(NoSteps, 120.0f, bRight), 0);

	const FEGTurnAngles Angles(60.0f, 135.0f);
	bool bReachedMax = false;
	TestEqual(TEXT("At the max angle the rate rises"), FSolver::ComputePlayRate(Entry, 135.0f, Angles, true, false, true, false, bReachedMax), Entry.PlayRateAtMaxAngle, 0.001f);
	TestTrue(TEXT("...and reports it for the latch"), bReachedMax);
	TestEqual(TEXT("The latch keeps the rate below the max angle"), FSolver::ComputePlayRate(Entry, 100.0f, Angles, true, false, true, true, bReachedMax), Entry.PlayRateAtMaxAngle, 0.001f);
	TestEqual(TEXT("A turn going the wrong way speeds up"), FSolver::ComputePlayRate(Entry, -80.0f, Angles, true, true, true, false, bReachedMax), Entry.PlayRateOnDirectionChange, 0.001f);
	TestEqual(TEXT("Normal turns play at 1"), FSolver::ComputePlayRate(Entry, 80.0f, Angles, true, true, true, false, bReachedMax), 1.0f, 0.001f);
	return true;
}

// -----------------------------------------------------------------------------
// Enabled state priority and montage rules
// -----------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGTurnInPlaceStateTest,
	"UnrealExtendedGameplay.Animation.TurnInPlace.State",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGTurnInPlaceStateTest::RunTest(const FString& Parameters)
{
	using FSolver = FEGTurnInPlaceSolver;

	FSolver::FStateInputs In;
	In.ConsoleOverride = 2;
	In.bPauseRequested = true;
	In.bLockRequested = true;
	In.Curves.PauseTurnInPlace = 1.0f;
	In.Curves.LockTurnInPlace = 1.0f;
	In.bPauseWindowOpen = true;
	In.bLockWindowOpen = true;
	In.MontageOverride = EEGTurnInPlaceOverride::ForceEnabled;
	In.bHasRootMotionSources = true;
	In.bRotationLockedByModifier = true;
	In.EntryState = EEGTurnInPlaceState::Enabled;

	EEGTurnStateReason Reason;
	auto Expect = [&](const TCHAR* What, EEGTurnInPlaceState State, EEGTurnStateReason ExpectedReason)
	{
		TestEqual(What, FSolver::ResolveState(In, Reason), State);
		TestEqual(*(FString(What) + TEXT(" (reason)")), Reason, ExpectedReason);
	};

	Expect(TEXT("Console override first"), EEGTurnInPlaceState::Locked, EEGTurnStateReason::ConsoleOverride);
	In.ConsoleOverride = 0;
	Expect(TEXT("Then pause requests"), EEGTurnInPlaceState::Paused, EEGTurnStateReason::PauseRequest);
	In.bPauseRequested = false;
	Expect(TEXT("Then lock requests"), EEGTurnInPlaceState::Locked, EEGTurnStateReason::LockRequest);
	In.bLockRequested = false;
	Expect(TEXT("Then the pause curve"), EEGTurnInPlaceState::Paused, EEGTurnStateReason::PauseCurve);
	In.Curves.PauseTurnInPlace = 0.0f;
	Expect(TEXT("Then the lock curve"), EEGTurnInPlaceState::Locked, EEGTurnStateReason::LockCurve);
	In.Curves.LockTurnInPlace = 0.0f;
	Expect(TEXT("Then pause windows"), EEGTurnInPlaceState::Paused, EEGTurnStateReason::PauseWindow);
	In.bPauseWindowOpen = false;
	Expect(TEXT("Then lock windows"), EEGTurnInPlaceState::Locked, EEGTurnStateReason::LockWindow);
	In.bLockWindowOpen = false;
	Expect(TEXT("Then montage overrides"), EEGTurnInPlaceState::Enabled, EEGTurnStateReason::Montage);
	In.MontageOverride = EEGTurnInPlaceOverride::Default;
	Expect(TEXT("Then root motion sources"), EEGTurnInPlaceState::Paused, EEGTurnStateReason::RootMotionSource);
	In.bHasRootMotionSources = false;
	Expect(TEXT("Then a zero rotation-rate modifier"), EEGTurnInPlaceState::Locked, EEGTurnStateReason::MovementModifier);
	In.bRotationLockedByModifier = false;
	Expect(TEXT("Otherwise the entry's state"), EEGTurnInPlaceState::Enabled, EEGTurnStateReason::Entry);

	In.Curves.PauseTurnInPlace = 1.2f;
	Expect(TEXT("Inertialization overshoot still pauses"), EEGTurnInPlaceState::Paused, EEGTurnStateReason::PauseCurve);
	In.Curves.PauseTurnInPlace = 0.9f;
	Expect(TEXT("Below the threshold does not"), EEGTurnInPlaceState::Enabled, EEGTurnStateReason::Entry);

	// Montage rules.
	UAnimMontage* const Attack = NewObject<UAnimMontage>(GetTransientPackage(), NAME_None, RF_Transient);
	UAnimMontage* const Emote = NewObject<UAnimMontage>(GetTransientPackage(), NAME_None, RF_Transient);
	FEGTurnInPlaceMontageRules Rules;
	TestEqual(TEXT("No montage changes nothing"), FSolver::EvaluateRootMotionMontage(Rules, nullptr), EEGTurnInPlaceOverride::Default);
	TestEqual(TEXT("A root-motion montage pauses"), FSolver::EvaluateRootMotionMontage(Rules, Attack), EEGTurnInPlaceOverride::ForcePaused);
	Rules.IgnoredMontages.Add(Emote);
	TestEqual(TEXT("An ignored montage does not"), FSolver::EvaluateRootMotionMontage(Rules, Emote), EEGTurnInPlaceOverride::Default);
	Rules.Overrides.Add(Attack, EEGTurnInPlaceOverride::ForceLocked);
	TestEqual(TEXT("A per-montage override wins"), FSolver::EvaluateRootMotionMontage(Rules, Attack), EEGTurnInPlaceOverride::ForceLocked);
	return true;
}

// -----------------------------------------------------------------------------
// Graph outputs and the dedicated-server pseudo states
// -----------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGTurnInPlaceGraphTest,
	"UnrealExtendedGameplay.Animation.TurnInPlace.Graph",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGTurnInPlaceGraphTest::RunTest(const FString& Parameters)
{
	using FSolver = FEGTurnInPlaceSolver;

	FEGTurnInPlaceGraphInput Input;
	Input.bValid = true;
	Input.State = EEGTurnInPlaceState::Enabled;
	Input.TurnOffset = 90.0f;
	Input.bWantsToTurn = true;
	Input.bHasAngles = true;
	Input.Angles = FEGTurnAngles(60.0f, 135.0f);

	FEGTurnInPlaceGraphOutput Output;
	FSolver::ComputeGraphOutput(Input, false, false, Output);
	TestTrue(TEXT("Wants to turn"), Output.bWantsToTurn);
	TestFalse(TEXT("No recovery on the entry frame (curves still stale)"), Output.bWantsRecovery);

	FSolver::ComputeGraphOutput(Input, true, false, Output);
	TestTrue(TEXT("Recovery once the turn was seen and the weight dropped"), Output.bWantsRecovery);

	Input.bIsTurning = true;
	FSolver::ComputeGraphOutput(Input, true, false, Output);
	TestFalse(TEXT("No recovery while still turning"), Output.bWantsRecovery);
	Input.bIsTurning = false;

	FSolver::ComputeGraphOutput(Input, false, true, Output);
	TestTrue(TEXT("A turn that played out recovers even if its weight never registered"), Output.bWantsRecovery);

	Input.bAbortTurn = true;
	FSolver::ComputeGraphOutput(Input, true, true, Output);
	TestFalse(TEXT("Abort beats recovery"), Output.bWantsRecovery);
	TestTrue(TEXT("...and is reported"), Output.bAbortTurn);
	Input.bAbortTurn = false;

	Input.bIsStrafing = true;
	FSolver::ComputeGraphOutput(Input, false, false, Output);
	TestTrue(TEXT("Strafing past the min angle skips Start"), Output.bStartToCycleFromTurn);

	Input.bPseudo = true;
	FSolver::ComputeGraphOutput(Input, false, false, Output);
	TestFalse(TEXT("Pseudo mode does not play turn animations"), Output.bPlayTurnAnim);

	// Pseudo states, with no sequences (empty entry): the turn is procedural and plays out.
	EEGTurnPseudoState State = EEGTurnPseudoState::Idle;
	FEGTurnInPlaceNodeState Node;
	FSolver::AdvancePseudoState(State, Node, Input, Output, 0.1f);
	TestEqual(TEXT("Idle -> Turn"), State, EEGTurnPseudoState::Turn);
	TestTrue(TEXT("A missing clip turns procedurally"), Node.bProcedural);
	TestEqual(TEXT("...closing the offset"), Node.ProceduralYaw, 90.0f);

	FSolver::AdvancePseudoState(State, Node, Input, Output, 0.1f);
	TestFalse(TEXT("A procedural turn takes its duration"), Node.bReachedAnimEnd);
	for (int32 Frame = 0; Frame < 20 && !Node.bReachedAnimEnd; ++Frame)
	{
		FSolver::AdvancePseudoState(State, Node, Input, Output, 0.1f);
	}
	TestTrue(TEXT("...then has played out"), Node.bReachedAnimEnd);

	FEGTurnInPlaceGraphOutput Recover;
	Recover.bWantsRecovery = true;
	FSolver::AdvancePseudoState(State, Node, Input, Recover, 0.1f);
	TestEqual(TEXT("Turn -> Recovery"), State, EEGTurnPseudoState::Recovery);
	FSolver::AdvancePseudoState(State, Node, Input, FEGTurnInPlaceGraphOutput(), 0.1f);
	TestEqual(TEXT("Recovery -> Idle when the sequence ends"), State, EEGTurnPseudoState::Idle);

	State = EEGTurnPseudoState::Turn;
	FEGTurnInPlaceGraphOutput Abort;
	Abort.bAbortTurn = true;
	FSolver::AdvancePseudoState(State, Node, Input, Abort, 0.1f);
	TestEqual(TEXT("Abort -> Idle"), State, EEGTurnPseudoState::Idle);
	return true;
}

// -----------------------------------------------------------------------------
// Upright rotation, procedural turns, settle, root yaw tracks
// -----------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGTurnInPlaceProceduralTest,
	"UnrealExtendedGameplay.Animation.TurnInPlace.Procedural",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGTurnInPlaceProceduralTest::RunTest(const FString& Parameters)
{
	using FSolver = FEGTurnInPlaceSolver;

	// Upright rotation: normal gravity keeps the plain yaw and drops a tilt.
	const FRotator Flat = FSolver::MakeUprightRotation(FRotator(10.0, 30.0, 5.0), 20.0f);
	TestEqual(TEXT("World up: yaw added"), Flat.Yaw, 50.0, 1.e-3);
	TestEqual(TEXT("World up: pitch cleared"), Flat.Pitch, 0.0, 1.e-3);
	TestEqual(TEXT("World up: roll cleared"), Flat.Roll, 0.0, 1.e-3);

	// Custom gravity: the result is upright to Up and turned about it by the asked yaw.
	const FVector Up = FVector(0.0, 1.0, 1.0).GetSafeNormal();
	const FRotator Base = FRotationMatrix::MakeFromZX(Up, FVector::ForwardVector).Rotator();
	const FRotator Turned = FSolver::MakeUprightRotation(Base, 45.0f, Up);
	TestTrue(TEXT("Custom up: stays upright"), Turned.Quaternion().GetUpVector().Equals(Up, 1.e-3));
	TestEqual(TEXT("Custom up: turned about Up"), FSolver::YawDeltaAroundUp(Base, Turned, Up), 45.0f, 0.05f);

	// Procedural turn curves.
	FEGTurnInPlaceProcedural Settings;
	Settings.TurnRate = 90.0f;
	Settings.MinDuration = 0.25f;
	TestEqual(TEXT("Duration from the rate"), FSolver::GetProceduralDuration(90.0f, Settings), 1.0f, 1.e-4f);
	TestEqual(TEXT("Duration never below the minimum"), FSolver::GetProceduralDuration(5.0f, Settings), 0.25f, 1.e-4f);

	FEGTurnInPlaceCurves Curves = FSolver::EvaluateProceduralTurn(90.0f, 0.0, Settings);
	TestEqual(TEXT("Starts with the whole yaw"), Curves.RemainingTurnYaw, 90.0f, 1.e-3f);
	TestTrue(TEXT("...and the weight up"), Curves.IsTurning());
	Curves = FSolver::EvaluateProceduralTurn(90.0f, 0.5, Settings);
	TestEqual(TEXT("Half way at half time (eased symmetrically)"), Curves.RemainingTurnYaw, 45.0f, 1.e-3f);
	Curves = FSolver::EvaluateProceduralTurn(-90.0f, 1.0, Settings);
	TestEqual(TEXT("Ends with nothing left"), Curves.RemainingTurnYaw, 0.0f, 1.e-3f);
	TestFalse(TEXT("...and the weight down"), Curves.IsTurning());

	// Settle: only small offsets, only after the delay, reset when anything else happens.
	Settings.bSettle = true;
	Settings.SettleAngle = 15.0f;
	Settings.SettleDelay = 1.0f;
	float Timer = 0.0f;
	TestFalse(TEXT("Settle: not before the delay"), FSolver::UpdateSettleTimer(Timer, Settings, 30.0f, 60.0f, true, 0.6f));
	TestTrue(TEXT("Settle: after the delay"), FSolver::UpdateSettleTimer(Timer, Settings, 30.0f, 60.0f, true, 0.6f));
	TestFalse(TEXT("Settle: an offset below the settle angle is left alone"), FSolver::UpdateSettleTimer(Timer, Settings, 10.0f, 60.0f, true, 2.0f));
	TestEqual(TEXT("...and resets the timer"), Timer, 0.0f);
	TestFalse(TEXT("Settle: a real turn is not a settle"), FSolver::UpdateSettleTimer(Timer, Settings, 70.0f, 60.0f, true, 2.0f));
	TestFalse(TEXT("Settle: not while it cannot"), FSolver::UpdateSettleTimer(Timer, Settings, 30.0f, 60.0f, false, 2.0f));

	// BeginTurn: a settle turn is procedural even where the step has a clip slot.
	FEGTurnInPlaceGraphInput Input;
	Input.bValid = true;
	Input.TurnOffset = -30.0f;
	Input.bWantsSettle = true;
	FEGTurnInPlaceNodeState Node;
	FSolver::BeginTurn(Node, Input);
	TestTrue(TEXT("Settle turn is procedural"), Node.bProcedural);
	TestFalse(TEXT("...to the left"), Node.bTurningRight);
	TestEqual(TEXT("...closing the offset"), Node.ProceduralYaw, -30.0f);
	TestNull(TEXT("A procedural turn plays no clip"), FSolver::GetTurnAnimation(Input.GetEntry(), Node, false));

	// Root yaw track: a right turn of 90 degrees crossing +-180, sampled at 10 Hz over one second.
	TArray<double> Times;
	TArray<double> Yaws;
	for (int32 Index = 0; Index <= 10; ++Index)
	{
		Times.Add(Index * 0.1);
		Yaws.Add(FRotator::NormalizeAxis(135.0 + 9.0 * Index));
	}
	FEGTurnYawTrack Track;
	TestTrue(TEXT("Track builds"), FSolver::BuildYawTrack(Times, Yaws, 1.0f, 0.2f, Track));
	TestEqual(TEXT("Track unwraps across 180"), Track.TotalYaw, 90.0f, 1.e-3f);
	TestEqual(TEXT("Weight ends the recovery blend before the end"), Track.WeightEndTime, 0.8f, 1.e-4f);

	Curves = FSolver::EvaluateYawTrack(Track, 0.0);
	TestEqual(TEXT("Track: all of it remains at the start"), Curves.RemainingTurnYaw, 90.0f, 1.e-3f);
	TestTrue(TEXT("Track: turning at the start"), Curves.IsTurning());
	Curves = FSolver::EvaluateYawTrack(Track, 0.55);
	TestEqual(TEXT("Track: interpolates between samples"), Curves.RemainingTurnYaw, 40.5f, 1.e-3f);
	Curves = FSolver::EvaluateYawTrack(Track, 0.9);
	TestFalse(TEXT("Track: recovery past the weight end"), Curves.IsTurning());

	TArray<double> Short = { 0.0 };
	TestFalse(TEXT("A single sample is not a track"), FSolver::BuildYawTrack(Short, Short, 1.0f, 0.2f, Track));
	return true;
}

// -----------------------------------------------------------------------------
// Last input direction
// -----------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGTurnInPlaceLastInputTest,
	"UnrealExtendedGameplay.Animation.TurnInPlace.LastInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGTurnInPlaceLastInputTest::RunTest(const FString& Parameters)
{
	using FSolver = FEGTurnInPlaceSolver;

	FVector Direction = FVector::ForwardVector;
	FSolver::FLastInputInputs In;
	In.Acceleration = FVector(0.0, 1.0, 0.0);

	In.AnalogInput = 0.3f;
	FSolver::UpdateLastInputDirection(Direction, In);
	TestEqual(TEXT("A stick bouncing back past centre is ignored"), Direction, FVector::ForwardVector);

	In.AnalogInput = 0.9f;
	FSolver::UpdateLastInputDirection(Direction, In);
	TestEqual(TEXT("Real input sets the direction"), Direction, FVector(0.0, 1.0, 0.0));

	In.AnalogInput = 0.0f;
	In.Acceleration = FVector::ZeroVector;
	In.bMovingOnGround = false;
	In.Velocity = FVector(-300.0, 0.0, 900.0);
	FSolver::UpdateLastInputDirection(Direction, In);
	TestEqual(TEXT("In the air only horizontal velocity counts"), Direction, FVector(-1.0, 0.0, 0.0));

	In.bMovingOnGround = true;
	In.Velocity = FVector(0.0, -400.0, 0.0);
	In.TimeSinceRootMotion = 0.1;
	FSolver::UpdateLastInputDirection(Direction, In);
	TestEqual(TEXT("Velocity right after root motion is not input"), Direction, FVector(-1.0, 0.0, 0.0));

	In.bRootMotion = true;
	In.Forward = FVector(0.0, 0.0, 1.0);
	FSolver::UpdateLastInputDirection(Direction, In);
	TestEqual(TEXT("During root motion the facing is the intent"), Direction, FVector(0.0, 0.0, 1.0));

	In.bRootMotion = false;
	In.Velocity = FVector::ZeroVector;
	In.bBotControlled = true;
	In.ControlForward = FVector(0.0, -1.0, 0.0);
	FSolver::UpdateLastInputDirection(Direction, In);
	TestEqual(TEXT("Idle bots follow their control rotation"), Direction, FVector(0.0, -1.0, 0.0));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
