// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "EGSoftCollisionTypes.h"

#include "EGSoftCollisionInterface.generated.h"

class UEGSoftCollisionProfile;

UINTERFACE(MinimalAPI, BlueprintType)
class UEGSoftCollisionInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Optional, on a pawn with a UEGSoftCollisionComponent. Nothing is required: without it the
 * component's own filter and profile apply and every push is issued as computed.
 *
 * None of these run per frame. The filter and profile are read at BeginPlay and whenever the
 * component's RefreshFilter is called (death, mounting, team change). ModifySoftPushVelocity runs
 * once per issued push, on the machine that decides it, before the push is quantized and sent.
 */
class UNREALEXTENDEDGAMEPLAY_API IEGSoftCollisionInterface
{
	GENERATED_BODY()

public:
	/** Who may push whom. DefaultFilter is the component's own. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Extended|Soft Collision")
	FEGSoftCollisionFilter GetSoftCollisionFilter(const FEGSoftCollisionFilter& DefaultFilter) const;

	/** A profile to use instead of the component's. None keeps the component's. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Extended|Soft Collision")
	UEGSoftCollisionProfile* GetSoftCollisionProfileOverride() const;

	/** Reshape a push before it is issued. Return a zero vector to veto it. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Extended|Soft Collision")
	FVector ModifySoftPushVelocity(FVector Velocity, EEGSoftPushBand Band) const;

	virtual FEGSoftCollisionFilter GetSoftCollisionFilter_Implementation(const FEGSoftCollisionFilter& DefaultFilter) const { return DefaultFilter; }
	virtual UEGSoftCollisionProfile* GetSoftCollisionProfileOverride_Implementation() const { return nullptr; }
	virtual FVector ModifySoftPushVelocity_Implementation(FVector Velocity, EEGSoftPushBand Band) const { return Velocity; }
};
