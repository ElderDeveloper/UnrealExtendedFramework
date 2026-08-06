// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EGInteractableInterface.h"
#include "EGInteractionTypes.h"
#include "GameFramework/Actor.h"

#include "EGInteractableActor.generated.h"

/**
 * AEGInteractableActor
 *
 * Base actor for anything the player can interact with. It owns only the plumbing every
 * interactable shares — press/hold activation, the focus outline, the success event and the
 * cosmetic multicast — and leaves every gameplay decision to the subclass.
 *
 * Subclasses override Interact_Implementation, do their work, and finish with
 * Super::Interact_Implementation(Interactor) to notify every client:
 *
 *     bool AMyThing::Interact_Implementation(AActor* Interactor)
 *     {
 *         if (!DoTheThing(Interactor)) { return false; }   // no event, no multicast
 *         return Super::Interact_Implementation(Interactor);
 *     }
 *
 * Prompt text and icons come from PromptText/PromptIcon, or from overriding
 * GetInteractionPresentation for something context-dependent. Extra conditions go in
 * CanInteract_Implementation — returning false also drops focus, so return true and vary the
 * prompt when a blocked interactable should still be visible.
 *
 * Hold activation is exclusive: while one interactor holds, CanInteract refuses everyone else.
 *
 * Owns no components and no root: every subclass builds its own hierarchy, and the focus
 * highlight walks whatever primitives it finds.
 *
 * Server-authoritative: the interaction component only routes InteractionStart/Tick/End to the
 * authority, so Interact always runs on the server.
 */
UCLASS(Abstract, Blueprintable)
class UNREALEXTENDEDGAMEPLAY_API AEGInteractableActor : public AActor, public IEGInteractableInterface
{
	GENERATED_BODY()

public:
	AEGInteractableActor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Fires on every machine the actor is relevant to, driven by the success multicast. */
	UPROPERTY(BlueprintAssignable, Category = "Extended|Interactable")
	FEGInteractableInteracted OnInteracted;

	UPROPERTY(BlueprintAssignable, Category = "Extended|Interactable")
	FEGInteractableHoldStateChanged OnHoldStateChanged;

	UFUNCTION(BlueprintPure, Category = "Extended|Interactable")
	bool IsHoldActive() const { return bHoldActive; }

	/** True when this interactable completes through a hold rather than a single press. */
	UFUNCTION(BlueprintPure, Category = "Extended|Interactable")
	bool UsesHoldActivation() const;

	/** True while somebody else owns the running hold. Useful for a "busy" prompt. */
	UFUNCTION(BlueprintPure, Category = "Extended|Interactable")
	bool IsHeldByOtherInteractor(const AActor* Interactor) const;

	/** 0..1 hold progress. Authority uses the accumulated time, clients extrapolate from the hold start. */
	UFUNCTION(BlueprintPure, Category = "Extended|Interactable")
	float GetHoldProgress() const;

	UFUNCTION(BlueprintPure, Category = "Extended|Interactable")
	AActor* GetActiveInteractor() const { return ActiveInteractor.Get(); }

	// IEGInteractableInterface
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual bool Interact_Implementation(AActor* Interactor) override;
	virtual void InteractionStart_Implementation(AActor* Interactor) override;
	virtual void InteractionTick_Implementation(AActor* Interactor, float DeltaTime) override;
	virtual void InteractionEnd_Implementation(AActor* Interactor) override;
	virtual FEGInteractionPresentation GetInteractionPresentation_Implementation(AActor* Interactor, const FHitResult& HitResult) const override;
	virtual void FocusStateChanged_Implementation(bool bIsFocused, const FHitResult& HitResult) override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interactable|Activation")
	EEGInteractionActivation ActivationMode = EEGInteractionActivation::Instant;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interactable|Activation", meta = (ClampMin = "0.0", EditCondition = "ActivationMode == EEGInteractionActivation::Hold", EditConditionHides))
	float HoldDuration = 1.0f;

	/** Default prompt text. Override GetInteractionPresentation for anything context-dependent. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interactable|Presentation")
	FText PromptText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interactable|Presentation")
	TObjectPtr<UTexture2D> PromptIcon;

	/** When false, CanInteract always refuses — locked doors, disabled levers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interactable|Rules")
	bool bIsInteractable = true;

	/** Local custom-depth outline while focused (the outline post-process reads the stencil). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interactable|Focus")
	bool bHighlightOnFocus = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interactable|Focus", meta = (ClampMin = "0", ClampMax = "255", EditCondition = "bHighlightOnFocus"))
	int32 HighlightStencilValue = 2;

	/** Cosmetic hooks — run on every machine the actor is relevant to, including the listen host. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Extended|Interactable")
	void BP_OnInteracted(AActor* Interactor);

	UFUNCTION(BlueprintImplementableEvent, Category = "Extended|Interactable")
	void BP_OnHoldStateChanged(bool bStarted, AActor* Interactor);

	/** Local focus feedback only; never replicated. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Extended|Interactable")
	void BP_OnFocusChanged(bool bIsFocused, const FHitResult& HitResult);

	void ApplyFocusHighlight(bool bEnabled) const;

private:
	void SetHoldActive(bool bNewHoldActive, AActor* Interactor);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastInteracted(AActor* Interactor);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastHoldStateChanged(bool bStarted, AActor* Interactor);

	TWeakObjectPtr<AActor> ActiveInteractor;

	float HoldElapsed = 0.0f;
	float HoldStartTime = 0.0f;
	bool bHoldActive = false;
};
