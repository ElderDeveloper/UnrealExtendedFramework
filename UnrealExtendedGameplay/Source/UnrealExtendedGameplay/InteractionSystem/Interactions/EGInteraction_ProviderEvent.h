// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealExtendedGameplay/InteractionSystem/EGInteraction.h"

#include "EGInteraction_ProviderEvent.generated.h"

/**
 * Instant interaction that hands the work to the target. On the server it fires the
 * provider's OnInteractionEvent with Completed, which multicasts to every machine, and ends.
 * The target actor handles the event in Blueprint or C++. Server only by default.
 *
 * This is the zero-code path for content that already knows what to do when interacted with.
 */
UCLASS(Blueprintable)
class UNREALEXTENDEDGAMEPLAY_API UEGInteraction_ProviderEvent : public UEGInteraction
{
	GENERATED_BODY()

public:
	UEGInteraction_ProviderEvent();

	virtual void ActivateInteraction_Implementation() override;
};
