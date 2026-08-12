// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "EGReportNoiseEventNotify.generated.h"

/**
 * 
 */
UCLASS()
class UNREALEXTENDEDGAMEPLAY_API UEGReportNoiseEventNotify : public UAnimNotify
{
	GENERATED_BODY()

public:

	/**
	 * @param Loudness Loudness of the noise. If MaxRange is non-zero, modifies MaxRange, otherwise modifies the squared distance of the sensor's range.
	 */
	UPROPERTY(EditAnywhere)
	float Loudness = 1;


	/**
	 * @param MaxRange Max range at which the sound can be heard, multiplied by Loudness. Values <= 0 mean no limit (still limited by listener's range however).
	 */
	UPROPERTY(EditAnywhere)
	float MaxRange = 200;

	
	/**
	 * @param Tag Identifier for the event.
	 */
	UPROPERTY(EditAnywhere)
	FName Tag;

		
	/**
	 * @param PrintToLog Track For Activation In Log
	 */
	bool PrintToLog = false;
	
	// Begin UAnimNotify interface
	#if ENGINE_MAJOR_VERSION != 5
		virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override;
	#endif

	#if	ENGINE_MAJOR_VERSION == 5
		virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	#endif
};
