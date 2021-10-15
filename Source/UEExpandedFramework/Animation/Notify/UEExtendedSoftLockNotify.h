// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "UEExpandedFramework/Gameplay/Trace/UEExtendedTraceData.h"

#include "UEExtendedSoftLockNotify.generated.h"

UCLASS()
class UEEXPANDEDFRAMEWORK_API UUEExtendedSoftLockNotify : public UAnimNotifyState
{
	GENERATED_BODY()


	UPROPERTY(EditAnywhere)
	FSphereTraceStruct SoftLockActorTrace;

	UPROPERTY(EditAnywhere)
	float SoftLockInterpSpeed;

protected:
	virtual void NotifyBegin(USkeletalMeshComponent * MeshComp, UAnimSequenceBase * Animation, float TotalDuration);
	virtual void NotifyTick(USkeletalMeshComponent * MeshComp, UAnimSequenceBase * Animation, float FrameDeltaTime);
	virtual void NotifyEnd(USkeletalMeshComponent * MeshComp, UAnimSequenceBase * Animation);

	void FindSoftLockActor(USkeletalMeshComponent* MeshComp);
private:

	AActor* SoftLockActor;
};

