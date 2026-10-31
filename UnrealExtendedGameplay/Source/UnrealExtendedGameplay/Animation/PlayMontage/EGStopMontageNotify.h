// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "Animation/AnimNotifies/AnimNotify.h"
#include "CoreMinimal.h"

#include "EGStopMontageNotify.generated.h"

class UAnimMontage;

/** Stops a montage, or every montage, on the mesh that fired the notify. */
UCLASS()
class UNREALEXTENDEDGAMEPLAY_API UEGStopMontageNotify : public UAnimNotify
{
	GENERATED_BODY()

public:
	/** Empty stops all montages, including the one this notify sits in. */
	UPROPERTY(EditAnywhere, Category = "Montage")
	TObjectPtr<UAnimMontage> MontageToStop;

	UPROPERTY(EditAnywhere, Category = "Montage", meta = (ClampMin = "0", Units = "s"))
	float BlendOutTime = 0.25;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
