// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGInteractableInterface.h"

#include "EGInteractionComponent.h"

void IEGInteractableInterface::InteractionStart_Implementation(AActor* Interactor)
{
	// Press-and-release interactables complete on the press. Hold interactables override this.
	// The hold trio carries no hit, so resolve the validated one the component published.
	Execute_Interact(Cast<UObject>(this), Interactor, UEGInteractionComponent::GetInteractionHit(Interactor));
}

void IEGInteractableInterface::InteractionTick_Implementation(AActor* Interactor, float DeltaTime)
{
	(void)Interactor;
	(void)DeltaTime;
}

void IEGInteractableInterface::InteractionEnd_Implementation(AActor* Interactor)
{
	(void)Interactor;
}
