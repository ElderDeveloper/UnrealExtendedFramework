// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EGSoftCollisionDriver.h"

#include "EGSoftCollisionDriver_PhysicsBody.generated.h"

class UPrimitiveComponent;

/**
 * Push for pawns whose root primitive simulates physics (vehicles, physics creatures): every frame
 * of the push, before movement, a velocity-change impulse tops the body's linear velocity up along
 * the push direction to the push's speed.
 *
 * Runs on the server; physics replication carries the result. Chaos physics prediction is not
 * involved.
 */
UCLASS(meta = (DisplayName = "Physics Body"))
class UNREALEXTENDEDGAMEPLAY_API UEGSoftCollisionDriver_PhysicsBody : public UEGSoftCollisionDriver
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Soft Collision", meta = (InvalidEnumValues = "Predicted"))
	EEGSoftCollisionDriverRole Role = EEGSoftCollisionDriverRole::ServerAuthority;

	/** Fraction of the missing speed added per frame. Physics bodies usually want less than 1. */
	UPROPERTY(EditAnywhere, Category = "Soft Collision", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Responsiveness = 0.5f;

	virtual EEGSoftCollisionDriverRole GetRole() const override;
	virtual bool SupportsPawn(const APawn& Pawn) const override;
	virtual bool ApplyPush(UEGSoftCollisionComponent& Component, const FEGSoftPushCommand& Command, int32& OutHandle) const override;
	virtual bool NeedsTick() const override { return true; }
	virtual void TickPush(UEGSoftCollisionComponent& Component, const FEGSoftPushCommand& Command, int32 Handle, float Elapsed, float DeltaSeconds) const override;

private:
	static UPrimitiveComponent* GetSimulatingRoot(const APawn* Pawn);
};
