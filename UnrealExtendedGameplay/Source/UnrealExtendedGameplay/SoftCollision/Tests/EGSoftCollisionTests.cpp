// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Math/RandomStream.h"
#include "UnrealExtendedGameplay/SoftCollision/EGSoftCollisionComponent.h"
#include "UnrealExtendedGameplay/SoftCollision/EGSoftCollisionSubsystem.h"
#include "UnrealExtendedGameplay/SoftCollision/EGSoftCollisionGrid.h"
#include "UnrealExtendedGameplay/SoftCollision/EGSoftCollisionMath.h"
#include "UnrealExtendedGameplay/SoftCollision/Prediction/EGSoftCollisionPrediction.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace EGSoftCollisionTest
{
	/** A capsule-sized cylinder standing at (X, Y), feet on the ground. */
	static FEGSoftBody MakeCylinder(int32 Id, double X, double Y, float Radius = 40.0f, const FVector2D& Velocity = FVector2D::ZeroVector)
	{
		FEGSoftBody Body;
		Body.Id = Id;
		Body.Shape = EEGSoftCollisionShapeType::Cylinder;
		Body.Location = FVector(X, Y, 90.0);
		Body.Radius = Radius;
		Body.HalfHeight = 90.0f;
		Body.Velocity = Velocity;
		return Body;
	}

	static FEGSoftBody MakeBox(int32 Id, double X, double Y, const FVector2D& HalfExtent, float YawDegrees = 0.0f)
	{
		FEGSoftBody Body;
		Body.Id = Id;
		Body.Shape = EEGSoftCollisionShapeType::Box;
		Body.Location = FVector(X, Y, 90.0);
		Body.BoxHalfExtent = HalfExtent;
		Body.HalfHeight = 90.0f;
		const float Radians = FMath::DegreesToRadians(YawDegrees);
		Body.Axis = FVector2D(FMath::Cos(Radians), FMath::Sin(Radians));
		return Body;
	}

	static bool NearlyEqual(const FVector2D& A, const FVector2D& B, double Tolerance = 1.0e-3)
	{
		return FVector2D::Distance(A, B) <= Tolerance;
	}
}

// -----------------------------------------------------------------------------
// Contacts
// -----------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGSoftCollisionContactTest,
	"UnrealExtendedGameplay.SoftCollision.Contacts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGSoftCollisionContactTest::RunTest(const FString& Parameters)
{
	using namespace EGSoftCollisionTest;
	using namespace EGSoftCollision;

	// Two 40 cm cylinders 60 cm apart overlap by 20 of their combined 80.
	{
		const FEGSoftBody Self = MakeCylinder(0, 60.0, 0.0);
		const FEGSoftBody Other = MakeCylinder(1, 0.0, 0.0);
		FContact Contact;
		TestTrue(TEXT("Overlapping cylinders touch"), ComputeContact(Self, Other, Contact));
		TestEqual(TEXT("Cylinder overlap"), Contact.Overlap, 20.0f, 1.0e-3f);
		TestEqual(TEXT("Cylinder penetration"), Contact.Penetration, 0.25f, 1.0e-4f);
		TestTrue(TEXT("Direction points away from the other pawn"), NearlyEqual(Contact.Direction, FVector2D(1.0, 0.0)));
	}

	// Exactly touching is not a contact.
	{
		FContact Contact;
		TestFalse(TEXT("Touching cylinders do not push"), ComputeContact(MakeCylinder(0, 80.0, 0.0), MakeCylinder(1, 0.0, 0.0), Contact));
	}

	// Heights must overlap too: a pawn standing on another's head is not pushed sideways.
	{
		FEGSoftBody Above = MakeCylinder(0, 10.0, 0.0);
		Above.Location.Z += 180.0;
		FContact Contact;
		TestFalse(TEXT("No contact without vertical overlap"), ComputeContact(Above, MakeCylinder(1, 0.0, 0.0), Contact));
	}

	// Coinciding centres still give opposite directions to the two pawns of the pair.
	{
		FContact A;
		FContact B;
		ComputeContact(MakeCylinder(3, 0.0, 0.0), MakeCylinder(7, 0.0, 0.0), A);
		ComputeContact(MakeCylinder(7, 0.0, 0.0), MakeCylinder(3, 0.0, 0.0), B);
		TestTrue(TEXT("Degenerate pair separates in opposite directions"), NearlyEqual(A.Direction, -B.Direction));
		TestEqual(TEXT("Coinciding centres are full penetration"), A.Penetration, 1.0f, 1.0e-4f);
	}

	// Circle outside a box face.
	{
		const FEGSoftBody Box = MakeBox(0, 0.0, 0.0, FVector2D(50.0, 50.0));
		const FEGSoftBody Circle = MakeCylinder(1, 60.0, 0.0, 20.0f);
		FContact CircleSide;
		FContact BoxSide;
		TestTrue(TEXT("Circle touches box"), ComputeContact(Circle, Box, CircleSide));
		TestTrue(TEXT("Box touches circle"), ComputeContact(Box, Circle, BoxSide));
		TestEqual(TEXT("Box-circle overlap"), CircleSide.Overlap, 10.0f, 1.0e-3f);
		TestEqual(TEXT("Box-circle penetration uses both extents"), CircleSide.Penetration, 10.0f / 70.0f, 1.0e-4f);
		TestTrue(TEXT("Circle is pushed off the face"), NearlyEqual(CircleSide.Direction, FVector2D(1.0, 0.0)));
		TestTrue(TEXT("Box is pushed the other way"), NearlyEqual(BoxSide.Direction, FVector2D(-1.0, 0.0)));
	}

	// Circle centre inside a box leaves through the nearest face.
	{
		const FEGSoftBody Box = MakeBox(0, 0.0, 0.0, FVector2D(50.0, 50.0));
		const FEGSoftBody Circle = MakeCylinder(1, 40.0, 5.0, 20.0f);
		FContact Contact;
		TestTrue(TEXT("Centre inside the box is a contact"), ComputeContact(Circle, Box, Contact));
		TestEqual(TEXT("Inside overlap reaches the nearest face"), Contact.Overlap, 30.0f, 1.0e-3f);
		TestTrue(TEXT("Inside leaves through the nearest face"), NearlyEqual(Contact.Direction, FVector2D(1.0, 0.0)));
	}

	// Rotated box: a box turned 90 degrees swaps its extents.
	{
		const FEGSoftBody Box = MakeBox(0, 0.0, 0.0, FVector2D(100.0, 20.0), 90.0f);
		FContact Contact;
		TestFalse(TEXT("Rotated box is narrow along X"), ComputeContact(MakeCylinder(1, 70.0, 0.0, 20.0f), Box, Contact));
		TestTrue(TEXT("Rotated box is long along Y"), ComputeContact(MakeCylinder(1, 0.0, 110.0, 20.0f), Box, Contact));
		TestTrue(TEXT("Pushed off the rotated box's end"), NearlyEqual(Contact.Direction, FVector2D(0.0, 1.0)));
	}

	// Two boxes separate along the axis of least overlap.
	{
		const FEGSoftBody A = MakeBox(0, 80.0, 10.0, FVector2D(50.0, 50.0));
		const FEGSoftBody B = MakeBox(1, 0.0, 0.0, FVector2D(50.0, 50.0));
		FContact Contact;
		TestTrue(TEXT("Boxes overlap"), ComputeContact(A, B, Contact));
		TestEqual(TEXT("Box-box overlap"), Contact.Overlap, 20.0f, 1.0e-3f);
		TestTrue(TEXT("Box-box direction"), NearlyEqual(Contact.Direction, FVector2D(1.0, 0.0)));
		TestFalse(TEXT("Separated boxes do not touch"), ComputeContact(MakeBox(0, 101.0, 0.0, FVector2D(50.0, 50.0)), B, Contact));
	}

	return true;
}

// -----------------------------------------------------------------------------
// Strength model
// -----------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGSoftCollisionStrengthTest,
	"UnrealExtendedGameplay.SoftCollision.Strength",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGSoftCollisionStrengthTest::RunTest(const FString& Parameters)
{
	using namespace EGSoftCollisionTest;
	using namespace EGSoftCollision;

	const FEGSoftCollisionTuning Tuning;

	// Momentum share.
	TestEqual(TEXT("Two idle equal pawns split evenly"), ComputeMomentumShare(1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 40.0f), 0.5f, 1.0e-4f);
	TestEqual(TEXT("An idle pawn hit by a runner takes most of it"),
		ComputeMomentumShare(1.0f, 0.0f, 1.0f, 1.0f, 400.0f, 40.0f), 400.0f / 440.0f, 1.0e-4f);
	TestEqual(TEXT("The runner takes little"),
		ComputeMomentumShare(1.0f, 400.0f, 1.0f, 1.0f, 0.0f, 40.0f), 40.0f / 440.0f, 1.0e-4f);
	TestEqual(TEXT("A pawn without push power pushes nobody"), ComputeMomentumShare(1.0f, 0.0f, 1.0f, 0.0f, 400.0f, 40.0f), 0.0f);
	TestTrue(TEXT("A heavier pawn takes less"),
		ComputeMomentumShare(4.0f, 0.0f, 1.0f, 1.0f, 0.0f, 40.0f) < ComputeMomentumShare(1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 40.0f));

	// Response and deep penetration.
	TestEqual(TEXT("Smoothstep at half"), EvaluateResponse(EEGSoftPushResponse::SmoothStep, 0.5f), 0.5f, 1.0e-4f);
	TestEqual(TEXT("Quadratic at half"), EvaluateResponse(EEGSoftPushResponse::Quadratic, 0.5f), 0.25f, 1.0e-4f);
	TestEqual(TEXT("Deep overlap reaches the full multiplier when centres meet"),
		EvaluatePenetrationFactor(Tuning, 1.0f), Tuning.DeepPenetrationMultiplier, 1.0e-4f);
	TestEqual(TEXT("No boost at the deep threshold"),
		EvaluatePenetrationFactor(Tuning, Tuning.DeepPenetration), EvaluateResponse(Tuning.Response, Tuning.DeepPenetration), 1.0e-4f);

	// Two idle overlapping pawns push each other apart equally.
	{
		const FEGSoftBody A = MakeCylinder(0, 40.0, 0.0);
		const FEGSoftBody B = MakeCylinder(1, 0.0, 0.0);
		FPushAccumulator ForA;
		FPushAccumulator ForB;
		TestTrue(TEXT("A sees B"), ForA.AddNeighbour(Tuning, A, B));
		TestTrue(TEXT("B sees A"), ForB.AddNeighbour(Tuning, B, A));
		const FEGSoftPushResult ResultA = ForA.Finish(Tuning);
		const FEGSoftPushResult ResultB = ForB.Finish(Tuning);
		TestTrue(TEXT("Idle pair is a separation"), ResultA.Band == EEGSoftPushBand::Separation);
		TestTrue(TEXT("Idle pair pushes equal and opposite"), NearlyEqual(ResultA.Velocity, -ResultB.Velocity, 1.0e-2));
		TestTrue(TEXT("A is pushed away from B"), ResultA.Velocity.X > 0.0);
		const float Expected = Tuning.SeparationStrength * EvaluatePenetrationFactor(Tuning, 0.5f);
		TestEqual(TEXT("Even share gives the full separation strength"), static_cast<float>(ResultA.Velocity.Size()), Expected, 1.0e-2f);
	}

	// A runner hitting an idle pawn bumps the idle pawn and is barely deflected itself.
	{
		const FEGSoftBody Idle = MakeCylinder(0, 50.0, 0.0);
		const FEGSoftBody Runner = MakeCylinder(1, 0.0, 0.0, 40.0f, FVector2D(400.0, 0.0));
		FPushAccumulator ForIdle;
		FPushAccumulator ForRunner;
		ForIdle.AddNeighbour(Tuning, Idle, Runner);
		ForRunner.AddNeighbour(Tuning, Runner, Idle);
		const FEGSoftPushResult IdleResult = ForIdle.Finish(Tuning);
		const FEGSoftPushResult RunnerResult = ForRunner.Finish(Tuning);
		TestTrue(TEXT("The idle pawn is bumped"), IdleResult.Band == EEGSoftPushBand::Bump);
		TestTrue(TEXT("The runner is not bumped"), RunnerResult.Band != EEGSoftPushBand::Bump);
		TestTrue(TEXT("The idle pawn moves much more than the runner"), IdleResult.Velocity.Size() > 4.0 * RunnerResult.Velocity.Size());
		TestTrue(TEXT("The idle pawn moves out of the runner's way"), IdleResult.Velocity.X > 0.0);
	}

	// Filters.
	{
		FEGSoftBody Self = MakeCylinder(0, 40.0, 0.0);
		FEGSoftBody Other = MakeCylinder(1, 0.0, 0.0);
		Other.Layer = 3;
		Self.PushedByMask = ~(1u << 3);
		FPushAccumulator Accumulator;
		TestFalse(TEXT("A masked layer does not push"), Accumulator.AddNeighbour(Tuning, Self, Other));
		Self.PushedByMask = 0xFFFFFFFFu;
		Other.bCanPush = false;
		TestFalse(TEXT("A pawn that cannot push does not"), Accumulator.AddNeighbour(Tuning, Self, Other));
		Other.bCanPush = true;
		Self.bCanBePushed = false;
		TestFalse(TEXT("A pawn that cannot be pushed is not"), Accumulator.AddNeighbour(Tuning, Self, Other));
		TestEqual(TEXT("Every rejected pair still counts as a test"), Accumulator.NeighbourTests, 3);
	}

	// Many neighbours are capped.
	{
		const FEGSoftBody Self = MakeCylinder(0, 0.0, 0.0);
		FPushAccumulator Accumulator;
		for (int32 Index = 0; Index < 8; ++Index)
		{
			// All on one side, deep, so the sum is far over the cap.
			Accumulator.AddNeighbour(Tuning, Self, MakeCylinder(Index + 1, -10.0, Index * 2.0));
		}
		const FEGSoftPushResult Result = Accumulator.Finish(Tuning);
		TestEqual(TEXT("The summed push is capped"), static_cast<float>(Result.Velocity.Size()), Tuning.MaxTotalStrength, 0.5f);
		TestEqual(TEXT("All neighbours counted"), Result.NeighbourCount, 8);
	}

	// Weak pushes are not issued.
	{
		FPushAccumulator Accumulator;
		Accumulator.AddNeighbour(Tuning, MakeCylinder(0, 79.0, 0.0), MakeCylinder(1, 0.0, 0.0));
		const FEGSoftPushResult Result = Accumulator.Finish(Tuning);
		TestTrue(TEXT("A barely touching pair issues nothing"), Result.Band == EEGSoftPushBand::None);
		TestEqual(TEXT("...but still counts the contact"), Result.NeighbourCount, 1);
	}

	// Decay.
	TestEqual(TEXT("Constant decay"), EvaluateDecayAverage(EEGSoftPushDecay::Constant, 0.0f, 1.0f), 1.0f, 1.0e-5f);
	TestEqual(TEXT("Linear decay average"), EvaluateDecayAverage(EEGSoftPushDecay::Linear, 0.0f, 1.0f), 0.5f, 1.0e-5f);
	TestEqual(TEXT("Ease-out decay average"), EvaluateDecayAverage(EEGSoftPushDecay::EaseOut, 0.0f, 1.0f), 1.0f / 3.0f, 1.0e-5f);
	TestEqual(TEXT("Tiny spans use the midpoint"), EvaluateDecayAverage(EEGSoftPushDecay::EaseOut, 0.5f, 0.5f), 0.25f, 1.0e-5f);
	TestEqual(TEXT("Decay past the end is zero"), EvaluateDecay(EEGSoftPushDecay::Linear, 2.0f), 0.0f, 1.0e-5f);

	return true;
}

// -----------------------------------------------------------------------------
// Quantization, sequences and rate limiting
// -----------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGSoftCollisionQuantizeTest,
	"UnrealExtendedGameplay.SoftCollision.Quantize",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGSoftCollisionQuantizeTest::RunTest(const FString& Parameters)
{
	using namespace EGSoftCollision;

	FRandomStream Stream(20261031);
	int32 RoundTripFailures = 0;
	int32 StabilityFailures = 0;
	for (int32 Index = 0; Index < 2000; ++Index)
	{
		const double Angle = static_cast<double>(Stream.FRandRange(-UE_PI, UE_PI));
		const double Speed = static_cast<double>(Stream.FRandRange(0.5f, 1500.0f));
		const FVector2D Velocity(FMath::Cos(Angle) * Speed, FMath::Sin(Angle) * Speed);

		const FEGSoftPushQuantized Quantized = Quantize(Velocity);
		const FVector2D Restored = Dequantize(Quantized);

		// Angle step is 360/65536 degrees, strength step 0.1 cm/s.
		const double AllowedError = QuantizedStrengthStep + Speed * (UE_DOUBLE_TWO_PI / 65536.0);
		if (FVector2D::Distance(Velocity, Restored) > AllowedError)
		{
			++RoundTripFailures;
		}

		// What the server builds from the integers must quantize back to the same integers.
		if (Quantize(Restored) != Quantized)
		{
			++StabilityFailures;
		}
	}
	TestEqual(TEXT("Quantize round trips stay within one step"), RoundTripFailures, 0);
	TestEqual(TEXT("Quantize is stable after a round trip"), StabilityFailures, 0);

	TestEqual(TEXT("Zero quantizes to zero"), static_cast<int32>(Quantize(FVector2D::ZeroVector).Strength), 0);
	TestEqual(TEXT("Strength saturates"), static_cast<int32>(Quantize(FVector2D(1.0e7, 0.0)).Strength), 65535);
	TestEqual(TEXT("East is yaw zero"), static_cast<int32>(Quantize(FVector2D(100.0, 0.0)).Yaw), 0);
	TestEqual(TEXT("North is a quarter turn"), static_cast<int32>(Quantize(FVector2D(0.0, 100.0)).Yaw), 16384);
	TestEqual(TEXT("Just below a full turn wraps cleanly"), static_cast<int32>(Quantize(FVector2D(100.0, -1.0e-6)).Yaw), 0);

	TestTrue(TEXT("Sequence order"), IsSequenceNewer(6, 5));
	TestFalse(TEXT("Sequence is not newer than itself"), IsSequenceNewer(5, 5));
	TestTrue(TEXT("Sequence wraps"), IsSequenceNewer(1, 65535));
	TestFalse(TEXT("Wrapped sequence is older"), IsSequenceNewer(65535, 1));

	FTokenBucket Bucket;
	TestTrue(TEXT("Burst 1"), Bucket.TryConsume(10.0, 1.0f, 3.0f));
	TestTrue(TEXT("Burst 2"), Bucket.TryConsume(10.0, 1.0f, 3.0f));
	TestTrue(TEXT("Burst 3"), Bucket.TryConsume(10.0, 1.0f, 3.0f));
	TestFalse(TEXT("Burst exhausted"), Bucket.TryConsume(10.0, 1.0f, 3.0f));
	TestTrue(TEXT("Refills over time"), Bucket.TryConsume(11.0, 1.0f, 3.0f));
	TestFalse(TEXT("Only one token per second"), Bucket.TryConsume(11.5, 1.0f, 3.0f));

	TestEqual(TEXT("Angle between opposite pushes"), AngleBetweenDegrees(FVector2D(1.0, 0.0), FVector2D(-2.0, 0.0)), 180.0f, 1.0e-3f);
	TestEqual(TEXT("Angle between perpendicular pushes"), AngleBetweenDegrees(FVector2D(1.0, 0.0), FVector2D(0.0, 3.0)), 90.0f, 1.0e-3f);

	return true;
}

// -----------------------------------------------------------------------------
// Grid
// -----------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGSoftCollisionGridTest,
	"UnrealExtendedGameplay.SoftCollision.Grid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGSoftCollisionGridTest::RunTest(const FString& Parameters)
{
	FEGSoftCollisionGrid Grid(100.0f);

	TestTrue(TEXT("Cell of a positive location"), Grid.GetCell(FVector(150.0, 250.0, 0.0)) == FIntPoint(1, 2));
	TestTrue(TEXT("Cells floor towards negative infinity"), Grid.GetCell(FVector(-1.0, -150.0, 0.0)) == FIntPoint(-1, -2));

	Grid.Add(0, FIntPoint(0, 0));
	Grid.Add(1, FIntPoint(1, 1));
	Grid.Add(2, FIntPoint(3, 3));
	TestEqual(TEXT("Entries counted"), Grid.GetNumEntries(), 3);

	const auto CollectNear = [&Grid](const FIntPoint& Cell)
	{
		TArray<int32> Found;
		Grid.ForEachNear(Cell, [&Found](int32 Id) { Found.Add(Id); });
		Found.Sort();
		return Found;
	};

	TestTrue(TEXT("3x3 neighbourhood sees adjacent cells"), CollectNear(FIntPoint(0, 0)) == TArray<int32>({ 0, 1 }));
	TestTrue(TEXT("3x3 neighbourhood skips far cells"), CollectNear(FIntPoint(3, 3)) == TArray<int32>({ 2 }));

	Grid.Move(2, FIntPoint(3, 3), FIntPoint(2, 2));
	TestTrue(TEXT("A moved entry is found in its new neighbourhood"), CollectNear(FIntPoint(1, 1)) == TArray<int32>({ 0, 1, 2 }));
	TestNull(TEXT("An emptied cell is removed"), Grid.FindBucket(FIntPoint(3, 3)));

	TestTrue(TEXT("Remove finds the entry"), Grid.Remove(1, FIntPoint(1, 1)));
	TestFalse(TEXT("Remove from the wrong cell fails"), Grid.Remove(0, FIntPoint(5, 5)));
	TestEqual(TEXT("Entries after removal"), Grid.GetNumEntries(), 2);

	TArray<int32> Wide;
	Grid.ForEachNear(FIntPoint(4, 4), [&Wide](int32 Id) { Wide.Add(Id); }, 2);
	TestTrue(TEXT("A wider radius reaches further"), Wide == TArray<int32>({ 2 }));

	Grid.Reset(50.0f);
	TestEqual(TEXT("Reset empties the grid"), Grid.GetNumEntries(), 0);
	TestEqual(TEXT("Reset changes the cell size"), Grid.GetCellSize(), 50.0f);

	return true;
}

// -----------------------------------------------------------------------------
// Prediction bookkeeping
// -----------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGSoftCollisionPredictionTest,
	"UnrealExtendedGameplay.SoftCollision.Prediction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGSoftCollisionPredictionTest::RunTest(const FString& Parameters)
{
	const auto MakeRequest = [](uint16 Sequence)
	{
		FEGSoftPushRequest Request;
		Request.Sequence = Sequence;
		Request.Band = EEGSoftPushBand::Separation;
		return Request;
	};

	// Batching and redundancy.
	{
		FEGSoftPushClientQueue Queue;
		TArray<FEGSoftPushRequest> Batch;

		Queue.Add(MakeRequest(1), 0.0);
		Queue.CollectBatch(0.0, 0.15f, 0.05f, 4, Batch);
		TestEqual(TEXT("A new request is sent at once"), Batch.Num(), 1);

		Queue.CollectBatch(0.01, 0.15f, 0.05f, 4, Batch);
		TestEqual(TEXT("No repeat inside the resend interval"), Batch.Num(), 0);

		Queue.CollectBatch(0.06, 0.15f, 0.05f, 4, Batch);
		TestEqual(TEXT("Repeated after the resend interval"), Batch.Num(), 1);

		Queue.Add(MakeRequest(2), 0.07);
		Queue.CollectBatch(0.07, 0.15f, 0.05f, 4, Batch);
		TestEqual(TEXT("Only the new request inside the interval"), Batch.Num(), 1);
		TestEqual(TEXT("...and it is the new one"), Batch.IsEmpty() ? 0 : static_cast<int32>(Batch[0].Sequence), 2);

		Queue.CollectBatch(0.2, 0.15f, 0.05f, 4, Batch);
		TestEqual(TEXT("Outside its window a request is no longer repeated"), Batch.Num(), 1);

		Queue.Acknowledge(1);
		TestEqual(TEXT("Acknowledging removes up to the sequence"), Queue.Num(), 1);
		Queue.Acknowledge(2);
		TestFalse(TEXT("Everything answered"), Queue.HasPending());
	}

	// Acknowledgement across the wrap.
	{
		FEGSoftPushClientQueue Queue;
		Queue.Add(MakeRequest(65534), 0.0);
		Queue.Add(MakeRequest(65535), 0.0);
		Queue.Add(MakeRequest(0), 0.0);
		Queue.Add(MakeRequest(1), 0.0);
		Queue.Acknowledge(0);
		TestEqual(TEXT("Wrapped acknowledgement keeps only newer requests"), Queue.Num(), 1);
		TestEqual(TEXT("...the one after the wrap"), static_cast<int32>(Queue.GetPending()[0].Request.Sequence), 1);
	}

	// Rejection and timeouts.
	{
		FEGSoftPushClientQueue Queue;
		Queue.Add(MakeRequest(5), 0.0);
		Queue.Add(MakeRequest(6), 0.3);
		TestTrue(TEXT("A rejected request is removed"), Queue.Remove(5));
		TestFalse(TEXT("...only once"), Queue.Remove(5));

		Queue.Add(MakeRequest(7), 0.0);
		TArray<uint16, TInlineAllocator<8>> TimedOut;
		Queue.CollectTimedOut(0.5, 0.4f, TimedOut);
		TestEqual(TEXT("Only old requests time out"), TimedOut.Num(), 1);
		TestEqual(TEXT("...the old one"), TimedOut.IsEmpty() ? 0 : static_cast<int32>(TimedOut[0]), 7);
		TestEqual(TEXT("The young one stays"), Queue.Num(), 1);
	}

	// Server duplicate detection.
	{
		FEGSoftPushServerState State;
		TestFalse(TEXT("Nothing is a duplicate before the first request"), State.IsDuplicate(10));
		State.MarkProcessed(10);
		TestTrue(TEXT("The same sequence is a duplicate"), State.IsDuplicate(10));
		TestTrue(TEXT("An older sequence is a duplicate"), State.IsDuplicate(9));
		TestFalse(TEXT("A newer sequence is not"), State.IsDuplicate(11));
		State.MarkProcessed(65535);
		TestFalse(TEXT("A wrapped sequence is newer"), State.IsDuplicate(0));
	}

	return true;
}

// -----------------------------------------------------------------------------
// Modifiers
// -----------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGSoftCollisionModifierTest,
	"UnrealExtendedGameplay.SoftCollision.Modifiers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGSoftCollisionModifierTest::RunTest(const FString& Parameters)
{
	FEGSoftCollisionModifierStack Stack;
	TestTrue(TEXT("An empty stack is neutral"), Stack.GetCombined().WeightScale == 1.0f && !Stack.GetCombined().bImmovable);

	FEGSoftCollisionModifier Heavy;
	Heavy.WeightScale = 4.0f;
	FEGSoftCollisionModifier Attack;
	Attack.bImmovable = true;
	Attack.WeightScale = 0.5f;

	const int32 HeavyId = Stack.Push(Heavy);
	const int32 AttackId = Stack.Push(Attack);
	TestTrue(TEXT("Ids are never zero"), HeavyId != 0 && AttackId != 0 && HeavyId != AttackId);

	FEGSoftCollisionModifier Combined = Stack.GetCombined();
	TestEqual(TEXT("Scales multiply"), Combined.WeightScale, 2.0f, 1.0e-5f);
	TestTrue(TEXT("Switches add up"), Combined.bImmovable);

	TestTrue(TEXT("Remove finds the modifier"), Stack.Remove(AttackId));
	TestFalse(TEXT("...only once"), Stack.Remove(AttackId));
	Combined = Stack.GetCombined();
	TestEqual(TEXT("The other modifier still holds"), Combined.WeightScale, 4.0f, 1.0e-5f);
	TestFalse(TEXT("The removed switch is gone"), Combined.bImmovable);
	TestFalse(TEXT("Removing id zero does nothing"), Stack.Remove(0));

	return true;
}

// -----------------------------------------------------------------------------
// World: the component, the subsystem and the force driver together
// -----------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGSoftCollisionWorldTest,
	"UnrealExtendedGameplay.SoftCollision.World",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGSoftCollisionWorldTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues WorldOptions = UWorld::InitializationValues()
		.AllowAudioPlayback(false).RequiresHitProxies(false).CreatePhysicsScene(true)
		.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false)
		.SetTransactional(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr,
		true, ERHIFeatureLevel::Num, &WorldOptions);
	if (!TestNotNull(TEXT("Test world"), World))
	{
		return false;
	}

	World->InitializeActorsForPlay(FURL());
	World->BeginPlay();
	// No game mode starts play in a bare world; the world settings do what it would.
	if (!World->HasBegunPlay() && World->GetWorldSettings())
	{
		World->GetWorldSettings()->NotifyBeginPlay();
	}

	if (!TestNotNull(TEXT("Soft collision subsystem"), UEGSoftCollisionSubsystem::Get(World)))
	{
		World->DestroyWorld(false);
		return false;
	}

	// Unpossessed Characters flying without gravity: the server-side force driver moves them, and
	// nothing else does.
	const auto SpawnPawn = [World](const FVector& Location, bool bCanBePushed) -> ACharacter*
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ACharacter* Character = World->SpawnActor<ACharacter>(Location, FRotator::ZeroRotator, SpawnParameters);
		if (!Character)
		{
			return nullptr;
		}
		UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
		Movement->bRunPhysicsWithNoController = true;
		Movement->GravityScale = 0.0f;
		Movement->SetMovementMode(MOVE_Flying);

		UEGSoftCollisionComponent* SoftCollision = NewObject<UEGSoftCollisionComponent>(Character);
		SoftCollision->Filter.bCanBePushed = bCanBePushed;
		Character->AddInstanceComponent(SoftCollision);
		SoftCollision->RegisterComponent();
		return Character;
	};

	ACharacter* A = SpawnPawn(FVector(0.0, 0.0, 500.0), true);
	ACharacter* B = SpawnPawn(FVector(40.0, 0.0, 500.0), true);
	ACharacter* Fixed = SpawnPawn(FVector(1000.0, 0.0, 500.0), false);
	ACharacter* Free = SpawnPawn(FVector(1040.0, 0.0, 500.0), true);
	if (!TestTrue(TEXT("Pawns spawned"), A && B && Fixed && Free))
	{
		World->DestroyWorld(false);
		return false;
	}

	const FVector FixedStart = Fixed->GetActorLocation();
	const FVector FreeStart = Free->GetActorLocation();

	constexpr float DeltaSeconds = 1.0f / 60.0f;
	for (int32 Frame = 0; Frame < 90; ++Frame)
	{
		World->Tick(LEVELTICK_All, DeltaSeconds);
	}

	TestTrue(TEXT("Overlapping pawns separate"), FVector::Dist2D(A->GetActorLocation(), B->GetActorLocation()) > 55.0);
	TestTrue(TEXT("They separate along the line between them"), A->GetActorLocation().X < 0.0 && B->GetActorLocation().X > 40.0);
	TestTrue(TEXT("A pawn that cannot be pushed stays put"), FVector::Dist2D(Fixed->GetActorLocation(), FixedStart) < 1.0);
	TestTrue(TEXT("...and the other pawn moves away from it alone"), FVector::Dist2D(Free->GetActorLocation(), FreeStart) > 10.0);

	World->DestroyWorld(false);
	return true;
}

#endif
