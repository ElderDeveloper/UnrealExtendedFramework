// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"

class AActor;

/**
 * One answer to "who is this character attacking" for every animation feature (soft lock, warp
 * targets), so they agree with the lock-on systems instead of each running their own search.
 */
namespace EGAnimTargeting
{
	/** The actor locked on by UEGTargetingComponent or UEGDynamicTargetingComponent, if any. */
	UNREALEXTENDEDGAMEPLAY_API AActor* GetLockedTarget(const AActor* Owner);

	/**
	 * The closest pawn within Radius whose direction from the owner is within HalfAngleDegrees of
	 * the owner's forward (180 accepts any direction). Ignores the owner and its attachments.
	 */
	UNREALEXTENDEDGAMEPLAY_API AActor* FindClosestPawn(const AActor* Owner, float Radius, float HalfAngleDegrees, const TArray<TEnumAsByte<EObjectTypeQuery>>& ObjectTypes);

	/** GetLockedTarget, then FindClosestPawn when nothing is locked on and Radius is above zero. */
	UNREALEXTENDEDGAMEPLAY_API AActor* FindTarget(const AActor* Owner, float Radius, float HalfAngleDegrees, const TArray<TEnumAsByte<EObjectTypeQuery>>& ObjectTypes);
}
