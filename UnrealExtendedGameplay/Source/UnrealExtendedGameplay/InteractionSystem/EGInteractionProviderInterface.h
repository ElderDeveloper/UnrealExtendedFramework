// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "EGInteractionProviderInterface.generated.h"

class UEGInteractionProviderComponent;

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UEGInteractionProviderInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Implemented by actors that want to hand out their provider component without a component
 * search. Optional: UEGInteractionProviderComponent::FindProvider falls back to
 * FindComponentByClass when the actor does not implement this.
 */
class UNREALEXTENDEDGAMEPLAY_API IEGInteractionProviderInterface
{
	GENERATED_BODY()

public:
	virtual UEGInteractionProviderComponent* GetInteractionProviderComponent() const = 0;
};
