// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGInteractionTestTypes.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "NativeGameplayTags.h"
#include "UnrealExtendedGameplay/Systems/Interaction/EGInteractionComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_EGTest_Mode_A, "EGTest.Mode.A");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_EGTest_Mode_B, "EGTest.Mode.B");

namespace EGInteractionTest
{
	/**
	 * Minimal game world holding one authoritative interactor.
	 *
	 * A real world is required: the component traces against it, and a component only receives
	 * BeginPlay once its owner has begun play.
	 */
	struct FScope
	{
		UWorld* World = nullptr;
		AActor* Owner = nullptr;
		UEGInteractionComponent* Interaction = nullptr;

		FScope()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
			WorldContext.SetCurrentWorld(World);

			World->InitializeActorsForPlay(FURL());
			World->SetBegunPlay(true);

			Owner = World->SpawnActor<AActor>();
			if (!Owner->HasActorBegunPlay())
			{
				Owner->DispatchBeginPlay();
			}

			Interaction = NewObject<UEGInteractionComponent>(Owner);
			// The test world never ticks, so a grace period would hold focus forever.
			Interaction->LostFocusGracePeriod = 0.0f;
			Interaction->RegisterComponent();
		}

		~FScope()
		{
			if (World)
			{
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
			}
		}

		AEGTestInteractable* SpawnInteractable(const FVector& Location) const
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AEGTestInteractable* Actor = World->SpawnActor<AEGTestInteractable>(
				AEGTestInteractable::StaticClass(), FTransform(Location), SpawnParams);
			if (Actor && !Actor->HasActorBegunPlay())
			{
				Actor->DispatchBeginPlay();
			}
			return Actor;
		}
	};
}

// ---------------------------------------------------------------------
// 1 · Interface inheritance (the migration's load-bearing assumption)
// ---------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGInteractionInterfaceInheritanceTest,
	"UnrealExtendedGameplay.Interaction.InterfaceInheritance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGInteractionInterfaceInheritanceTest::RunTest(const FString& Parameters)
{
	EGInteractionTest::FScope Scope;

	AEGTestDerivedInterfaceActor* Target = Scope.World->SpawnActor<AEGTestDerivedInterfaceActor>();
	if (!Target)
	{
		AddError(TEXT("Failed to spawn the derived-interface actor"));
		return false;
	}

	// The class declares only IEGTestDerivedInteractableInterface. Everything below goes through
	// the BASE interface, which is exactly what the interaction component does at runtime.
	TestTrue(TEXT("Implements the base interface through inheritance"),
		Target->GetClass()->ImplementsInterface(UEGInteractableInterface::StaticClass()));

	TestTrue(TEXT("Execute_CanInteract dispatches through the base interface"),
		IEGInteractableInterface::Execute_CanInteract(Target, Scope.Owner));
	TestEqual(TEXT("CanInteract reached the derived implementation"), Target->CanInteractCalls, 1);

	TestTrue(TEXT("Execute_Interact dispatches through the base interface"),
		IEGInteractableInterface::Execute_Interact(Target, Scope.Owner));
	TestEqual(TEXT("Interact reached the derived implementation"), Target->InteractCalls, 1);

	const FEGInteractionPresentation Presentation =
		IEGInteractableInterface::Execute_GetInteractionPresentation(Target, Scope.Owner, FHitResult());
	TestEqual(TEXT("Presentation came from the derived implementation"),
		Presentation.Text.ToString(), FString(TEXT("DerivedPrompt")));

	Target->bAllowInteraction = false;
	TestFalse(TEXT("A refusal in the derived implementation is observed through the base"),
		IEGInteractableInterface::Execute_CanInteract(Target, Scope.Owner));

	return true;
}

// ---------------------------------------------------------------------
// 2 · Focus
// ---------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGInteractionFocusTest,
	"UnrealExtendedGameplay.Interaction.Focus",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGInteractionFocusTest::RunTest(const FString& Parameters)
{
	EGInteractionTest::FScope Scope;
	AEGTestInteractable* Target = Scope.SpawnInteractable(FVector(200.0f, 0.0f, 0.0f));

	Scope.Interaction->RefreshFocus();
	TestEqual(TEXT("Focused the interactable in front of the owner"), Scope.Interaction->GetFocusedActor(), (AActor*)Target);
	TestEqual(TEXT("Focus entered once"), Target->FocusEnterCalls, 1);

	// Re-tracing the same target must not re-fire the enter notification.
	Scope.Interaction->RefreshFocus();
	TestEqual(TEXT("Focus did not re-enter on an unchanged trace"), Target->FocusEnterCalls, 1);
	TestEqual(TEXT("Focus did not exit on an unchanged trace"), Target->FocusExitCalls, 0);

	Target->SetActorLocation(FVector(4000.0f, 0.0f, 0.0f));
	Scope.Interaction->RefreshFocus();
	TestNull(TEXT("Focus cleared once the target left the trace"), Scope.Interaction->GetFocusedActor());
	TestEqual(TEXT("Focus exited once"), Target->FocusExitCalls, 1);

	// With a grace period, a single missed trace must not drop focus.
	Target->SetActorLocation(FVector(200.0f, 0.0f, 0.0f));
	Scope.Interaction->LostFocusGracePeriod = 1.0f;
	Scope.Interaction->RefreshFocus();
	TestEqual(TEXT("Re-acquired the target"), Scope.Interaction->GetFocusedActor(), (AActor*)Target);

	Target->SetActorLocation(FVector(4000.0f, 0.0f, 0.0f));
	Scope.Interaction->RefreshFocus();
	TestEqual(TEXT("Grace period held focus through a missed trace"),
		Scope.Interaction->GetFocusedActor(), (AActor*)Target);

	return true;
}

// ---------------------------------------------------------------------
// 3 · Mode stack
// ---------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGInteractionModeStackTest,
	"UnrealExtendedGameplay.Interaction.ModeStack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGInteractionModeStackTest::RunTest(const FString& Parameters)
{
	EGInteractionTest::FScope Scope;
	AEGTestInteractable* Target = Scope.SpawnInteractable(FVector(200.0f, 0.0f, 0.0f));

	TestFalse(TEXT("Baseline mode is the empty tag"), Scope.Interaction->GetActiveMode().IsValid());
	TestTrue(TEXT("Interaction starts allowed"), Scope.Interaction->IsInteractionAllowed());

	const FEGInteractionModeHandle ToolHandle =
		Scope.Interaction->PushToolInteractionMode(TAG_EGTest_Mode_A.GetTag(), Scope.Owner, TEXT("Tool"));
	TestTrue(TEXT("Tool mode wins over the baseline"), Scope.Interaction->GetActiveMode() == TAG_EGTest_Mode_A.GetTag());

	const FEGInteractionModeHandle WorldHandle = Scope.Interaction->PushInteractionMode(
		TAG_EGTest_Mode_B.GetTag(), EGInteractionModePriority::WorldOverride, Scope.Owner, TEXT("World"));
	TestTrue(TEXT("Higher priority wins"), Scope.Interaction->GetActiveMode() == TAG_EGTest_Mode_B.GetTag());

	// Equal priority: the entry pushed last wins.
	const FEGInteractionModeHandle SecondWorldHandle = Scope.Interaction->PushInteractionMode(
		TAG_EGTest_Mode_A.GetTag(), EGInteractionModePriority::WorldOverride, Scope.Owner, TEXT("World2"));
	TestTrue(TEXT("A tie goes to the entry pushed last"), Scope.Interaction->GetActiveMode() == TAG_EGTest_Mode_A.GetTag());

	TestTrue(TEXT("Removed the tie-breaking entry"), Scope.Interaction->RemoveInteractionMode(SecondWorldHandle));
	TestTrue(TEXT("Fell back to the remaining world entry"), Scope.Interaction->GetActiveMode() == TAG_EGTest_Mode_B.GetTag());

	TestTrue(TEXT("Removed the world entry"), Scope.Interaction->RemoveInteractionMode(WorldHandle));
	TestTrue(TEXT("Fell back to the tool entry"), Scope.Interaction->GetActiveMode() == TAG_EGTest_Mode_A.GetTag());

	// A blocking entry refuses interaction outright and drops focus.
	Scope.Interaction->RefreshFocus();
	TestEqual(TEXT("Focused before blocking"), Scope.Interaction->GetFocusedActor(), (AActor*)Target);

	const FEGInteractionModeHandle BlockHandle = Scope.Interaction->PushBlockingInteractionMode(Scope.Owner, TEXT("Modal"));
	TestTrue(TEXT("Blocking entry reports blocked"), Scope.Interaction->IsBlockedByMode());
	TestFalse(TEXT("Interaction is refused while blocked"), Scope.Interaction->IsInteractionAllowed());
	TestNull(TEXT("Blocking dropped focus"), Scope.Interaction->GetFocusedActor());

	Scope.Interaction->RefreshFocus();
	TestNull(TEXT("Focus cannot be re-acquired while blocked"), Scope.Interaction->GetFocusedActor());

	Scope.Interaction->RemoveInteractionMode(BlockHandle);
	TestTrue(TEXT("Interaction is allowed again"), Scope.Interaction->IsInteractionAllowed());
	TestEqual(TEXT("Focus returned after unblocking"), Scope.Interaction->GetFocusedActor(), (AActor*)Target);

	TestEqual(TEXT("Removing by source cleared the tool entry"),
		Scope.Interaction->RemoveInteractionModesForSource(Scope.Owner), 1);
	TestFalse(TEXT("Back to the baseline mode"), Scope.Interaction->GetActiveMode().IsValid());
	TestFalse(TEXT("A stale handle removes nothing"), Scope.Interaction->RemoveInteractionMode(ToolHandle));

	return true;
}

// ---------------------------------------------------------------------
// 4 · Hold
// ---------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGInteractionHoldTest,
	"UnrealExtendedGameplay.Interaction.Hold",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGInteractionHoldTest::RunTest(const FString& Parameters)
{
	EGInteractionTest::FScope Scope;
	AEGTestInteractable* Target = Scope.SpawnInteractable(FVector(200.0f, 0.0f, 0.0f));
	Target->SetHoldActivation(1.0f);

	AActor* OtherInteractor = Scope.World->SpawnActor<AActor>();

	IEGInteractableInterface::Execute_InteractionStart(Target, Scope.Owner);
	TestTrue(TEXT("Hold started"), Target->IsHoldActive());
	TestEqual(TEXT("Nothing ran on the press"), Target->InteractCalls, 0);

	IEGInteractableInterface::Execute_InteractionTick(Target, Scope.Owner, 0.5f);
	TestEqual(TEXT("Hold is halfway"), Target->GetHoldProgress(), 0.5f);
	TestEqual(TEXT("Still nothing ran"), Target->InteractCalls, 0);

	// Exclusivity: the hold belongs to whoever started it.
	TestFalse(TEXT("Another interactor is refused mid-hold"),
		IEGInteractableInterface::Execute_CanInteract(Target, OtherInteractor));
	TestTrue(TEXT("The holder is still allowed"),
		IEGInteractableInterface::Execute_CanInteract(Target, Scope.Owner));

	IEGInteractableInterface::Execute_InteractionTick(Target, OtherInteractor, 10.0f);
	TestEqual(TEXT("A non-holder cannot advance the hold"), Target->GetHoldProgress(), 0.5f);

	IEGInteractableInterface::Execute_InteractionTick(Target, Scope.Owner, 0.6f);
	TestEqual(TEXT("The hold completed"), Target->InteractCalls, 1);
	TestFalse(TEXT("The hold cleared on completion"), Target->IsHoldActive());
	TestFalse(TEXT("The hold was cleared BEFORE the interaction ran"), Target->bHoldActiveDuringInteract);

	// A still-pressed input must not re-trigger.
	IEGInteractableInterface::Execute_InteractionTick(Target, Scope.Owner, 5.0f);
	TestEqual(TEXT("A held input did not re-trigger"), Target->InteractCalls, 1);

	IEGInteractableInterface::Execute_InteractionEnd(Target, Scope.Owner);
	TestNull(TEXT("The holder was released"), Target->GetActiveInteractor());
	TestTrue(TEXT("Another interactor is allowed once the hold is over"),
		IEGInteractableInterface::Execute_CanInteract(Target, OtherInteractor));

	// An instant interactable completes on the press instead.
	AEGTestInteractable* Instant = Scope.SpawnInteractable(FVector(200.0f, 300.0f, 0.0f));
	IEGInteractableInterface::Execute_InteractionStart(Instant, Scope.Owner);
	TestEqual(TEXT("An instant interactable ran on the press"), Instant->InteractCalls, 1);

	return true;
}

// ---------------------------------------------------------------------
// 5 · Server validation
// ---------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGInteractionServerValidationTest,
	"UnrealExtendedGameplay.Interaction.ServerValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGInteractionServerValidationTest::RunTest(const FString& Parameters)
{
	EGInteractionTest::FScope Scope;
	AEGTestInteractable* Target = Scope.SpawnInteractable(FVector(200.0f, 0.0f, 0.0f));

	Scope.Interaction->RefreshFocus();
	TestEqual(TEXT("Focused the target"), Scope.Interaction->GetFocusedActor(), (AActor*)Target);

	TestTrue(TEXT("A valid request runs"), Scope.Interaction->TryInteract());
	TestEqual(TEXT("The interaction ran once"), Target->InteractCalls, 1);
	TestEqual(TEXT("The interactor was carried through"), Target->LastInteractor.Get(), Scope.Owner);
	TestNull(TEXT("The interaction was released"), Scope.Interaction->GetActiveInteractionActor());

	// The target itself refuses.
	Target->SetInteractable(false);
	Scope.Interaction->TryInteract();
	TestEqual(TEXT("CanInteract=false was rejected"), Target->InteractCalls, 1);
	Target->SetInteractable(true);

	// Out of range: focus is stale, but the server re-derives the distance itself.
	Target->SetActorLocation(FVector(6000.0f, 0.0f, 0.0f));
	Scope.Interaction->TryInteract();
	TestEqual(TEXT("An out-of-range request was rejected"), Target->InteractCalls, 1);
	Target->SetActorLocation(FVector(200.0f, 0.0f, 0.0f));

	// An external gate refuses, and clears focus on the way.
	Scope.Interaction->RefreshFocus();
	Scope.Interaction->SetInteractionBlocked(true);
	TestFalse(TEXT("A blocked component refuses"), Scope.Interaction->TryInteract());
	TestEqual(TEXT("Nothing ran while blocked"), Target->InteractCalls, 1);

	Scope.Interaction->SetInteractionBlocked(false);
	Scope.Interaction->RefreshFocus();
	TestTrue(TEXT("Unblocking restored interaction"), Scope.Interaction->TryInteract());
	TestEqual(TEXT("The interaction ran again"), Target->InteractCalls, 2);

	return true;
}

// ---------------------------------------------------------------------
// 6 · Teardown
// ---------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGInteractionTeardownTest,
	"UnrealExtendedGameplay.Interaction.Teardown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGInteractionTeardownTest::RunTest(const FString& Parameters)
{
	EGInteractionTest::FScope Scope;
	AEGTestInteractable* Target = Scope.SpawnInteractable(FVector(200.0f, 0.0f, 0.0f));
	Target->SetHoldActivation(2.0f);

	Scope.Interaction->RefreshFocus();
	Scope.Interaction->InteractionStart();
	TestEqual(TEXT("The component owns the running interaction"),
		Scope.Interaction->GetActiveInteractionActor(), (AActor*)Target);
	TestTrue(TEXT("The hold is running"), Target->IsHoldActive());

	// Walking away drops focus, and dropping focus must end the interaction.
	Target->SetActorLocation(FVector(6000.0f, 0.0f, 0.0f));
	Scope.Interaction->RefreshFocus();
	TestNull(TEXT("Focus cleared"), Scope.Interaction->GetFocusedActor());
	TestNull(TEXT("Losing focus ended the interaction"), Scope.Interaction->GetActiveInteractionActor());
	TestFalse(TEXT("Losing focus ended the hold"), Target->IsHoldActive());
	TestEqual(TEXT("An abandoned hold never completed"), Target->InteractCalls, 0);

	// EndPlay must not leave anything running either.
	Target->SetActorLocation(FVector(200.0f, 0.0f, 0.0f));
	Scope.Interaction->RefreshFocus();
	Scope.Interaction->InteractionStart();
	TestTrue(TEXT("A second hold started"), Target->IsHoldActive());

	Scope.Interaction->EndPlay(EEndPlayReason::Destroyed);
	TestNull(TEXT("EndPlay released the interaction"), Scope.Interaction->GetActiveInteractionActor());
	TestFalse(TEXT("EndPlay ended the hold"), Target->IsHoldActive());

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
