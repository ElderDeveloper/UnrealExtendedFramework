// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "UnrealExtendedGameplay/Systems/Minigames/SkillCheck/EGSkillCheckTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace EGSkillCheckTest
{
	/**
	 * A round with nothing left to chance, so a test can name the angle it means.
	 *
	 * Every scenario below starts from this and overrides exactly the fields it is about. The
	 * whole point of the extraction is that this needs no world, no widget and no tick.
	 */
	static FEGSkillCheckConfig MakeFixedConfig()
	{
		FEGSkillCheckConfig Config;
		Config.Seed = 1234;
		Config.SweepMode = EEGSkillCheckSweepMode::Continuous;
		Config.SweepDegreesPerSecond = 180.0f;
		Config.SweepDirection = 1.0f;
		Config.bRandomizeDirection = false;
		Config.StartAngleDegrees = 0.0f;
		Config.bRandomizeStartAngle = false;
		Config.ZoneCentreDegrees = 90.0f;
		Config.bRandomizeZoneCentre = false;
		Config.ZoneWidthDegrees = 60.0f;
		Config.PerfectWidthDegrees = 10.0f;
		return Config;
	}
}

// -----------------------------------------------------------------------------
// Test 1 — A zone straddling 0/360 is a zone, not an empty set
// -----------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGSkillCheckWraparoundTest,
	"UnrealExtendedGameplay.SkillCheck.Wraparound",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGSkillCheckWraparoundTest::RunTest(const FString& Parameters)
{
	// Zone centred on 0 with a 60-degree band: it runs 330..360 and 0..30, and the naive
	// "min <= angle <= max" that every dial minigame is first written with scores nothing at all.
	FEGSkillCheckConfig Config = EGSkillCheckTest::MakeFixedConfig();
	Config.ZoneCentreDegrees = 0.0f;
	Config.ZoneWidthDegrees = 60.0f;
	Config.PerfectWidthDegrees = 10.0f;

	FEGSkillCheckState State;
	TestTrue(TEXT("A fixed config starts"), State.Start(Config));

	TestEqual(TEXT("Dead centre on the seam is Perfect"),
		State.EvaluateAtAngle(0.0f).Tier, EEGSkillCheckTier::Perfect);
	TestEqual(TEXT("Just below the seam is Perfect"),
		State.EvaluateAtAngle(356.0f).Tier, EEGSkillCheckTier::Perfect);
	TestEqual(TEXT("Just above the seam is Perfect"),
		State.EvaluateAtAngle(4.0f).Tier, EEGSkillCheckTier::Perfect);

	TestEqual(TEXT("Inside the band below the seam is Good"),
		State.EvaluateAtAngle(340.0f).Tier, EEGSkillCheckTier::Good);
	TestEqual(TEXT("Inside the band above the seam is Good"),
		State.EvaluateAtAngle(20.0f).Tier, EEGSkillCheckTier::Good);

	TestEqual(TEXT("Outside the band below is a Miss"),
		State.EvaluateAtAngle(320.0f).Tier, EEGSkillCheckTier::Miss);
	TestEqual(TEXT("Outside the band above is a Miss"),
		State.EvaluateAtAngle(40.0f).Tier, EEGSkillCheckTier::Miss);
	TestEqual(TEXT("The far side of the dial is a Miss"),
		State.EvaluateAtAngle(180.0f).Tier, EEGSkillCheckTier::Miss);

	// The signed error has to cross the seam too, or every readout built on it lies by 360.
	TestTrue(TEXT("Error below the seam is small and negative"),
		FMath::IsNearlyEqual(State.EvaluateAtAngle(350.0f).SignedErrorDegrees, -10.0f, 0.01f));
	TestTrue(TEXT("Error above the seam is small and positive"),
		FMath::IsNearlyEqual(State.EvaluateAtAngle(10.0f).SignedErrorDegrees, 10.0f, 0.01f));

	// An unnormalized angle is the same angle. A caller handing over 725 has not left the zone.
	TestEqual(TEXT("725 degrees is 5 degrees"),
		State.EvaluateAtAngle(725.0f).Tier, EEGSkillCheckTier::Perfect);
	TestEqual(TEXT("-5 degrees is 355 degrees"),
		State.EvaluateAtAngle(-5.0f).Tier, EEGSkillCheckTier::Perfect);

	// Accuracy is symmetric across the seam — it reads the signed delta, not the raw numbers.
	const float BelowAccuracy = State.EvaluateAtAngle(350.0f).Accuracy;
	const float AboveAccuracy = State.EvaluateAtAngle(10.0f).Accuracy;
	TestTrue(TEXT("Accuracy is symmetric across the seam"),
		FMath::IsNearlyEqual(BelowAccuracy, AboveAccuracy, 0.001f));
	TestTrue(TEXT("Accuracy falls off the centre"), BelowAccuracy < 1.0f && BelowAccuracy > 0.0f);
	TestTrue(TEXT("Dead centre reads 1.0"),
		FMath::IsNearlyEqual(State.EvaluateAtAngle(0.0f).Accuracy, 1.0f, 0.001f));
	TestTrue(TEXT("A Miss reads 0.0"),
		FMath::IsNearlyEqual(State.EvaluateAtAngle(180.0f).Accuracy, 0.0f, 0.001f));

	// And a zone that spans everything contains everything — the degenerate case the donor's
	// interval branch would have collapsed to a single admissible angle.
	FEGSkillCheckConfig FullCircle = Config;
	FullCircle.ZoneWidthDegrees = 360.0f;
	FullCircle.PerfectWidthDegrees = 0.0f;
	FEGSkillCheckState FullState;
	TestTrue(TEXT("A full-circle zone is a valid config"), FullState.Start(FullCircle));
	TestEqual(TEXT("A full-circle zone admits the far side"),
		FullState.EvaluateAtAngle(180.0f).Tier, EEGSkillCheckTier::Good);
	TestEqual(TEXT("A zone with no Perfect width never grades Perfect"),
		FullState.EvaluateAtAngle(0.0f).Tier, EEGSkillCheckTier::Good);

	return true;
}

// -----------------------------------------------------------------------------
// Test 2 — Same seed, same elapsed time, same round. Everywhere.
// -----------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGSkillCheckDeterminismTest,
	"UnrealExtendedGameplay.SkillCheck.Determinism",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGSkillCheckDeterminismTest::RunTest(const FString& Parameters)
{
	FEGSkillCheckConfig Config = EGSkillCheckTest::MakeFixedConfig();
	Config.Seed = 90210;
	Config.bRandomizeDirection = true;
	Config.bRandomizeStartAngle = true;
	Config.bRandomizeZoneCentre = true;

	FEGSkillCheckState A;
	FEGSkillCheckState B;
	TestTrue(TEXT("First state starts"), A.Start(Config));
	TestTrue(TEXT("Second state starts"), B.Start(Config));

	TestTrue(TEXT("Same seed rolls the same zone centre"),
		FMath::IsNearlyEqual(A.GetResolvedConfig().ZoneCentreDegrees, B.GetResolvedConfig().ZoneCentreDegrees));
	TestTrue(TEXT("Same seed rolls the same start angle"),
		FMath::IsNearlyEqual(A.GetResolvedConfig().StartAngleDegrees, B.GetResolvedConfig().StartAngleDegrees));
	TestTrue(TEXT("Same seed rolls the same direction"),
		FMath::IsNearlyEqual(A.GetResolvedConfig().SweepDirection, B.GetResolvedConfig().SweepDirection));

	// The resolved config carries no remaining randomness. A client handed one draws exactly the
	// round the server is judging, which is what makes it safe to send.
	TestFalse(TEXT("Resolved config has no unrolled direction"), A.GetResolvedConfig().bRandomizeDirection);
	TestFalse(TEXT("Resolved config has no unrolled start angle"), A.GetResolvedConfig().bRandomizeStartAngle);
	TestFalse(TEXT("Resolved config has no unrolled zone centre"), A.GetResolvedConfig().bRandomizeZoneCentre);

	// Frame-rate independence, stated as the property it actually is: the angle is a function of
	// elapsed seconds alone. B is advanced in seven ragged steps — a 13 ms frame next to a
	// 600 ms hitch — and A in one jump, and they land on the same angle. The donor, integrating
	// CurrentAngle inside NativeTick, could not do this.
	const float TargetSeconds = 1.7f;
	A.AdvanceTo(TargetSeconds);

	const float RaggedSteps[] = { 0.013f, 0.4f, 0.002f, 0.25f, 0.6f, 0.1f, 0.335f };
	float Running = 0.0f;
	for (const float Step : RaggedSteps)
	{
		Running += Step;
		B.AdvanceTo(Running);
	}
	B.AdvanceTo(TargetSeconds);

	TestTrue(TEXT("One jump and twenty ragged steps land on the same angle"),
		FMath::IsNearlyEqual(A.GetCurrentAngleDegrees(), B.GetCurrentAngleDegrees(), 0.01f));

	// Advancing to the same time twice is not a second advance.
	const float Held = A.GetCurrentAngleDegrees();
	A.AdvanceTo(TargetSeconds);
	TestTrue(TEXT("AdvanceTo is idempotent"), FMath::IsNearlyEqual(A.GetCurrentAngleDegrees(), Held));

	// Grades agree because angles agree.
	const FEGSkillCheckResult ResultA = A.Stop(TargetSeconds);
	const FEGSkillCheckResult ResultB = B.Stop(TargetSeconds);
	TestEqual(TEXT("Same seed and time grade identically"), ResultA.Tier, ResultB.Tier);
	TestTrue(TEXT("Same seed and time score identically"),
		FMath::IsNearlyEqual(ResultA.Accuracy, ResultB.Accuracy, 0.001f));

	// A different seed is a different round. Without this the determinism above would also be
	// satisfied by a model that ignored the seed entirely.
	FEGSkillCheckConfig Other = Config;
	Other.Seed = 90211;
	FEGSkillCheckState C;
	TestTrue(TEXT("A differently seeded state starts"), C.Start(Other));
	TestFalse(TEXT("A different seed rolls a different round"),
		FMath::IsNearlyEqual(A.GetResolvedConfig().ZoneCentreDegrees, C.GetResolvedConfig().ZoneCentreDegrees, 0.01f)
		&& FMath::IsNearlyEqual(A.GetResolvedConfig().StartAngleDegrees, C.GetResolvedConfig().StartAngleDegrees, 0.01f));

	// The seed describes the round on its own: turning one roll off must not shift the others,
	// or a config and a seed together would still not be enough to replay what a player saw.
	FEGSkillCheckConfig FixedDirection = Config;
	FixedDirection.bRandomizeDirection = false;
	FixedDirection.SweepDirection = 1.0f;
	FEGSkillCheckState D;
	TestTrue(TEXT("A partly fixed config starts"), D.Start(FixedDirection));
	TestTrue(TEXT("Fixing the direction does not move the rolled zone centre"),
		FMath::IsNearlyEqual(A.GetResolvedConfig().ZoneCentreDegrees, D.GetResolvedConfig().ZoneCentreDegrees, 0.001f));
	TestTrue(TEXT("Fixing the direction does not move the rolled start angle"),
		FMath::IsNearlyEqual(A.GetResolvedConfig().StartAngleDegrees, D.GetResolvedConfig().StartAngleDegrees, 0.001f));

	return true;
}

// -----------------------------------------------------------------------------
// Test 3 — One attempt resolves exactly once
// -----------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGSkillCheckSingleCompletionTest,
	"UnrealExtendedGameplay.SkillCheck.SingleCompletion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGSkillCheckSingleCompletionTest::RunTest(const FString& Parameters)
{
	const FEGSkillCheckConfig Config = EGSkillCheckTest::MakeFixedConfig();

	// Stopping before starting resolves nothing and grades nothing.
	{
		FEGSkillCheckState Cold;
		TestFalse(TEXT("An unstarted state is not running"), Cold.IsRunning());
		const FEGSkillCheckResult Result = Cold.Stop(1.0f);
		TestEqual(TEXT("Stopping an unstarted round grades a Miss"), Result.Tier, EEGSkillCheckTier::Miss);
		TestFalse(TEXT("Stopping an unstarted round does not complete it"), Cold.IsComplete());
	}

	// Start, stop, stop again: the second stop returns the banked result and moves nothing.
	{
		FEGSkillCheckState State;
		TestTrue(TEXT("State starts"), State.Start(Config));
		TestTrue(TEXT("A started state is running"), State.IsRunning());

		// 0.5s at 180 deg/s from 0 puts the indicator on 90 — dead centre of the zone.
		const FEGSkillCheckResult First = State.Stop(0.5f);
		TestEqual(TEXT("A stop at the zone centre is Perfect"), First.Tier, EEGSkillCheckTier::Perfect);
		TestTrue(TEXT("The stop angle is where the trajectory says"),
			FMath::IsNearlyEqual(First.StopAngleDegrees, 90.0f, 0.01f));
		TestTrue(TEXT("The result records the elapsed seconds"),
			FMath::IsNearlyEqual(First.ElapsedSeconds, 0.5f, 0.001f));
		TestFalse(TEXT("A stopped state stops running"), State.IsRunning());
		TestTrue(TEXT("A stopped state is complete"), State.IsComplete());

		// A second stop at a time that WOULD have graded differently. If completion were not
		// once-only, this is what a double payout looks like from the model's side.
		const FEGSkillCheckResult Second = State.Stop(1.5f);
		TestEqual(TEXT("A replayed stop returns the banked tier"), Second.Tier, First.Tier);
		TestTrue(TEXT("A replayed stop returns the banked angle"),
			FMath::IsNearlyEqual(Second.StopAngleDegrees, First.StopAngleDegrees, 0.001f));
		TestTrue(TEXT("A replayed stop does not move the indicator"),
			FMath::IsNearlyEqual(State.GetCurrentAngleDegrees(), 90.0f, 0.01f));

		// Nor does advancing after completion.
		State.AdvanceTo(3.0f);
		TestTrue(TEXT("A completed round ignores further advances"),
			FMath::IsNearlyEqual(State.GetCurrentAngleDegrees(), 90.0f, 0.01f));
	}

	// Start, cancel, stop: an abandoned round cannot be cashed in afterwards.
	{
		FEGSkillCheckState State;
		TestTrue(TEXT("State starts"), State.Start(Config));
		State.AdvanceTo(0.5f);
		State.Cancel();

		TestFalse(TEXT("A cancelled state stops running"), State.IsRunning());
		TestTrue(TEXT("A cancelled state records the cancellation"), State.IsCancelled());
		TestFalse(TEXT("A cancelled state is not complete"), State.IsComplete());

		const FEGSkillCheckResult AfterCancel = State.Stop(0.5f);
		TestEqual(TEXT("Stopping a cancelled round grades nothing"), AfterCancel.Tier, EEGSkillCheckTier::Miss);
		TestFalse(TEXT("Stopping a cancelled round does not complete it"), State.IsComplete());

		// And cancelling twice is not two cancellations.
		State.Cancel();
		TestTrue(TEXT("A second cancel is a no-op"), State.IsCancelled());
	}

	// Restarting is a fresh round, not a resumed one.
	{
		FEGSkillCheckState State;
		TestTrue(TEXT("State starts"), State.Start(Config));
		State.Stop(0.5f);
		TestTrue(TEXT("A completed state can start again"), State.Start(Config));
		TestTrue(TEXT("A restarted state is running"), State.IsRunning());
		TestFalse(TEXT("A restart clears completion"), State.IsComplete());
		TestFalse(TEXT("A restart clears cancellation"), State.IsCancelled());
		TestTrue(TEXT("A restart rewinds elapsed time"), FMath::IsNearlyZero(State.GetElapsedSeconds()));
		TestEqual(TEXT("A restart clears the banked result"),
			State.GetLastResult().Tier, EEGSkillCheckTier::Miss);
	}

	return true;
}

// -----------------------------------------------------------------------------
// Test 4 — The needle sweeps back and forth, and a bad config is refused
// -----------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGSkillCheckSweepModesTest,
	"UnrealExtendedGameplay.SkillCheck.SweepModes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGSkillCheckSweepModesTest::RunTest(const FString& Parameters)
{
	// A gauge arc of 0..180 at 180 deg/s: one second to the far end, one second back.
	FEGSkillCheckConfig Config = EGSkillCheckTest::MakeFixedConfig();
	Config.SweepMode = EEGSkillCheckSweepMode::PingPong;
	Config.SweepMinAngleDegrees = 0.0f;
	Config.SweepMaxAngleDegrees = 180.0f;
	Config.StartAngleDegrees = 0.0f;
	Config.SweepDegreesPerSecond = 180.0f;

	FEGSkillCheckState State;
	TestTrue(TEXT("A ping-pong config starts"), State.Start(Config));

	TestTrue(TEXT("At t=0 the needle is at the low end"),
		FMath::IsNearlyEqual(State.AngleAtElapsed(0.0f), 0.0f, 0.01f));
	TestTrue(TEXT("At t=0.5 it is halfway up"),
		FMath::IsNearlyEqual(State.AngleAtElapsed(0.5f), 90.0f, 0.01f));
	TestTrue(TEXT("At t=1 it is at the high end"),
		FMath::IsNearlyEqual(State.AngleAtElapsed(1.0f), 180.0f, 0.01f));
	TestTrue(TEXT("At t=1.5 it has turned round"),
		FMath::IsNearlyEqual(State.AngleAtElapsed(1.5f), 90.0f, 0.01f));
	TestTrue(TEXT("At t=2 it is back at the low end"),
		FMath::IsNearlyEqual(State.AngleAtElapsed(2.0f), 0.0f, 0.01f));
	TestTrue(TEXT("The cycle repeats"),
		FMath::IsNearlyEqual(State.AngleAtElapsed(2.5f), State.AngleAtElapsed(0.5f), 0.01f));

	// The needle never leaves its gauge — the property a continuous sweep does not have, and the
	// one a band drawn on an arc depends on.
	bool bStayedOnGauge = true;
	for (int32 Step = 0; Step <= 400; ++Step)
	{
		const float Angle = State.AngleAtElapsed(Step * 0.037f);
		if (Angle < Config.SweepMinAngleDegrees - 0.01f || Angle > Config.SweepMaxAngleDegrees + 0.01f)
		{
			bStayedOnGauge = false;
			break;
		}
	}
	TestTrue(TEXT("The needle never leaves the arc"), bStayedOnGauge);

	// A rolled start and zone land on the gauge too, or the round is unwinnable rather than hard.
	FEGSkillCheckConfig Rolled = Config;
	Rolled.bRandomizeStartAngle = true;
	Rolled.bRandomizeZoneCentre = true;
	bool bAllRollsOnGauge = true;
	for (int32 Seed = 0; Seed < 200; ++Seed)
	{
		Rolled.Seed = Seed;
		FEGSkillCheckState RolledState;
		if (!RolledState.Start(Rolled))
		{
			bAllRollsOnGauge = false;
			break;
		}

		const FEGSkillCheckConfig& Resolved = RolledState.GetResolvedConfig();
		if (Resolved.StartAngleDegrees < Rolled.SweepMinAngleDegrees - 0.01f
			|| Resolved.StartAngleDegrees > Rolled.SweepMaxAngleDegrees + 0.01f
			|| Resolved.ZoneCentreDegrees < Rolled.SweepMinAngleDegrees - 0.01f
			|| Resolved.ZoneCentreDegrees > Rolled.SweepMaxAngleDegrees + 0.01f)
		{
			bAllRollsOnGauge = false;
			break;
		}
	}
	TestTrue(TEXT("Every rolled ping-pong round is reachable"), bAllRollsOnGauge);

	// Direction reverses the trajectory rather than the arc.
	FEGSkillCheckConfig Reversed = Config;
	Reversed.StartAngleDegrees = 90.0f;
	Reversed.SweepDirection = -1.0f;
	FEGSkillCheckState ReversedState;
	TestTrue(TEXT("A reversed config starts"), ReversedState.Start(Reversed));
	TestTrue(TEXT("A negative direction walks the needle down"),
		ReversedState.AngleAtElapsed(0.25f) < 90.0f);
	TestTrue(TEXT("A reversed needle still bounces off the low end"),
		ReversedState.AngleAtElapsed(0.75f) > 0.0f);

	// Configs that cannot produce a playable round are refused, not repaired.
	{
		FEGSkillCheckConfig Bad = Config;
		Bad.SweepMaxAngleDegrees = Bad.SweepMinAngleDegrees;
		FEGSkillCheckState BadState;
		FString Error;
		TestFalse(TEXT("A zero-width gauge is refused"), BadState.Start(Bad, &Error));
		TestFalse(TEXT("The refusal says why"), Error.IsEmpty());
		TestFalse(TEXT("A refused start leaves the state inert"), BadState.IsRunning());
	}
	{
		FEGSkillCheckConfig Bad = EGSkillCheckTest::MakeFixedConfig();
		Bad.PerfectWidthDegrees = Bad.ZoneWidthDegrees + 1.0f;
		FEGSkillCheckState BadState;
		TestFalse(TEXT("A Perfect band wider than the Good band is refused"), BadState.Start(Bad));
	}
	{
		FEGSkillCheckConfig Bad = EGSkillCheckTest::MakeFixedConfig();
		Bad.SweepDegreesPerSecond = 0.0f;
		FEGSkillCheckState BadState;
		TestFalse(TEXT("A motionless indicator is refused"), BadState.Start(Bad));
	}
	{
		FEGSkillCheckConfig Bad = EGSkillCheckTest::MakeFixedConfig();
		Bad.ZoneWidthDegrees = 0.0f;
		FEGSkillCheckState BadState;
		TestFalse(TEXT("A zero-width zone is refused"), BadState.Start(Bad));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
