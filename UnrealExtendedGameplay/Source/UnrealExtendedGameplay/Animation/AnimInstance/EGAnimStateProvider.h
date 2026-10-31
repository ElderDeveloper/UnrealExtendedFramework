// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"

#include "EGAnimStateProvider.generated.h"

UINTERFACE(BlueprintType, MinimalAPI)
class UEGAnimStateProvider : public UInterface
{
	GENERATED_BODY()
};

/**
 * IEGAnimStateProvider
 *
 * Implemented by the actor or its components to describe gameplay state the animation needs (in
 * cover, crouched in cover, seated, a combo window open) as tags. UEGAnimInstance gathers them on
 * the game thread once per update, so an AnimBP reads tags instead of casting to every system.
 */
class UNREALEXTENDEDGAMEPLAY_API IEGAnimStateProvider
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Extended|Anim State")
	FGameplayTagContainer GetAnimStateTags() const;

	virtual FGameplayTagContainer GetAnimStateTags_Implementation() const { return FGameplayTagContainer(); }
};
