// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGStateMachineTestTypes.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "TimerManager.h"
#include "UnrealExtendedGameplay/AI/StateMachine/EGStateMachineComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace EGStateMachineTest
{
	/**
	 * Minimal game world with one authoritative actor carrying a machine.
	 *
	 * A real world (rather than a bare NewObject owner) is needed because states resolve their
	 * world through the component for timers, and because BeginPlay drives registration.
	 */
	struct FScope
	{
		UWorld* World = nullptr;
		AActor* Owner = nullptr;
		UEGStateMachineComponent* Machine = nullptr;

		FScope()
		{
			FEGStateMachineTestLog::Reset();

			World = UWorld::CreateWorld(EWorldType::Game, false);
			FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
			WorldContext.SetCurrentWorld(World);

			World->InitializeActorsForPlay(FURL());

			// UWorld::BeginPlay() only flips the flag through a GameMode, and this world has none.
			// The flag has to be set by hand or the spawned actor never begins play — and a
			// component only receives BeginPlay when its owner has already begun play
			// (AActor::HandleRegisterComponentWithWorld).
			World->SetBegunPlay(true);

			Owner = World->SpawnActor<AActor>();
			if (!Owner->HasActorBegunPlay())
			{
				Owner->DispatchBeginPlay();
			}

			Machine = NewObject<UEGStateMachineComponent>(Owner);
			Machine->SetAutoStart(false);
		}

		~FScope()
		{
			if (World)
			{
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
			}
		}

		/** Adds a definition instance before the component is registered, mirroring constructor-time authoring. */
		template <typename StateType>
		StateType* AddState()
		{
			StateType* State = NewObject<StateType>(Machine);
			Machine->AddStateDefinition(State);
			return State;
		}

		/** Registers the component, which runs BeginPlay and therefore registers the definitions. */
		void Begin()
		{
			Machine->RegisterComponent();
		}

		/**
		 * FTimerManager parks a timer that was set outside its own tick in a pending set and only
		 * promotes it to the active heap at the end of the next tick, and it refuses to tick more
		 * than once per frame. So advancing time in a test means bumping the frame counter and
		 * ticking more than once, or a freshly set timer can never reach its expiry.
		 */
		/** Turns this scope's machine into a simulated proxy — a client mirror. */
		void MakeClient()
		{
			Owner->SetRole(ROLE_SimulatedProxy);
		}

		void AdvanceTime(float Seconds, int32 Steps = 2)
		{
			const float StepSeconds = Seconds / static_cast<float>(FMath::Max(Steps, 1));
			for (int32 Step = 0; Step < Steps; ++Step)
			{
				++GFrameCounter;
				World->GetTimerManager().Tick(StepSeconds);
			}
		}
	};
}

// -----------------------------------------------------------------------------
// Test 1 — Registration
// -----------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGStateMachineRegistrationTest,
	"UnrealExtendedGameplay.StateMachine.Registration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGStateMachineRegistrationTest::RunTest(const FString& Parameters)
{
	EGStateMachineTest::FScope Scope;
	UEGTestState_A* StateA = Scope.AddState<UEGTestState_A>();
	Scope.AddState<UEGTestState_B>();
	Scope.Begin();

	TestEqual(TEXT("Definitions are registered at BeginPlay"), Scope.Machine->GetRegisteredStates().Num(), 2);
	TestTrue(TEXT("Registered state is retrievable by class"), Scope.Machine->GetState(UEGTestState_A::StaticClass()) == StateA);
	TestEqual(TEXT("Registration raises OnGiven once"), StateA->GivenCount, 1);

	// Rule 3: instances belong to this component alone.
	TestTrue(TEXT("Instance is outered to its own machine"), StateA->GetOuter() == Scope.Machine);
	TestTrue(TEXT("Instance knows its machine"), StateA->GetOwningMachine() == Scope.Machine);

	// Rule 8 / one-instance invariant: a duplicate never displaces the incumbent.
	UEGTestState_A* Duplicate = NewObject<UEGTestState_A>(Scope.Machine);
	UEGState* Resolved = Scope.Machine->AddStateDefinition(Duplicate);
	TestTrue(TEXT("Duplicate class registration returns the incumbent"), Resolved == StateA);
	TestEqual(TEXT("Duplicate registration does not grow the registry"), Scope.Machine->GetRegisteredStates().Num(), 2);

	// GiveState on an already-registered class is idempotent, not a second instance.
	TestTrue(TEXT("GiveState returns the existing instance"), Scope.Machine->GiveState(UEGTestState_A::StaticClass()) == StateA);
	TestEqual(TEXT("GiveState does not grow the registry"), Scope.Machine->GetRegisteredStates().Num(), 2);

	return true;
}

// -----------------------------------------------------------------------------
// Test 2 — The one-instance invariant, and the stack-growth regression it fixes
// -----------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGStateMachineInvariantTest,
	"UnrealExtendedGameplay.StateMachine.Invariant",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGStateMachineInvariantTest::RunTest(const FString& Parameters)
{
	EGStateMachineTest::FScope Scope;
	Scope.AddState<UEGTestState_Brain>();
	Scope.AddState<UEGTestState_A>();
	Scope.AddState<UEGTestState_B>();
	Scope.Begin();
	Scope.Machine->Start();

	TestTrue(TEXT("Machine starts in the brain"), Scope.Machine->IsCurrentState(UEGTestState_Brain::StaticClass()));

	TestTrue(TEXT("First push succeeds"), Scope.Machine->PushState(UEGTestState_A::StaticClass()));
	TestFalse(TEXT("Pushing the active class again is refused"), Scope.Machine->PushState(UEGTestState_A::StaticClass()));
	TestEqual(TEXT("Refused push leaves the stack untouched"), Scope.Machine->GetStackDepth(), 1);

	TestTrue(TEXT("Pushing a different class succeeds"), Scope.Machine->PushState(UEGTestState_B::StaticClass()));
	TestFalse(TEXT("Pushing a class already paused in the stack is refused"), Scope.Machine->PushState(UEGTestState_A::StaticClass()));
	TestEqual(TEXT("Stack still holds brain + A"), Scope.Machine->GetStackDepth(), 2);

	TestEqual(TEXT("Refusal reports AlreadyPresent"),
		static_cast<int32>(Scope.Machine->TryPushState(UEGTestState_A::StaticClass())),
		static_cast<int32>(EEGStateTransitionResult::AlreadyPresent));

	// The DevilOfPlague regression: a brain alternating A/B via push-without-pop grew the stack
	// without bound. The invariant has to hold the line no matter how long that runs.
	Scope.Machine->PopToBrain();
	for (int32 Iteration = 0; Iteration < 1000; ++Iteration)
	{
		Scope.Machine->PushState(UEGTestState_A::StaticClass());
		Scope.Machine->PushState(UEGTestState_B::StaticClass());
	}

	TestEqual(TEXT("Stack holds exactly brain + A after 1000 alternations"), Scope.Machine->GetStackDepth(), 2);
	TestTrue(TEXT("B is the active state"), Scope.Machine->IsCurrentState(UEGTestState_B::StaticClass()));

	return true;
}

// -----------------------------------------------------------------------------
// Test 3 — Lifecycle ordering
// -----------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGStateMachineLifecycleOrderTest,
	"UnrealExtendedGameplay.StateMachine.LifecycleOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGStateMachineLifecycleOrderTest::RunTest(const FString& Parameters)
{
	EGStateMachineTest::FScope Scope;
	Scope.AddState<UEGTestState_Brain>();
	UEGTestState_A* StateA = Scope.AddState<UEGTestState_A>();
	UEGTestState_B* StateB = Scope.AddState<UEGTestState_B>();
	UEGTestState_C* StateC = Scope.AddState<UEGTestState_C>();
	Scope.Begin();

	// Given precedes any Enter.
	TestEqual(TEXT("All states are given before anything enters"), FEGStateMachineTestLog::Join(),
		FString(TEXT("EGTestState_Brain:Given|EGTestState_A:Given|EGTestState_B:Given|EGTestState_C:Given")));

	FEGStateMachineTestLog::Reset();
	Scope.Machine->Start();
	TestEqual(TEXT("Start enters the brain"), FEGStateMachineTestLog::Join(), FString(TEXT("EGTestState_Brain:Enter")));

	// Push: the displaced state pauses, it does not exit.
	FEGStateMachineTestLog::Reset();
	Scope.Machine->PushState(UEGTestState_A::StaticClass());
	TestEqual(TEXT("Push pauses the old state then enters the new one"), FEGStateMachineTestLog::Join(),
		FString(TEXT("EGTestState_Brain:Pause|EGTestState_A:Enter")));

	FEGStateMachineTestLog::Reset();
	Scope.Machine->PushState(UEGTestState_B::StaticClass());
	TestEqual(TEXT("A second push stacks the same way"), FEGStateMachineTestLog::Join(),
		FString(TEXT("EGTestState_A:Pause|EGTestState_B:Enter")));
	TestEqual(TEXT("A was not exited by the push"), StateA->ExitCount, 0);

	// Pop: the top exits, the one beneath resumes rather than re-entering.
	FEGStateMachineTestLog::Reset();
	Scope.Machine->PopState();
	TestEqual(TEXT("Pop exits the top then resumes the one below"), FEGStateMachineTestLog::Join(),
		FString(TEXT("EGTestState_B:Exit|EGTestState_A:Resume")));
	TestEqual(TEXT("A entered exactly once across the whole sequence"), StateA->EnterCount, 1);

	// Switch: current exits, the stack unwinds bottom-out, then the new state enters.
	Scope.Machine->PushState(UEGTestState_B::StaticClass());
	FEGStateMachineTestLog::Reset();
	Scope.Machine->SwitchState(UEGTestState_C::StaticClass());
	TestEqual(TEXT("Switch exits current, unwinds the stack, then enters"), FEGStateMachineTestLog::Join(),
		FString(TEXT("EGTestState_B:Exit|EGTestState_A:Exit|EGTestState_Brain:Exit|EGTestState_C:Enter")));
	TestEqual(TEXT("Switch leaves an empty stack"), Scope.Machine->GetStackDepth(), 0);
	TestTrue(TEXT("C is active"), StateC->IsStateActive());
	TestFalse(TEXT("A is no longer active"), StateA->IsStateActive());
	TestFalse(TEXT("B is no longer active"), StateB->IsStateActive());

	return true;
}

// -----------------------------------------------------------------------------
// Test 4 — Transition policy
// -----------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGStateMachinePolicyTest,
	"UnrealExtendedGameplay.StateMachine.TransitionPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGStateMachinePolicyTest::RunTest(const FString& Parameters)
{
	EGStateMachineTest::FScope Scope;
	Scope.AddState<UEGTestState_Brain>();
	Scope.AddState<UEGTestState_A>();
	UEGTestState_Veto* Veto = Scope.AddState<UEGTestState_Veto>();
	Scope.AddState<UEGTestState_Blocker>();
	Scope.Begin();
	Scope.Machine->Start();

	// CanEnterState veto.
	TestFalse(TEXT("A vetoing state refuses entry"), Scope.Machine->PushState(UEGTestState_Veto::StaticClass()));
	TestEqual(TEXT("Refusal reports VetoedByState"),
		static_cast<int32>(Scope.Machine->TryPushState(UEGTestState_Veto::StaticClass())),
		static_cast<int32>(EEGStateTransitionResult::VetoedByState));

	Veto->bAllowEntry = true;
	TestTrue(TEXT("The same state enters once it stops vetoing"), Scope.Machine->PushState(UEGTestState_Veto::StaticClass()));
	Scope.Machine->PopToBrain();

	// bBlocksInterrupts: normal pushes still work, force-pushes do not.
	TestTrue(TEXT("Blocker enters"), Scope.Machine->PushState(UEGTestState_Blocker::StaticClass()));
	TestFalse(TEXT("Force-push over a blocker is refused"), Scope.Machine->ForcePushState(UEGTestState_A::StaticClass()));
	TestEqual(TEXT("Refusal reports BlockedByActiveState"),
		static_cast<int32>(Scope.Machine->TryPushState(UEGTestState_A::StaticClass(), nullptr, /*bForce*/ true)),
		static_cast<int32>(EEGStateTransitionResult::BlockedByActiveState));
	TestTrue(TEXT("A normal push over a blocker still succeeds"), Scope.Machine->PushState(UEGTestState_A::StaticClass()));
	Scope.Machine->PopToBrain();

	// The interrupt idiom: bPushWhenGiven + bRemoveWhenExited.
	FEGStateMachineTestLog::Reset();
	UEGState* Interrupt = Scope.Machine->GiveState(UEGTestState_Interrupt::StaticClass());
	TestNotNull(TEXT("Giving an interrupt state returns it"), Interrupt);
	TestTrue(TEXT("Giving it pushes it immediately"), Scope.Machine->IsCurrentState(UEGTestState_Interrupt::StaticClass()));
	TestEqual(TEXT("Give registers, pauses the incumbent, then enters"), FEGStateMachineTestLog::Join(),
		FString(TEXT("EGTestState_Interrupt:Given|EGTestState_Brain:Pause|EGTestState_Interrupt:Enter")));

	FEGStateMachineTestLog::Reset();
	Scope.Machine->PopState();
	TestEqual(TEXT("Exit is followed by unregistration"), FEGStateMachineTestLog::Join(),
		FString(TEXT("EGTestState_Interrupt:Exit|EGTestState_Interrupt:Removed|EGTestState_Brain:Resume")));
	TestFalse(TEXT("The interrupt state is no longer registered"), Scope.Machine->HasState(UEGTestState_Interrupt::StaticClass()));

	// It can be given again — the rollback path matters here: a dormant registration would make
	// every later give a silent no-op.
	TestNotNull(TEXT("A second give works"), Scope.Machine->GiveState(UEGTestState_Interrupt::StaticClass()));
	TestTrue(TEXT("Second give fires again"), Scope.Machine->IsCurrentState(UEGTestState_Interrupt::StaticClass()));

	return true;
}

// -----------------------------------------------------------------------------
// Test 5 — Brain contract
// -----------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGStateMachineBrainTest,
	"UnrealExtendedGameplay.StateMachine.Brain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGStateMachineBrainTest::RunTest(const FString& Parameters)
{
	// Zero brains refuses to start.
	{
		EGStateMachineTest::FScope Scope;
		Scope.AddState<UEGTestState_A>();
		Scope.Begin();

		AddExpectedErrorPlain(TEXT("No brain state registered"), EAutomationExpectedErrorFlags::Contains, 1);
		Scope.Machine->Start();

		TestFalse(TEXT("A machine with no brain does not run"), Scope.Machine->IsRunning());
		TestNull(TEXT("A machine with no brain has no active state"), Scope.Machine->GetCurrentState());
	}

	// Two brains refuses to start: the ambiguity is a hard failure, not a silent pick.
	{
		EGStateMachineTest::FScope Scope;
		Scope.AddState<UEGTestState_Brain>();
		Scope.AddState<UEGTestState_BrainSecond>();
		Scope.Begin();

		AddExpectedErrorPlain(TEXT("Multiple brain states registered"), EAutomationExpectedErrorFlags::Contains, 1);
		AddExpectedErrorPlain(TEXT("No brain state registered"), EAutomationExpectedErrorFlags::Contains, 1);
		Scope.Machine->Start();

		TestFalse(TEXT("A machine with two brains does not run"), Scope.Machine->IsRunning());
	}

	// Returning null is a no-op, not a transition.
	{
		EGStateMachineTest::FScope Scope;
		UEGTestState_Brain* Brain = Scope.AddState<UEGTestState_Brain>();
		Scope.AddState<UEGTestState_A>();
		Scope.Begin();
		Scope.Machine->Start();

		TestTrue(TEXT("The brain is the active state"), Scope.Machine->IsCurrentState(UEGTestState_Brain::StaticClass()));
		TestEqual(TEXT("Entering the brain evaluates once"), Brain->EvaluateCount, 1);
		TestEqual(TEXT("A null decision changes nothing"), Scope.Machine->GetStackDepth(), 0);
	}

	// A decision pushes, and popping the child resumes the brain, which decides again.
	{
		EGStateMachineTest::FScope Scope;
		UEGTestState_Brain* Brain = Scope.AddState<UEGTestState_Brain>();
		UEGTestState_A* StateA = Scope.AddState<UEGTestState_A>();
		Scope.Begin();

		Brain->DesiredStateClass = UEGTestState_A::StaticClass();
		Scope.Machine->Start();

		TestTrue(TEXT("The brain's decision is entered"), Scope.Machine->IsCurrentState(UEGTestState_A::StaticClass()));
		TestEqual(TEXT("The brain remains the stack floor"), Scope.Machine->GetStackDepth(), 1);
		TestEqual(TEXT("A entered once"), StateA->EnterCount, 1);

		// This is the event-driven half of the loop: no tick involved.
		Scope.Machine->PopState();
		TestEqual(TEXT("Resuming the brain re-evaluates"), Brain->EvaluateCount, 2);
		TestEqual(TEXT("Re-evaluation re-enters the child"), StateA->EnterCount, 2);
		TestTrue(TEXT("A is active again"), Scope.Machine->IsCurrentState(UEGTestState_A::StaticClass()));
	}

	// The brain is the floor: it cannot be popped away.
	{
		EGStateMachineTest::FScope Scope;
		UEGTestState_Brain* Brain = Scope.AddState<UEGTestState_Brain>();
		Scope.AddState<UEGTestState_A>();
		Scope.Begin();
		Scope.Machine->Start();

		TestFalse(TEXT("Popping the brain is refused"), Scope.Machine->PopState());
		TestTrue(TEXT("The brain is still active"), Scope.Machine->IsCurrentState(UEGTestState_Brain::StaticClass()));
		TestEqual(TEXT("The brain was not exited"), Brain->ExitCount, 0);
	}

	// PopToBrain has to terminate even though resuming the brain makes it push again. Unwinding
	// one PopState at a time would resume the brain between pops and loop forever.
	{
		EGStateMachineTest::FScope Scope;
		UEGTestState_Brain* Brain = Scope.AddState<UEGTestState_Brain>();
		UEGTestState_A* StateA = Scope.AddState<UEGTestState_A>();
		Scope.AddState<UEGTestState_B>();
		Scope.Begin();

		Brain->DesiredStateClass = UEGTestState_A::StaticClass();
		Scope.Machine->Start();
		Scope.Machine->PushState(UEGTestState_B::StaticClass());
		TestEqual(TEXT("Brain, A and B are live"), Scope.Machine->GetStackDepth(), 2);

		Scope.Machine->PopToBrain();

		// One unwind, one resume, and the brain's own decision puts its child back.
		TestEqual(TEXT("Unwinding leaves exactly the brain plus its decision"), Scope.Machine->GetStackDepth(), 1);
		TestTrue(TEXT("The brain re-pushed its child on resume"), Scope.Machine->IsCurrentState(UEGTestState_A::StaticClass()));
		TestEqual(TEXT("The child exited once during the unwind"), StateA->ExitCount, 1);
		TestEqual(TEXT("The child entered twice overall"), StateA->EnterCount, 2);
	}

	// Exclusive mode: the previous child is dropped rather than stacked on.
	{
		EGStateMachineTest::FScope Scope;
		UEGTestState_Brain* Brain = Scope.AddState<UEGTestState_Brain>();
		UEGTestState_A* StateA = Scope.AddState<UEGTestState_A>();
		Scope.AddState<UEGTestState_B>();
		Scope.Begin();

		Brain->SetExclusive(true);
		Brain->DesiredStateClass = UEGTestState_A::StaticClass();
		Scope.Machine->Start();
		TestTrue(TEXT("Exclusive brain enters its first child"), Scope.Machine->IsCurrentState(UEGTestState_A::StaticClass()));

		Brain->DesiredStateClass = UEGTestState_B::StaticClass();
		Scope.Machine->GetBrainState();
		Brain->EvaluateNow();

		TestTrue(TEXT("Exclusive brain swaps to the new child"), Scope.Machine->IsCurrentState(UEGTestState_B::StaticClass()));
		TestEqual(TEXT("The previous child exited"), StateA->ExitCount, 1);
		TestEqual(TEXT("Only the brain is below the child"), Scope.Machine->GetStackDepth(), 1);
	}

	return true;
}

// -----------------------------------------------------------------------------
// Test 6 — Stack depth
// -----------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGStateMachineStackDepthTest,
	"UnrealExtendedGameplay.StateMachine.StackDepth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGStateMachineStackDepthTest::RunTest(const FString& Parameters)
{
	EGStateMachineTest::FScope Scope;
	Scope.AddState<UEGTestState_Brain>();
	Scope.AddState<UEGTestState_A>();
	Scope.AddState<UEGTestState_B>();
	Scope.AddState<UEGTestState_C>();
	Scope.AddState<UEGTestState_Blocker>();
	Scope.AddState<UEGTestState_Veto>();
	Scope.AddState<UEGTestState_Timer>();
	Scope.Begin();
	Scope.Machine->Start();

	// Default MaxStackDepth is 5 paused entries: brain + A + B + C + Blocker, with Timer active.
	TestTrue(TEXT("Push 1"), Scope.Machine->PushState(UEGTestState_A::StaticClass()));
	TestTrue(TEXT("Push 2"), Scope.Machine->PushState(UEGTestState_B::StaticClass()));
	TestTrue(TEXT("Push 3"), Scope.Machine->PushState(UEGTestState_C::StaticClass()));
	TestTrue(TEXT("Push 4"), Scope.Machine->PushState(UEGTestState_Blocker::StaticClass()));
	TestTrue(TEXT("Push 5"), Scope.Machine->PushState(UEGTestState_Timer::StaticClass()));
	TestEqual(TEXT("Stack is at the configured maximum"), Scope.Machine->GetStackDepth(), 5);

	const TSubclassOf<UEGState> ActiveBefore = Scope.Machine->GetCurrentStateClass();

	// Let the spare state through its own veto so the only thing left to refuse it is the depth cap.
	UEGTestState_Veto* Veto = Cast<UEGTestState_Veto>(Scope.Machine->GetState(UEGTestState_Veto::StaticClass()));
	if (!TestNotNull(TEXT("Spare state is registered"), Veto))
	{
		return false;
	}
	Veto->bAllowEntry = true;

	TestFalse(TEXT("A push past the maximum is refused"), Scope.Machine->PushState(UEGTestState_Veto::StaticClass()));
	TestEqual(TEXT("Refusal reports StackFull"),
		static_cast<int32>(Scope.Machine->TryPushState(UEGTestState_Veto::StaticClass())),
		static_cast<int32>(EEGStateTransitionResult::StackFull));
	TestEqual(TEXT("Refused push leaves depth unchanged"), Scope.Machine->GetStackDepth(), 5);
	TestTrue(TEXT("Refused push leaves the active state unchanged"), Scope.Machine->GetCurrentStateClass() == ActiveBefore);

	return true;
}

// -----------------------------------------------------------------------------
// Test 7 — Replication: mirrors converge on the authoritative snapshot
//
// Driven through ApplyNetStateSnapshot rather than a live net driver: the snapshot is the entire
// contract between the two sides, so feeding it in directly exercises the same mirroring code a
// replicated OnRep would.
// -----------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGStateMachineReplicationTest,
	"UnrealExtendedGameplay.StateMachine.Replication",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGStateMachineReplicationTest::RunTest(const FString& Parameters)
{
	EGStateMachineTest::FScope Server;
	Server.AddState<UEGTestState_Brain>();
	Server.AddState<UEGTestState_Mirrored>();
	Server.AddState<UEGTestState_MirroredSecond>();
	Server.Begin();
	Server.Machine->Start();

	// Two independent mirrors, as in a two-client session.
	EGStateMachineTest::FScope ClientA;
	ClientA.AddState<UEGTestState_Brain>();
	UEGTestState_Mirrored* ClientMirrored = ClientA.AddState<UEGTestState_Mirrored>();
	ClientA.AddState<UEGTestState_MirroredSecond>();
	ClientA.Begin();
	ClientA.MakeClient();

	EGStateMachineTest::FScope ClientB;
	ClientB.AddState<UEGTestState_Brain>();
	ClientB.AddState<UEGTestState_Mirrored>();
	ClientB.AddState<UEGTestState_MirroredSecond>();
	ClientB.Begin();
	ClientB.MakeClient();

	// A client never decides anything.
	TestFalse(TEXT("A client push is refused"), ClientA.Machine->PushState(UEGTestState_Mirrored::StaticClass()));
	TestEqual(TEXT("Refusal reports NoAuthority"),
		static_cast<int32>(ClientA.Machine->TryPushState(UEGTestState_Mirrored::StaticClass())),
		static_cast<int32>(EEGStateTransitionResult::NoAuthority));
	TestFalse(TEXT("A client pop is refused"), ClientA.Machine->PopState());

	// Server pushes; both mirrors follow.
	Server.Machine->PushState(UEGTestState_Mirrored::StaticClass(), Server.Owner);
	ClientA.Machine->ApplyNetStateSnapshot(Server.Machine->GetNetState());
	ClientB.Machine->ApplyNetStateSnapshot(Server.Machine->GetNetState());

	TestTrue(TEXT("Mirror A matches the server"), ClientA.Machine->IsCurrentState(UEGTestState_Mirrored::StaticClass()));
	TestTrue(TEXT("Mirror B matches the server"), ClientB.Machine->IsCurrentState(UEGTestState_Mirrored::StaticClass()));
	TestEqual(TEXT("Mirror A reproduces the stack depth"), ClientA.Machine->GetStackDepth(), Server.Machine->GetStackDepth());
	TestTrue(TEXT("The context actor replicates with the stack"), ClientA.Machine->GetContextActor() == Server.Owner);
	TestEqual(TEXT("The mirrored state entered locally"), ClientMirrored->EnterCount, 1);

	// Deeper push: the displaced mirrored state pauses rather than exiting.
	Server.Machine->PushState(UEGTestState_MirroredSecond::StaticClass());
	ClientA.Machine->ApplyNetStateSnapshot(Server.Machine->GetNetState());
	TestEqual(TEXT("The displaced mirror paused"), ClientMirrored->PauseCount, 1);
	TestEqual(TEXT("The displaced mirror did not exit"), ClientMirrored->ExitCount, 0);
	TestEqual(TEXT("Mirror depth follows the server"), ClientA.Machine->GetStackDepth(), 2);

	// Pop: the top exits and the one beneath resumes, not re-enters.
	Server.Machine->PopState();
	ClientA.Machine->ApplyNetStateSnapshot(Server.Machine->GetNetState());
	TestEqual(TEXT("The exposed mirror resumed"), ClientMirrored->ResumeCount, 1);
	TestEqual(TEXT("The exposed mirror did not re-enter"), ClientMirrored->EnterCount, 1);

	// Leaving a state and coming back to it lands on an identical current-plus-stack. Only Serial
	// distinguishes the two snapshots, which is what makes the property dirty enough to replicate.
	const FEGStateMachineNetState BeforeCycle = Server.Machine->GetNetState();

	Server.Machine->PopState();
	ClientA.Machine->ApplyNetStateSnapshot(Server.Machine->GetNetState());
	TestEqual(TEXT("The mirror exited the state"), ClientMirrored->ExitCount, 1);

	Server.Machine->PushState(UEGTestState_Mirrored::StaticClass());
	const FEGStateMachineNetState AfterCycle = Server.Machine->GetNetState();

	TestTrue(TEXT("The cycle returns to the same current state"),
		AfterCycle.CurrentStateClass == BeforeCycle.CurrentStateClass);
	TestEqual(TEXT("...with the same stack shape"), AfterCycle.StackClasses.Num(), BeforeCycle.StackClasses.Num());
	TestNotEqual(TEXT("...but a different snapshot, so the change still replicates"),
		static_cast<int32>(AfterCycle.Serial), static_cast<int32>(BeforeCycle.Serial));

	ClientA.Machine->ApplyNetStateSnapshot(AfterCycle);
	TestEqual(TEXT("The mirror re-entered the state"), ClientMirrored->EnterCount, 2);

	// Convergence from an arbitrary starting point: a late joiner gets the same result in one pass.
	EGStateMachineTest::FScope LateJoiner;
	LateJoiner.AddState<UEGTestState_Brain>();
	LateJoiner.AddState<UEGTestState_Mirrored>();
	LateJoiner.AddState<UEGTestState_MirroredSecond>();
	LateJoiner.Begin();
	LateJoiner.MakeClient();
	LateJoiner.Machine->ApplyNetStateSnapshot(Server.Machine->GetNetState());

	TestTrue(TEXT("A late joiner converges in one snapshot"),
		LateJoiner.Machine->IsCurrentState(Server.Machine->GetCurrentStateClass()));
	TestEqual(TEXT("A late joiner reproduces the depth"), LateJoiner.Machine->GetStackDepth(), Server.Machine->GetStackDepth());

	return true;
}

// -----------------------------------------------------------------------------
// Test 8 — Mirroring is opt-in
//
// This is the test that protects both games' existing assumption that state code only ever runs
// on the server.
// -----------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGStateMachineMirrorOptInTest,
	"UnrealExtendedGameplay.StateMachine.MirrorOptIn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGStateMachineMirrorOptInTest::RunTest(const FString& Parameters)
{
	EGStateMachineTest::FScope Server;
	Server.AddState<UEGTestState_Brain>();
	Server.AddState<UEGTestState_A>();
	Server.AddState<UEGTestState_Mirrored>();
	Server.Begin();
	Server.Machine->Start();

	EGStateMachineTest::FScope Client;
	Client.AddState<UEGTestState_Brain>();
	UEGTestState_A* PlainState = Client.AddState<UEGTestState_A>();
	UEGTestState_Mirrored* MirroredState = Client.AddState<UEGTestState_Mirrored>();
	Client.Begin();
	Client.MakeClient();

	// A state that did not opt in receives nothing client-side.
	Server.Machine->PushState(UEGTestState_A::StaticClass());
	Client.Machine->ApplyNetStateSnapshot(Server.Machine->GetNetState());

	TestTrue(TEXT("The mirror still tracks which state is current"),
		Client.Machine->IsCurrentState(UEGTestState_A::StaticClass()));
	TestEqual(TEXT("An opt-out state does not enter on a client"), PlainState->EnterCount, 0);
	TestEqual(TEXT("An opt-out state does not tick on a client"), PlainState->TickCount, 0);

	// It still holds its place, so its neighbours pause and resume in the right order.
	Server.Machine->PushState(UEGTestState_Mirrored::StaticClass());
	Client.Machine->ApplyNetStateSnapshot(Server.Machine->GetNetState());

	TestEqual(TEXT("An opt-out state does not pause on a client"), PlainState->PauseCount, 0);
	TestEqual(TEXT("The opted-in state above it entered"), MirroredState->EnterCount, 1);
	TestEqual(TEXT("The mirror stack has the placeholder underneath"), Client.Machine->GetStackDepth(), 2);

	Server.Machine->PopState();
	Client.Machine->ApplyNetStateSnapshot(Server.Machine->GetNetState());

	TestEqual(TEXT("The opted-in state exited"), MirroredState->ExitCount, 1);
	TestEqual(TEXT("The placeholder beneath is exposed without a resume call"), PlainState->ResumeCount, 0);
	TestTrue(TEXT("The placeholder is current again"), Client.Machine->IsCurrentState(UEGTestState_A::StaticClass()));

	// Ticking is gated on the same opt-in, one level further.
	Client.Machine->TickComponent(0.1f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("An opt-out state never ticks on a mirror"), PlainState->TickCount, 0);

	Server.Machine->PushState(UEGTestState_Mirrored::StaticClass());
	Client.Machine->ApplyNetStateSnapshot(Server.Machine->GetNetState());
	Client.Machine->TickComponent(0.1f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("An opted-in state ticks on a mirror"), MirroredState->TickCount, 1);

	return true;
}

// -----------------------------------------------------------------------------
// Test 9 — State-scoped timers
// -----------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGStateMachineTimerTest,
	"UnrealExtendedGameplay.StateMachine.StateTimers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGStateMachineTimerTest::RunTest(const FString& Parameters)
{
	EGStateMachineTest::FScope Scope;
	Scope.AddState<UEGTestState_Brain>();
	Scope.AddState<UEGTestState_A>();
	UEGTestState_Timer* TimerState = Scope.AddState<UEGTestState_Timer>();
	Scope.Begin();
	Scope.Machine->Start();

	// A timer set by an active state fires normally. 0.4s against a 0.1s delay leaves margin over
	// the promotion tick, so the assertion is not sitting on a float equality.
	Scope.Machine->PushState(UEGTestState_Timer::StaticClass());
	Scope.AdvanceTime(0.4f);
	TestEqual(TEXT("A timer on an active state fires"), TimerState->TimerFiredCount, 1);

	// The same timer, with the state popped before it elapses, must never fire.
	Scope.Machine->PopState();
	Scope.Machine->PushState(UEGTestState_Timer::StaticClass());
	Scope.Machine->PopState();
	Scope.AdvanceTime(1.0f);
	TestEqual(TEXT("A timer does not fire after its state exited"), TimerState->TimerFiredCount, 1);

	// And the same for a switch, which unwinds the stack rather than popping one entry.
	Scope.Machine->PushState(UEGTestState_Timer::StaticClass());
	Scope.Machine->SwitchState(UEGTestState_A::StaticClass());
	TestTrue(TEXT("The switch actually happened"), Scope.Machine->IsCurrentState(UEGTestState_A::StaticClass()));
	Scope.AdvanceTime(1.0f);
	TestEqual(TEXT("A timer does not survive a switch away"), TimerState->TimerFiredCount, 1);

	return true;
}

// -----------------------------------------------------------------------------
// Test 10 — Teardown
// -----------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGStateMachineTeardownTest,
	"UnrealExtendedGameplay.StateMachine.Teardown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGStateMachineTeardownTest::RunTest(const FString& Parameters)
{
	EGStateMachineTest::FScope Scope;
	UEGTestState_Brain* Brain = Scope.AddState<UEGTestState_Brain>();
	UEGTestState_A* StateA = Scope.AddState<UEGTestState_A>();
	UEGTestState_B* StateB = Scope.AddState<UEGTestState_B>();
	Scope.Begin();
	Scope.Machine->Start();

	Scope.Machine->PushState(UEGTestState_A::StaticClass());
	Scope.Machine->PushState(UEGTestState_B::StaticClass());
	TestEqual(TEXT("Three states are live before teardown"), Scope.Machine->GetStackDepth(), 2);

	FEGStateMachineTestLog::Reset();
	Scope.Machine->Stop();

	TestEqual(TEXT("Stop unwinds the whole stack, top-down"), FEGStateMachineTestLog::Join(),
		FString(TEXT("EGTestState_B:Exit|EGTestState_A:Exit|EGTestState_Brain:Exit")));
	TestFalse(TEXT("Machine is stopped"), Scope.Machine->IsRunning());
	TestEqual(TEXT("Stack is empty"), Scope.Machine->GetStackDepth(), 0);
	TestNull(TEXT("No active state remains"), Scope.Machine->GetCurrentState());

	TestFalse(TEXT("Brain is inactive"), Brain->IsStateActive());
	TestFalse(TEXT("A is inactive"), StateA->IsStateActive());
	TestFalse(TEXT("B is inactive"), StateB->IsStateActive());

	TestEqual(TEXT("Every state exited exactly once"), StateA->ExitCount, 1);
	TestEqual(TEXT("Instances survive teardown for reuse"), Scope.Machine->GetRegisteredStates().Num(), 3);

	// Stop is idempotent.
	FEGStateMachineTestLog::Reset();
	Scope.Machine->Stop();
	TestEqual(TEXT("A second Stop does nothing"), FEGStateMachineTestLog::Join(), FString());

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
