// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "CoreMinimal.h"

#include "EGMoveForwardNotifyWindow.generated.h"

/**
 * Moves the owner along its forward (turned by ForwardRotationPlus) for the length of the window.
 * Runs on the server and the owning client; simulated proxies get the move through replication.
 *
 * Sets the actor location directly, around character movement. For characters prefer Root Motion
 * Move, which the movement component simulates and predicts.
 */
UCLASS()
class UNREALEXTENDEDGAMEPLAY_API UEGMoveForwardNotifyWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	/** Turns the move direction away from the actor's forward, in the actor's frame. Yaw 90 moves right. */
	UPROPERTY(EditAnywhere, Category = "Move")
	FRotator ForwardRotationPlus = FRotator::ZeroRotator;

	/** Distance covered per second of the window (cm/s), not in total. */
	UPROPERTY(EditAnywhere, Category = "Move", meta = (DisplayName = "Move Speed"))
	float MoveDistance = 100;

	/**
	 * Whether we sweep to the destination location, triggering overlaps along the way and stopping short of the target if blocked by something.
	 * Only the root component is swept and checked for blocking collision, child components move without sweeping. If collision is off, this has no effect.
	 */
	UPROPERTY(EditAnywhere, Category = "Move")
	bool bSweep = true;

	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;
};
