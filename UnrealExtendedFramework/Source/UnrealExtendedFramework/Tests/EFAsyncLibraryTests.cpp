// Copyright Moon Punch Games. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Curves/CurveFloat.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Testing/EFTestHelpers.h"
#include "TimerManager.h"
#include "UObject/StrongObjectPtr.h"
#include "UnrealExtendedFramework/AsyncLibrary/Delay/EFAsyncDelay.h"
#include "UnrealExtendedFramework/AsyncLibrary/Loop/EFAsyncForLoopDelay.h"
#include "UnrealExtendedFramework/AsyncLibrary/Loop/EFAsyncLoopDelay.h"
#include "UnrealExtendedFramework/AsyncLibrary/Timeline/EFAsyncFloatTimeline.h"
#include "UnrealExtendedFramework/Tests/EFAsyncTestListener.h"

namespace EFAsyncLibraryTests
{
	/** Game seconds each test frame advances the timers by. */
	constexpr float FrameSeconds = 0.05f;

	/**
	 * Nothing ticks the test game instance's world, so the waits below advance its timers
	 * themselves, once per engine frame (the timer manager ignores a second tick in one frame).
	 */
	void TickTimers(const EFTest::FScopedGameInstance& Game)
	{
		if (UWorld* World = Game.GetWorld())
		{
			World->GetTimerManager().Tick(FrameSeconds);
		}
	}
}


// ═══════════════════════════════════════════════════════════════════════════════
// Delay: Finished fires once, after the delay
// ═══════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEFAsyncDelayCompletesOnceTest, "UnrealExtendedFramework.AsyncLibrary.Delay.CompletesOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEFAsyncDelayCompletesOnceTest::RunTest(const FString& Parameters)
{
	struct FState
	{
		TSharedPtr<EFTest::FScopedGameInstance> Game;
		TStrongObjectPtr<UEFAsyncTestListener> Pins;
		TStrongObjectPtr<UEFAsyncDelay> Node;
		int32 FramesAfterFinished = 0;
	};
	const TSharedRef<FState> State = MakeShared<FState>();
	State->Game = MakeShared<EFTest::FScopedGameInstance>();

	UGameInstance* GameInstance = State->Game->Get();
	if (!TestNotNull(TEXT("the test game instance exists"), GameInstance)
		|| !TestNotNull(TEXT("the test game instance has a world"), State->Game->GetWorld()))
	{
		return false;
	}

	State->Pins.Reset(NewObject<UEFAsyncTestListener>());
	State->Node.Reset(UEFAsyncDelay::EFAsyncDelay(GameInstance, 0.2f));
	State->Node->Finished.AddDynamic(State->Pins.Get(), &UEFAsyncTestListener::HandleEvent);
	State->Node->Activate();

	TestEqual(TEXT("Finished waits for the delay"), State->Pins->EventCount, 0);
	TestTrue(TEXT("the node is running"), State->Node->IsActive());

	ADD_LATENT_AUTOMATION_COMMAND(FEFWaitUntilCommand(this, TEXT("the delay to finish"), [State]()
	{
		EFAsyncLibraryTests::TickTimers(*State->Game);
		return State->Pins->EventCount > 0;
	}));

	// A finished delay must stay finished.
	ADD_LATENT_AUTOMATION_COMMAND(FEFWaitUntilCommand(this, TEXT("a few frames after the delay"), [State]()
	{
		EFAsyncLibraryTests::TickTimers(*State->Game);
		return ++State->FramesAfterFinished >= 5;
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FEFRunLatentCommand([this, State]()
	{
		TestEqual(TEXT("Finished fired exactly once"), State->Pins->EventCount, 1);
		TestTrue(TEXT("the node finished"), State->Node->IsFinished());
		TestFalse(TEXT("the node is no longer active"), State->Node->IsActive());

		State->Node.Reset();
		State->Game.Reset();
	}));

	return true;
}


// ═══════════════════════════════════════════════════════════════════════════════
// Loop Delay: cancelling through the Async Task pin stops the loop
// ═══════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEFAsyncLoopDelayCancelTest, "UnrealExtendedFramework.AsyncLibrary.LoopDelay.CancelStopsLoop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEFAsyncLoopDelayCancelTest::RunTest(const FString& Parameters)
{
	struct FState
	{
		TSharedPtr<EFTest::FScopedGameInstance> Game;
		TStrongObjectPtr<UEFAsyncTestListener> Pins;
		TStrongObjectPtr<UEFAsyncLoopDelay> Node;
		int32 LoopsWhenCancelled = 0;
		int32 FramesAfterCancel = 0;
	};
	const TSharedRef<FState> State = MakeShared<FState>();
	State->Game = MakeShared<EFTest::FScopedGameInstance>();

	UGameInstance* GameInstance = State->Game->Get();
	if (!TestNotNull(TEXT("the test game instance exists"), GameInstance)
		|| !TestNotNull(TEXT("the test game instance has a world"), State->Game->GetWorld()))
	{
		return false;
	}

	State->Pins.Reset(NewObject<UEFAsyncTestListener>());
	State->Node.Reset(UEFAsyncLoopDelay::LoopDelay(GameInstance, /*shouldLoop*/ true, 0.05f));
	State->Node->Loop.AddDynamic(State->Pins.Get(), &UEFAsyncTestListener::HandleEvent);
	State->Node->Activate();

	ADD_LATENT_AUTOMATION_COMMAND(FEFWaitUntilCommand(this, TEXT("the loop to fire twice"), [State]()
	{
		EFAsyncLibraryTests::TickTimers(*State->Game);
		return State->Pins->EventCount >= 2;
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FEFRunLatentCommand([this, State]()
	{
		State->LoopsWhenCancelled = State->Pins->EventCount;
		State->Node->Cancel();
		TestTrue(TEXT("Cancel finishes the node"), State->Node->IsFinished());
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FEFWaitUntilCommand(this, TEXT("frames after the cancel"), [State]()
	{
		EFAsyncLibraryTests::TickTimers(*State->Game);
		return ++State->FramesAfterCancel >= 10;
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FEFRunLatentCommand([this, State]()
	{
		TestEqual(TEXT("Loop did not fire after Cancel"), State->Pins->EventCount, State->LoopsWhenCancelled);
		TestFalse(TEXT("the node is no longer active"), State->Node->IsActive());

		State->Node.Reset();
		State->Game.Reset();
	}));

	return true;
}


// ═══════════════════════════════════════════════════════════════════════════════
// Float Timeline: plays the curve to its end, then fires Completed once
// ═══════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEFAsyncFloatTimelineEndTest, "UnrealExtendedFramework.AsyncLibrary.FloatTimeline.ReachesEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEFAsyncFloatTimelineEndTest::RunTest(const FString& Parameters)
{
	struct FState
	{
		TSharedPtr<EFTest::FScopedGameInstance> Game;
		TStrongObjectPtr<UEFAsyncTestListener> Pins;
		TStrongObjectPtr<UCurveFloat> Curve;
		TStrongObjectPtr<UEFAsyncFloatTimeline> Node;
	};
	const TSharedRef<FState> State = MakeShared<FState>();
	State->Game = MakeShared<EFTest::FScopedGameInstance>();

	UGameInstance* GameInstance = State->Game->Get();
	if (!TestNotNull(TEXT("the test game instance exists"), GameInstance)
		|| !TestNotNull(TEXT("the test game instance has a world"), State->Game->GetWorld()))
	{
		return false;
	}

	// 0 -> 1 over one second, sampled every 0.1 seconds.
	State->Curve.Reset(NewObject<UCurveFloat>());
	State->Curve->FloatCurve.AddKey(0.f, 0.f);
	State->Curve->FloatCurve.AddKey(1.f, 1.f);

	UEFAsyncFloatTimeline* Timeline = UEFAsyncFloatTimeline::ExtendedFloatTimeline(
		FName(TEXT("EFTest.AsyncLibrary.FloatTimeline")), State->Curve.Get(), GameInstance, 0.1f, Ext_PlayFromStart, false);
	if (!TestNotNull(TEXT("the timeline starts"), Timeline))
	{
		return false;
	}

	State->Pins.Reset(NewObject<UEFAsyncTestListener>());
	State->Node.Reset(Timeline);
	Timeline->Loop.AddDynamic(State->Pins.Get(), &UEFAsyncTestListener::HandleFloat);
	Timeline->Completed.AddDynamic(State->Pins.Get(), &UEFAsyncTestListener::HandleEvent);
	Timeline->Activate();

	ADD_LATENT_AUTOMATION_COMMAND(FEFWaitUntilCommand(this, TEXT("the timeline to complete"), [State]()
	{
		EFAsyncLibraryTests::TickTimers(*State->Game);
		return State->Pins->EventCount > 0;
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FEFRunLatentCommand([this, State]()
	{
		const TArray<float>& Values = State->Pins->FloatValues;
		TestEqual(TEXT("Completed fired once"), State->Pins->EventCount, 1);
		TestTrue(TEXT("the node finished"), State->Node->IsFinished());
		if (TestTrue(TEXT("Loop fired for the whole curve"), Values.Num() >= 9))
		{
			TestTrue(TEXT("Loop started at the curve's start"), FMath::IsNearlyEqual(Values[0], 0.f, 0.01f));
			TestTrue(TEXT("Loop reached the curve's end"), Values.Last() >= 0.85f);

			bool bRising = true;
			for (int32 Index = 1; Index < Values.Num(); ++Index)
			{
				bRising &= Values[Index] >= Values[Index - 1];
			}
			TestTrue(TEXT("Loop values follow the curve forward"), bRising);
		}

		State->Node.Reset();
		State->Game.Reset();
	}));

	return true;
}


// ═══════════════════════════════════════════════════════════════════════════════
// For Loop With Delay: runs FirstIndex up to LastIndex, then fires Completed
// ═══════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEFAsyncForLoopDelayRangeTest, "UnrealExtendedFramework.AsyncLibrary.ForLoopDelay.RunsRangeThenCompletes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEFAsyncForLoopDelayRangeTest::RunTest(const FString& Parameters)
{
	struct FState
	{
		TSharedPtr<EFTest::FScopedGameInstance> Game;
		TStrongObjectPtr<UEFAsyncTestListener> Pins;
		TStrongObjectPtr<UEFAsyncForLoopDelay> Node;
	};
	const TSharedRef<FState> State = MakeShared<FState>();
	State->Game = MakeShared<EFTest::FScopedGameInstance>();

	UGameInstance* GameInstance = State->Game->Get();
	if (!TestNotNull(TEXT("the test game instance exists"), GameInstance)
		|| !TestNotNull(TEXT("the test game instance has a world"), State->Game->GetWorld()))
	{
		return false;
	}

	UEFAsyncForLoopDelay* ForLoop = UEFAsyncForLoopDelay::EFForLoopWithDelay(2, 5, GameInstance, 0.05f);
	if (!TestNotNull(TEXT("the loop starts"), ForLoop))
	{
		return false;
	}

	State->Pins.Reset(NewObject<UEFAsyncTestListener>());
	State->Node.Reset(ForLoop);
	ForLoop->Loop.AddDynamic(State->Pins.Get(), &UEFAsyncTestListener::HandleIndex);
	ForLoop->Completed.AddDynamic(State->Pins.Get(), &UEFAsyncTestListener::HandleCompletedIndex);
	ForLoop->Activate();

	ADD_LATENT_AUTOMATION_COMMAND(FEFWaitUntilCommand(this, TEXT("the loop to complete"), [State]()
	{
		EFAsyncLibraryTests::TickTimers(*State->Game);
		return State->Pins->CompletedIndices.Num() > 0;
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FEFRunLatentCommand([this, State]()
	{
		TestTrue(TEXT("Loop fired for 2, 3 and 4, in order"), State->Pins->Indices == TArray<int32>({ 2, 3, 4 }));
		TestTrue(TEXT("Completed fired once, with LastIndex"), State->Pins->CompletedIndices == TArray<int32>({ 5 }));
		TestTrue(TEXT("the node finished"), State->Node->IsFinished());

		State->Node.Reset();
		State->Game.Reset();
	}));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
