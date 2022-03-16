// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "UnrealExtendedFramework/Data/EFMacro.h"
#include "EGMoveForwardNotifyWindow.generated.h"


UCLASS()
class UNREALEXTENDEDGAMEPLAY_API UEGMoveForwardNotifyWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:

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

	#if ENGINE_MAJOR_VERSION != 5
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime) override;
	#endif

	#if ENGINE_MAJOR_VERSION == 5
		virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;
	#endif
};
