// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UnrealExtendedGameplay/Systems/Interaction/EGInteractableActor.h"
#include "UnrealExtendedGameplay/Systems/Interaction/EGInteractableInterface.h"

#include "EGInteractionTestTypes.generated.h"

class USphereComponent;

/**
 * A game-owned interface inheriting the plugin's, exactly as DevilOfPlague's and TalesOfTrade's
 * do after the migration. Declares nothing of its own — the point is that a class implementing
 * only this one is still found and dispatched through IEGInteractableInterface.
 */
UINTERFACE(BlueprintType)
class UEGTestDerivedInteractableInterface : public UEGInteractableInterface
{
	GENERATED_BODY()
};

class IEGTestDerivedInteractableInterface : public IEGInteractableInterface
{
	GENERATED_BODY()
};

/** Implements ONLY the derived interface. Used by the interface-inheritance test. */
UCLASS(NotBlueprintable, Hidden)
class AEGTestDerivedInterfaceActor : public AActor, public IEGTestDerivedInteractableInterface
{
	GENERATED_BODY()

public:
	virtual bool CanInteract_Implementation(AActor* Interactor) const override
	{
		++CanInteractCalls;
		return bAllowInteraction;
	}

	virtual bool Interact_Implementation(AActor* Interactor) override
	{
		++InteractCalls;
		return true;
	}

	virtual FEGInteractionPresentation GetInteractionPresentation_Implementation(AActor* Interactor, const FHitResult& HitResult) const override
	{
		FEGInteractionPresentation Presentation;
		Presentation.Text = FText::FromString(TEXT("DerivedPrompt"));
		return Presentation;
	}

	bool bAllowInteraction = true;
	mutable int32 CanInteractCalls = 0;
	int32 InteractCalls = 0;
};

/** Concrete interactable with collision, so a trace can actually find it. */
UCLASS(NotBlueprintable, Hidden)
class AEGTestInteractable : public AEGInteractableActor
{
	GENERATED_BODY()

public:
	AEGTestInteractable(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool Interact_Implementation(AActor* Interactor) override
	{
		++InteractCalls;
		LastInteractor = Interactor;
		// Records the contract that a completed hold is cleared *before* the interaction runs,
		// so a still-pressed input cannot re-trigger on the next tick.
		bHoldActiveDuringInteract = IsHoldActive();
		// Deliberately skips Super: the multicast needs a net driver the test world has no use for.
		return true;
	}

	virtual void FocusStateChanged_Implementation(bool bIsFocused, const FHitResult& HitResult) override
	{
		bIsFocused ? ++FocusEnterCalls : ++FocusExitCalls;
	}

	void SetHoldActivation(float InHoldDuration)
	{
		ActivationMode = EEGInteractionActivation::Hold;
		HoldDuration = InHoldDuration;
	}

	void SetInteractable(bool bValue) { bIsInteractable = bValue; }
	void SetPrompt(const FText& InText) { PromptText = InText; }

	UPROPERTY()
	TObjectPtr<USphereComponent> Collision;

	int32 InteractCalls = 0;
	int32 FocusEnterCalls = 0;
	int32 FocusExitCalls = 0;
	bool bHoldActiveDuringInteract = false;
	TWeakObjectPtr<AActor> LastInteractor;
};
