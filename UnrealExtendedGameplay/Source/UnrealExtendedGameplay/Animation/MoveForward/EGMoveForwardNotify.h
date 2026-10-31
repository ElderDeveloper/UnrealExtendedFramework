// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "Animation/AnimNotifies/AnimNotify.h"
#include "CoreMinimal.h"

#include "EGMoveForwardNotify.generated.h"

/**
 * Moves the owner by MoveDistance along its forward (turned by ForwardRotationPlus) in one step.
 * Runs on the server and the owning client; simulated proxies get the move through replication.
 */
UCLASS()
class UNREALEXTENDEDGAMEPLAY_API UEGMoveForwardNotify : public UAnimNotify
{
	GENERATED_BODY()

public:
	/** Turns the move direction away from the actor's forward, in the actor's frame. Yaw 90 moves right. */
	UPROPERTY(EditAnywhere, Category = "Move")
	FRotator ForwardRotationPlus = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, Category = "Move", meta = (Units = "cm"))
	float MoveDistance = 100;

	/**
	 * Whether we sweep to the destination location, triggering overlaps along the way and stopping short of the target if blocked by something.
	 * Only the root component is swept and checked for blocking collision, child components move without sweeping. If collision is off, this has no effect.
	 */
	UPROPERTY(EditAnywhere, Category = "Move")
	bool bSweep = true;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
