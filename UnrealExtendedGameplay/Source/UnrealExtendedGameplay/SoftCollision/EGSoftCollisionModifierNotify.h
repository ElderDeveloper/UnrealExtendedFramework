// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "CoreMinimal.h"
#include "EGSoftCollisionTypes.h"

#include "EGSoftCollisionModifierNotify.generated.h"

/**
 * Applies a Soft Collision modifier to the owning pawn for the length of the window: an attack that
 * must not be shoved off its target (Immovable), a heavy stance (WeightScale), a dodge that pushes
 * nobody. The pawn needs a UEGSoftCollisionComponent; without one the window does nothing.
 *
 * Overlapping windows stack in the component. The default makes the pawn immovable.
 */
UCLASS(meta = (DisplayName = "Soft Collision Modifier"))
class UNREALEXTENDEDGAMEPLAY_API UEGSoftCollisionModifierNotify : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UEGSoftCollisionModifierNotify();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soft Collision")
	FEGSoftCollisionModifier Modifier;

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

#if WITH_EDITOR
	virtual FString GetNotifyName_Implementation() const override;
#endif

private:
	/** Handles per mesh, oldest first; the notify object is shared by every mesh playing the clip. */
	TMap<TWeakObjectPtr<USkeletalMeshComponent>, TArray<FEGSoftCollisionModifierHandle>> ActiveHandles;
};
