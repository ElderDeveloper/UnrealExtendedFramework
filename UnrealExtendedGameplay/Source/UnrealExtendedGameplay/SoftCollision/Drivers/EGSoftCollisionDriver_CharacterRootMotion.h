// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EGSoftCollisionDriver.h"

#include "EGSoftCollisionDriver_CharacterRootMotion.generated.h"

/**
 * Predicted push for player-controlled Characters: a FEGRootMotionSource_SoftPush on the
 * CharacterMovementComponent.
 *
 * Root motion sources are the only predicted movement an unmodified CMC offers: the owning client
 * stores the active sources in its saved moves and replays them after a correction. A push with a
 * start time (predicted pushes, and the server's copy of them) starts after that client move time;
 * the engine applies a source only to the part of a move after its start time.
 *
 * Moves are not combined while a root motion source is active, and simulated proxies receive active
 * sources through the Character's replicated root motion, so this driver is meant for the few
 * player pawns, not for crowds.
 */
UCLASS(meta = (DisplayName = "Character Root Motion (Predicted)"))
class UNREALEXTENDEDGAMEPLAY_API UEGSoftCollisionDriver_CharacterRootMotion : public UEGSoftCollisionDriver
{
	GENERATED_BODY()

public:
	/** Higher priorities are applied first. Keep it below gameplay root motion. */
	UPROPERTY(EditAnywhere, Category = "Soft Collision", meta = (ClampMin = "0", ClampMax = "65535"))
	int32 Priority = 2;

	UPROPERTY(EditAnywhere, Category = "Soft Collision")
	FName InstanceName = FName(TEXT("EGSoftPush"));

	virtual EEGSoftCollisionDriverRole GetRole() const override { return EEGSoftCollisionDriverRole::Predicted; }
	virtual bool SupportsPawn(const APawn& Pawn) const override;
	virtual bool ApplyPush(UEGSoftCollisionComponent& Component, const FEGSoftPushCommand& Command, int32& OutHandle) const override;
	virtual void CancelPush(UEGSoftCollisionComponent& Component, const FEGSoftPushCommand& Command, int32 Handle) const override;
	virtual bool IsPushRunning(const UEGSoftCollisionComponent& Component, int32 Handle) const override;
};
