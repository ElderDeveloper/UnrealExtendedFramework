// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "CoreMinimal.h"
#include "UnrealExtendedGameplay/Movement/EGMovementModifierComponent.h"

#include "EGControlMovementNotify.generated.h"

/**
 * Scales the owning character's movement for the length of the window, through
 * UEGMovementModifierComponent (added on demand). The default zeroes acceleration so input
 * cannot steer the character; velocity it already has still decays through braking.
 *
 * Overlapping windows stack in the component instead of saving and restoring values themselves.
 */
UCLASS()
class UNREALEXTENDEDGAMEPLAY_API UEGControlMovementNotify : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UEGControlMovementNotify();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	FEGMovementModifier Modifier;

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

private:
	/** Handles per mesh, oldest first; the notify object is shared by every mesh playing the clip. */
	TMap<TWeakObjectPtr<USkeletalMeshComponent>, TArray<FEGMovementModifierHandle>> ActiveHandles;
};
