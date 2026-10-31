// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "CoreMinimal.h"
#include "GameFramework/RootMotionSource.h"

#include "EGRootMotionMoveNotifyState.generated.h"

class UCurveFloat;

/**
 * Moves a character for the length of the window with a constant-force root motion source on its
 * CharacterMovementComponent: a dash, a lunge, a step. Unlike setting the actor's location, the
 * movement component simulates it, so it sweeps and slides properly and is predicted on the
 * owning client. Applied on the server and the owning client; simulated proxies get the result
 * through replication.
 *
 * The direction is fixed when the window begins. Prefer this to Move Forward / Move Forward
 * Window for characters.
 */
UCLASS(meta = (DisplayName = "Root Motion Move"))
class UNREALEXTENDEDGAMEPLAY_API UEGRootMotionMoveNotifyState : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	/** Turns the direction away from the actor's forward, in the actor's frame. Yaw 90 moves right. */
	UPROPERTY(EditAnywhere, Category = "Root Motion")
	FRotator DirectionOffset = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, Category = "Root Motion", meta = (ClampMin = "0", Units = "CentimetersPerSecond"))
	float Speed = 600.0f;

	/** Strength over the window's normalised time (0..1). Empty is constant. */
	UPROPERTY(EditAnywhere, Category = "Root Motion")
	TObjectPtr<UCurveFloat> StrengthOverTime = nullptr;

	/** Add to the character's own movement instead of replacing it. Replacing also replaces gravity. */
	UPROPERTY(EditAnywhere, Category = "Root Motion")
	bool bAdditive = false;

	UPROPERTY(EditAnywhere, Category = "Root Motion|Finish")
	ERootMotionFinishVelocityMode FinishVelocityMode = ERootMotionFinishVelocityMode::MaintainLastRootMotionVelocity;

	/** Velocity set when the move finishes, with FinishVelocityMode SetVelocity. */
	UPROPERTY(EditAnywhere, Category = "Root Motion|Finish", meta = (EditCondition = "FinishVelocityMode == ERootMotionFinishVelocityMode::SetVelocity"))
	FVector FinishSetVelocity = FVector::ZeroVector;

	/** Speed the velocity is clamped to when the move finishes, with FinishVelocityMode ClampVelocity. */
	UPROPERTY(EditAnywhere, Category = "Root Motion|Finish", meta = (EditCondition = "FinishVelocityMode == ERootMotionFinishVelocityMode::ClampVelocity", ClampMin = "0"))
	float FinishClampVelocity = 0.0f;

	/** Among simultaneous root motion sources, higher wins. */
	UPROPERTY(EditAnywhere, Category = "Root Motion")
	int32 Priority = 500;

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

private:
	/** Root motion source ids per mesh, oldest first; the notify object is shared by every mesh playing the clip. */
	TMap<TWeakObjectPtr<USkeletalMeshComponent>, TArray<uint16>> ActiveSources;
};
