// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EGSoftCollisionDriver.h"

#include "EGSoftCollisionDriver_CharacterForce.generated.h"

class ACharacter;
class UCharacterMovementComponent;

/**
 * Server push for Characters the server alone simulates (AI, unpossessed pawns): every frame of the
 * push, before movement, an AddImpulse with velocity change tops the Character's velocity along the
 * push direction up to the push's speed. Nothing is added when the pawn already moves that fast that
 * way, and ground friction fades what is left once the push ends.
 *
 * No root motion source, so nothing extra replicates: simulated proxies follow normal movement
 * replication. Do not use it on a pawn a remote player controls; the owning client would not predict
 * it and would be corrected.
 *
 * A Character whose movement does not run (no controller, bRunPhysicsWithNoController off, no root
 * motion) is not pushed: it cannot move, and the impulses would pile up and launch it the moment it
 * is possessed.
 */
UCLASS(meta = (DisplayName = "Character Force (Server)"))
class UNREALEXTENDEDGAMEPLAY_API UEGSoftCollisionDriver_CharacterForce : public UEGSoftCollisionDriver
{
	GENERATED_BODY()

public:
	/** Fraction of the missing speed added per frame. 1 reaches the push speed at once. */
	UPROPERTY(EditAnywhere, Category = "Soft Collision", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Responsiveness = 1.0f;

	virtual EEGSoftCollisionDriverRole GetRole() const override { return EEGSoftCollisionDriverRole::ServerAuthority; }
	virtual bool SupportsPawn(const APawn& Pawn) const override;
	virtual bool ApplyPush(UEGSoftCollisionComponent& Component, const FEGSoftPushCommand& Command, int32& OutHandle) const override;
	virtual bool NeedsTick() const override { return true; }
	virtual void TickPush(UEGSoftCollisionComponent& Component, const FEGSoftPushCommand& Command, int32 Handle, float Elapsed, float DeltaSeconds) const override;

private:
	/** Whether the movement component moves the Character at all: a controller, bRunPhysicsWithNoController or root motion (its TickComponent test). */
	static bool IsMovementRunning(const ACharacter& Character, const UCharacterMovementComponent& Movement);
};
