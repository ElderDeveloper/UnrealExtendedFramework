// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EGInteractionTypes.h"
#include "UObject/Interface.h"

#include "EGInteractableInterface.generated.h"

UINTERFACE(BlueprintType)
class UNREALEXTENDEDGAMEPLAY_API UEGInteractableInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Implemented by anything the interaction component can focus and run.
 *
 * Two execution shapes share one interface. A press-and-release interactable only needs
 * CanInteract + Interact; the default InteractionStart runs Interact immediately. A hold
 * interactable overrides the InteractionStart/Tick/End trio and decides for itself when the
 * hold is complete — AEGInteractableActor already implements that, so most content inherits
 * it rather than writing it.
 *
 * Interact and the hold trio run on **authority only**: the component routes every execution
 * path through a server RPC and re-validates before dispatching. CanInteract,
 * GetInteractionPresentation and FocusStateChanged run on both sides, because the client needs
 * them to decide what prompt to draw.
 *
 * The current interaction mode and the hit that produced the focus are not parameters — they are
 * per-call state on the component, reachable with UEGInteractionComponent::GetInteractionMode()
 * and ::GetInteractionHit() from the interactor. Keeping them off the signature is what lets a
 * game's own interaction interface inherit this one without editing every implementer.
 */
class UNREALEXTENDEDGAMEPLAY_API IEGInteractableInterface
{
	GENERATED_BODY()

public:
	/** Can this actor be interacted with right now? Returning false also drops focus. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Extended|Interaction")
	bool CanInteract(AActor* Interactor) const;
	virtual bool CanInteract_Implementation(AActor* Interactor) const { return Interactor != nullptr; }

	/** Run the interaction. Authority only. Return false to report failure to the interactor. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Extended|Interaction")
	bool Interact(AActor* Interactor);
	virtual bool Interact_Implementation(AActor* Interactor) { return false; }

	/**
	 * Called once on the server when the interaction input is pressed.
	 * The default implementation completes the interaction immediately.
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Extended|Interaction")
	void InteractionStart(AActor* Interactor);
	virtual void InteractionStart_Implementation(AActor* Interactor);

	/**
	 * Called on the server while the interaction input remains pressed.
	 * Hold duration and completion are owned by the interactable.
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Extended|Interaction")
	void InteractionTick(AActor* Interactor, float DeltaTime);
	virtual void InteractionTick_Implementation(AActor* Interactor, float DeltaTime);

	/** Called once when the interaction input is released or the target is lost. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Extended|Interaction")
	void InteractionEnd(AActor* Interactor);
	virtual void InteractionEnd_Implementation(AActor* Interactor);

	/** The prompt text and icon for this interaction context. Queried during focus updates. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Extended|Interaction")
	FEGInteractionPresentation GetInteractionPresentation(AActor* Interactor, const FHitResult& HitResult) const;
	virtual FEGInteractionPresentation GetInteractionPresentation_Implementation(AActor* Interactor, const FHitResult& HitResult) const
	{
		return FEGInteractionPresentation();
	}

	/** Local focus notification, for outlines and other cosmetic feedback. Never replicated. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Extended|Interaction")
	void FocusStateChanged(bool bIsFocused, const FHitResult& HitResult);
	virtual void FocusStateChanged_Implementation(bool bIsFocused, const FHitResult& HitResult) {}
};
