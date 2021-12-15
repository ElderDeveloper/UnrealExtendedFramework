// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UEExtendedPlayMontageNotify.generated.h"

/**
 * 
 */
UCLASS()
class UEEXPANDEDFRAMEWORK_API UUEExtendedPlayMontageNotify : public UAnimNotify
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere)
	UAnimMontage* MontageToPlay;

	UPROPERTY(EditAnywhere)
	float InPlayRate = 1;

	UPROPERTY(EditAnywhere)
	float InTimeToStartMontageAt = 0;
	
	// Begin UAnimNotify interface
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override;
};
