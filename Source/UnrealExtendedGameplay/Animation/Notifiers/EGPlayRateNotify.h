// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "UObject/Object.h"
#include "EGPlayRateNotify.generated.h"

/**
 * 
 */
UCLASS()
class UNREALEXTENDEDGAMEPLAY_API UEGPlayRateNotify : public UAnimNotifyState
{
	GENERATED_BODY()

	
	UPROPERTY(EditDefaultsOnly)
	float BeginPlayRate = 1;
	UPROPERTY(EditDefaultsOnly)
	float EndPlayRate = 1;

	virtual void NotifyBegin(USkeletalMeshComponent * MeshComp, UAnimSequenceBase * Animation, float TotalDuration) override;
	virtual void NotifyEnd(USkeletalMeshComponent * MeshComp, UAnimSequenceBase * Animation) override;
};
