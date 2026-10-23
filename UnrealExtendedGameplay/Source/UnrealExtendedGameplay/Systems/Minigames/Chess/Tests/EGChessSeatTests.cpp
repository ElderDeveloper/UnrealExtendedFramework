// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Player/EGChessPlayerComponent.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Table/EGChessSeatComponent.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Table/EGChessTableActor.h"
#include "UObject/UnrealType.h"

/** Deliver replicated fields independently, as their RepNotifies can arrive over the network. */
struct FEGChessSeatTestAccess
{
	static void ReceiveOccupant(UEGChessSeatComponent* Seat, const FEGChessSeatOccupant& Occupant)
	{
		Seat->Occupant = Occupant;
		Seat->OnRep_Occupant();
	}

	static void ReceiveNone(UEGChessSeatComponent* Seat)
	{
		Seat->AnimState.Action = EEGChessSeatAction::None;
		++Seat->AnimState.Sequence;
		Seat->OnRep_AnimState();
	}

	static void FinishExit(UEGChessSeatComponent* Seat)
	{
		Seat->FinishSequence();
	}
};

namespace EGChessSeatTests
{
	/** Use the existing editor world: never create a second temporary world in the editor. */
	struct FFixture
	{
		UWorld* World = nullptr;
		AEGChessTableActor* Table = nullptr;
		TArray<TWeakObjectPtr<AActor>> Actors;

		FFixture()
		{
			if (GEngine)
			{
				for (const FWorldContext& Context : GEngine->GetWorldContexts())
				{
					if (Context.WorldType == EWorldType::Editor && Context.World())
					{
						World = Context.World();
						break;
					}
				}
			}
			Table = Spawn<AEGChessTableActor>();
		}

		~FFixture()
		{
			if (IsValid(Table))
			{
				Table->SeatA->ClearOccupant();
				Table->SeatB->ClearOccupant();
			}
			for (int32 Index = Actors.Num() - 1; Index >= 0; --Index)
			{
				if (AActor* Actor = Actors[Index].Get())
				{
					Actor->Destroy();
				}
			}
		}

		template <typename T>
		T* Spawn()
		{
			if (!World)
			{
				return nullptr;
			}
			FActorSpawnParameters Params;
			Params.ObjectFlags = RF_Transient;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			T* Actor = World->SpawnActor<T>(FVector(500000.0, Actors.Num() * 500.0, 50000.0), FRotator::ZeroRotator, Params);
			Actors.Add(Actor);
			return Actor;
		}

		ACharacter* SeatBody(UEGChessSeatComponent* Seat)
		{
			ACharacter* Pawn = Spawn<ACharacter>();
			if (Pawn)
			{
				Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
				FEGChessSeatOccupant Occupant;
				Occupant.Pawn = Pawn;
				Occupant.bAI = true;
				Seat->SetOccupant(Occupant);
				Seat->BeginEnter(EEGChessEntrySide::Left, Pawn->GetActorTransform());
			}
			return Pawn;
		}
	};

	void CheckReleased(FAutomationTestBase& Test, ACharacter* Pawn, AEGChessTableActor* Table)
	{
		Test.TestTrue(TEXT("Released pawn replicates movement"), Pawn->IsReplicatingMovement());
		Test.TestEqual(TEXT("Released pawn can walk"), Pawn->GetCharacterMovement()->MovementMode.GetValue(), MOVE_Walking);
		Test.TestFalse(TEXT("Server movement correction is restored"), Pawn->GetCharacterMovement()->bIgnoreClientMovementErrorChecksAndCorrection);
		Test.TestFalse(TEXT("Capsule no longer ignores the chess table"), Pawn->GetCapsuleComponent()->GetMoveIgnoreActors().Contains(Table));
		Test.TestTrue(TEXT("Capsule collision remains enabled"), Pawn->GetCapsuleComponent()->GetCollisionEnabled() != ECollisionEnabled::NoCollision);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGChessEmptyOccupantCleanupTest,
	"UnrealExtendedGameplay.Chess.Seat.EmptyOccupantBeforeAnimation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGChessEmptyOccupantCleanupTest::RunTest(const FString& Parameters)
{
	EGChessSeatTests::FFixture Fixture;
	if (!TestNotNull(TEXT("Existing editor world supports the fixture"), Fixture.Table)) return false;
	UEGChessSeatComponent* Seat = Fixture.Table->SeatA;
	ACharacter* Pawn = Fixture.SeatBody(Seat);
	if (!TestNotNull(TEXT("Fixture pawn"), Pawn)) return false;
	TestFalse(TEXT("Seat owns movement replication while entering"), Pawn->IsReplicatingMovement());
	TestTrue(TEXT("Entry sequence is active"), Seat->IsSequenceActive());

	// Reproduce a cleared occupant arriving while the previous Entering state is still present.
	FEGChessSeatTestAccess::ReceiveOccupant(Seat, FEGChessSeatOccupant());
	EGChessSeatTests::CheckReleased(*this, Pawn, Fixture.Table);
	TestFalse(TEXT("An empty seat cannot continue moving the old body"), Seat->IsSequenceActive());
	TestFalse(TEXT("Empty seat stops ticking"), Seat->IsComponentTickEnabled());
	const FVector WalkingLocation = Pawn->GetActorLocation() + FVector(300.0, 100.0, 0.0);
	Pawn->SetActorLocation(WalkingLocation);
	Seat->TickComponent(1.0f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("A stale seat tick does not pull the released pawn back"), Pawn->GetActorLocation().Equals(WalkingLocation));

	// A later None notification must not overwrite a new movement mode after release.
	Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	FEGChessSeatTestAccess::ReceiveNone(Seat);
	TestEqual(TEXT("Repeated cleanup leaves subsequent movement changes alone"), Pawn->GetCharacterMovement()->MovementMode.GetValue(), MOVE_Flying);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGChessSeatUnregisterCleanupTest,
	"UnrealExtendedGameplay.Chess.Seat.UnregisterDuringExit", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGChessSeatUnregisterCleanupTest::RunTest(const FString& Parameters)
{
	EGChessSeatTests::FFixture Fixture;
	if (!TestNotNull(TEXT("Fixture table"), Fixture.Table)) return false;
	UEGChessSeatComponent* Seat = Fixture.Table->SeatA;
	ACharacter* Pawn = Fixture.SeatBody(Seat);
	if (!TestNotNull(TEXT("Fixture pawn"), Pawn)) return false;
	Seat->BeginExit(EEGChessEntrySide::Left, false);
	Seat->UnregisterComponent();
	EGChessSeatTests::CheckReleased(*this, Pawn, Fixture.Table);
	TestFalse(TEXT("Unregistered seat has no active sequence"), Seat->IsSequenceActive());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGChessSeatReplacementCleanupTest,
	"UnrealExtendedGameplay.Chess.Seat.OccupantReplacement", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGChessSeatReplacementCleanupTest::RunTest(const FString& Parameters)
{
	EGChessSeatTests::FFixture Fixture;
	if (!TestNotNull(TEXT("Fixture table"), Fixture.Table)) return false;
	UEGChessSeatComponent* Seat = Fixture.Table->SeatA;
	ACharacter* Previous = Fixture.SeatBody(Seat);
	ACharacter* Next = Fixture.Spawn<ACharacter>();
	if (!TestNotNull(TEXT("Previous pawn"), Previous) || !TestNotNull(TEXT("Replacement pawn"), Next)) return false;
	FEGChessSeatOccupant Replacement;
	Replacement.Pawn = Next;
	Replacement.bAI = true;
	FEGChessSeatTestAccess::ReceiveOccupant(Seat, Replacement);
	EGChessSeatTests::CheckReleased(*this, Previous, Fixture.Table);
	TestFalse(TEXT("Replacement pawn is prepared for the current seat state"), Next->IsReplicatingMovement());
	Seat->ClearOccupant();
	EGChessSeatTests::CheckReleased(*this, Next, Fixture.Table);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGChessSimultaneousExitTest,
	"UnrealExtendedGameplay.Chess.Seat.SimultaneousExit", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGChessSimultaneousExitTest::RunTest(const FString& Parameters)
{
	EGChessSeatTests::FFixture Fixture;
	if (!TestNotNull(TEXT("Fixture table"), Fixture.Table)) return false;
	FObjectProperty* ActiveTableProperty = FindFProperty<FObjectProperty>(UEGChessPlayerComponent::StaticClass(), TEXT("ActiveTable"));
	if (!TestNotNull(TEXT("Replicated player table property"), ActiveTableProperty)) return false;
	ACharacter* Pawns[2];
	APlayerController* Controllers[2];
	UEGChessPlayerComponent* Players[2];
	UEGChessSeatComponent* Seats[2] = { Fixture.Table->SeatA, Fixture.Table->SeatB };
	for (int32 Index = 0; Index < 2; ++Index)
	{
		Pawns[Index] = Fixture.Spawn<ACharacter>();
		Controllers[Index] = Fixture.Spawn<APlayerController>();
		APlayerState* State = Fixture.Spawn<APlayerState>();
		if (!Pawns[Index] || !Controllers[Index] || !State) { AddError(TEXT("Could not spawn player fixture")); return false; }
		Controllers[Index]->PlayerState = State;
		State->SetOwner(Controllers[Index]);
		Players[Index] = UEGChessPlayerComponent::FindOrAdd(Controllers[Index]);
		if (!TestNotNull(TEXT("Server player component"), Players[Index])) return false;
	}

	// Re-seat and leave again, reversing completion order on the second round.
	for (int32 Round = 0; Round < 2; ++Round)
	{
		for (int32 Index = 0; Index < 2; ++Index)
		{
			FEGChessSeatOccupant Occupant;
			Occupant.Pawn = Pawns[Index];
			Occupant.PlayerState = Controllers[Index]->PlayerState;
			Seats[Index]->SetOccupant(Occupant);
			Seats[Index]->SnapSeated();
			// Set only replicated server state; this fixture has no local player's UI/input stack.
			ActiveTableProperty->SetObjectPropertyValue_InContainer(Players[Index], Fixture.Table);
		}
		Fixture.Table->HandleLeaveSeat(Controllers[0]);
		const uint8 Sequence = Seats[0]->GetAnimState().Sequence;
		Fixture.Table->HandleLeaveSeat(Controllers[0]);
		TestEqual(TEXT("Duplicate leave does not restart the exit"), Seats[0]->GetAnimState().Sequence, Sequence);
		Fixture.Table->HandleLeaveSeat(Controllers[1]);
		for (int32 Index = 0; Index < 2; ++Index)
		{
			TestTrue(TEXT("Server keeps controls locked until exit completes"), Players[Index]->GetTable() == Fixture.Table);
			TestFalse(TEXT("Exit sequence still owns pawn movement"), Pawns[Index]->IsReplicatingMovement());
		}
		for (int32 Offset = 0; Offset < 2; ++Offset)
		{
			const int32 Index = (Round + Offset) % 2;
			FEGChessSeatTestAccess::FinishExit(Seats[Index]);
			EGChessSeatTests::CheckReleased(*this, Pawns[Index], Fixture.Table);
			TestNull(TEXT("Controls are released after server pawn cleanup"), Players[Index]->GetTable());
			TestFalse(TEXT("Completed exit frees its seat"), Seats[Index]->IsOccupied());
			if (Offset == 0)
			{
				TestTrue(TEXT("Other player's controls remain locked during their exit"), Players[1 - Index]->GetTable() == Fixture.Table);
			}
		}
	}
	return true;
}

#endif
