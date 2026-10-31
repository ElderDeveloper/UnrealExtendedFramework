// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "Systems/Pool/EFPoolableInterface.h"
#include "EFPoolTestTypes.generated.h"

/** Counts pool callbacks, for the object pool tests. Not for use in games. */
UCLASS(NotBlueprintable, NotPlaceable, HideDropdown)
class AEFPoolTestActor : public AActor, public IEFPoolable
{
	GENERATED_BODY()

public:
	AEFPoolTestActor()
	{
		RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
		PrimaryActorTick.bCanEverTick = true;
		PrimaryActorTick.bStartWithTickEnabled = true;
	}

	virtual void OnAcquiredFromPool_Implementation() override { ++Acquires; }
	virtual void OnReleasedToPool_Implementation() override { ++Releases; }

	int32 Acquires = 0;
	int32 Releases = 0;
};
