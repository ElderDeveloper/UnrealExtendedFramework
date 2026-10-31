// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EGSoftCollisionDriver.h"

#include "EGSoftCollisionDriver_PawnMovement.generated.h"

/**
 * Push for any pawn movement component that is not a CharacterMovementComponent (floating pawn
 * movement and custom ones): every frame of the push, before movement, the component's velocity is
 * topped up along the push direction to the push's speed.
 *
 * Runs on the server by default. Pawn movement that the owning client drives (and replicates
 * itself) wants Owner Authority instead.
 */
UCLASS(meta = (DisplayName = "Pawn Movement"))
class UNREALEXTENDEDGAMEPLAY_API UEGSoftCollisionDriver_PawnMovement : public UEGSoftCollisionDriver
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Soft Collision", meta = (InvalidEnumValues = "Predicted"))
	EEGSoftCollisionDriverRole Role = EEGSoftCollisionDriverRole::ServerAuthority;

	/** Fraction of the missing speed added per frame. */
	UPROPERTY(EditAnywhere, Category = "Soft Collision", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Responsiveness = 1.0f;

	virtual EEGSoftCollisionDriverRole GetRole() const override;
	virtual bool SupportsPawn(const APawn& Pawn) const override;
	virtual bool ApplyPush(UEGSoftCollisionComponent& Component, const FEGSoftPushCommand& Command, int32& OutHandle) const override;
	virtual bool NeedsTick() const override { return true; }
	virtual void TickPush(UEGSoftCollisionComponent& Component, const FEGSoftPushCommand& Command, int32 Handle, float Elapsed, float DeltaSeconds) const override;
};
