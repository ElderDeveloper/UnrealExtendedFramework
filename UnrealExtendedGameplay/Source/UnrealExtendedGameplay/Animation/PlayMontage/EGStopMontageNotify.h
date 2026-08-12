// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "EGStopMontageNotify.generated.h"

/**
 * 
 */
UCLASS()
class UNREALEXTENDEDGAMEPLAY_API UEGStopMontageNotify : public UAnimNotify
{
	GENERATED_BODY()

	/*
	 * If Its Null It Stops All Montages
	 */
	UPROPERTY(EditAnywhere)
	UAnimMontage* MontageToStop;

	UPROPERTY(EditAnywhere)
	float BlendOutTime = 0.25;


#if ENGINE_MAJOR_VERSION != 5
	// Begin UAnimNotify interface
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override;
#endif
	

#if ENGINE_MAJOR_VERSION == 5
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
#endif
};
