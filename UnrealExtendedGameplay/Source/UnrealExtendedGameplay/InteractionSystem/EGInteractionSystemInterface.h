// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "EGInteractionSystemInterface.generated.h"

class UEGInteractionSystemComponent;

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UEGInteractionSystemInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Implemented by the actor that carries a UEGInteractionSystemComponent, the same way the
 * ability system interface only hands out its component. Nothing else lives here.
 */
class UNREALEXTENDEDGAMEPLAY_API IEGInteractionSystemInterface
{
	GENERATED_BODY()

public:
	virtual UEGInteractionSystemComponent* GetInteractionSystemComponent() const = 0;
};
