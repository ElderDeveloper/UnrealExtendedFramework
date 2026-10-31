// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "EFPoolableInterface.generated.h"

UINTERFACE(Blueprintable)
class UNREALEXTENDEDFRAMEWORK_API UEFPoolable : public UInterface
{
	GENERATED_BODY()
};

/**
 * Optional callbacks for actors (and their components) that live in an object pool.
 *
 * BeginPlay runs once, when the pool first spawns the actor. Put per-use setup in
 * OnAcquiredFromPool (reset health, set velocity, restart effects) and per-use cleanup in
 * OnReleasedToPool (stop sounds, clear references). The pool itself shows and hides the actor,
 * turns collision and tick on and off, and activates and deactivates its components.
 */
class UNREALEXTENDEDFRAMEWORK_API IEFPoolable
{
	GENERATED_BODY()

public:
	/** Taken from the pool (or just spawned for it), placed, shown and active again. */
	UFUNCTION(BlueprintNativeEvent, Category = "Extended Framework|Pool")
	void OnAcquiredFromPool();
	virtual void OnAcquiredFromPool_Implementation() {}

	/** Returned to the pool: about to be hidden and deactivated until the next acquire. */
	UFUNCTION(BlueprintNativeEvent, Category = "Extended Framework|Pool")
	void OnReleasedToPool();
	virtual void OnReleasedToPool_Implementation() {}
};
