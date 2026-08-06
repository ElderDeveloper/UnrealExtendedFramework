// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGInteractableInterface.h"

void IEGInteractableInterface::InteractionStart_Implementation(AActor* Interactor)
{
	// Press-and-release interactables complete on the press. Hold interactables override this.
	Execute_Interact(Cast<UObject>(this), Interactor);
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
