// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "Animation/AnimNotifies/AnimNotify.h"
#include "CoreMinimal.h"

#include "EGPlayMontageNotify.generated.h"

class UAnimMontage;

/** Plays a montage on the mesh that fired the notify. */
UCLASS()
class UNREALEXTENDEDGAMEPLAY_API UEGPlayMontageNotify : public UAnimNotify
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Montage")
	TObjectPtr<UAnimMontage> MontageToPlay;

	UPROPERTY(EditAnywhere, Category = "Montage")
	float InPlayRate = 1;

	UPROPERTY(EditAnywhere, Category = "Montage", meta = (Units = "s"))
	float InTimeToStartMontageAt = 0;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
