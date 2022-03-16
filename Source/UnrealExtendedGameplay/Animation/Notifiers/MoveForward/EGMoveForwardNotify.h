// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Runtime/Launch/Resources/Version.h"
#include "UObject/Object.h"
#include "EGMoveForwardNotify.generated.h"

/**
 * 
 */
UCLASS()
class UNREALEXTENDEDGAMEPLAY_API UEGMoveForwardNotify : public UAnimNotify
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FRotator ForwardRotationPlus;

	UPROPERTY(EditAnywhere)
	float MoveDistance = 100;

	/**
	**	@param bSweep	Whether we sweep to the destination location, triggering overlaps along the way and stopping short of the target if blocked by something.
	**	Only the root component is swept and checked for blocking collision, child components move without sweeping. If collision is off, this has no effect.
	**/
	UPROPERTY(EditAnywhere)
	bool bSweep = true;
	
	// Begin UAnimNotify interface
	#if ENGINE_MAJOR_VERSION != 5 
		virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override;
	#endif

	#if	ENGINE_MAJOR_VERSION == 5
		virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	#endif
};
