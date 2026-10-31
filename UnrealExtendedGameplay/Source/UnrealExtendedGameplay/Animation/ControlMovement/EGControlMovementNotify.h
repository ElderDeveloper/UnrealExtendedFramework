// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "CoreMinimal.h"

#include "EGControlMovementNotify.generated.h"

class UCharacterMovementComponent;

/**
 * Zeroes the owning character's MaxAcceleration for the length of the window, so input cannot
 * steer it. Velocity it already has still decays through braking.
 *
 * Overlapping windows (a combo blending from one attack into the next, two clips that both carry
 * the notify) are reference counted per movement component: the value saved by the first window
 * is restored when the last one ends.
 */
UCLASS()
class UNREALEXTENDEDGAMEPLAY_API UEGControlMovementNotify : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

private:
	struct FMovementLock
	{
		int32 Count = 0;
		float SavedMaxAcceleration = 0.0f;
	};

	/** Shared by every instance of this notify, so windows from different clips stack correctly. */
	static TMap<TWeakObjectPtr<UCharacterMovementComponent>, FMovementLock> ActiveLocks;

	static UCharacterMovementComponent* GetMovement(const USkeletalMeshComponent* MeshComp);
};
