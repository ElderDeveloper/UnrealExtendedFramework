// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "CoreMinimal.h"

#include "EGPlayRateNotify.generated.h"

/**
 * Sets the montage play rate when the window begins and again when it ends. Placed on a montage it
 * changes only that montage; placed on a sequence it changes every active montage.
 */
UCLASS()
class UNREALEXTENDEDGAMEPLAY_API UEGPlayRateNotify : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Play Rate")
	float BeginPlayRate = 1;

	UPROPERTY(EditAnywhere, Category = "Play Rate")
	float EndPlayRate = 1;

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

private:
	static void SetPlayRate(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float PlayRate);
};
