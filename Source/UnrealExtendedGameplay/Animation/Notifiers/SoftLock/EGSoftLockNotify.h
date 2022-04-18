// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "UnrealExtendedFramework/Data/EFTraceData.h"
#include "UObject/Object.h"
#include "EGSoftLockNotify.generated.h"

/**
 * 
 */
UCLASS()
class UNREALEXTENDEDGAMEPLAY_API UEGSoftLockNotify : public UAnimNotifyState
{
	GENERATED_BODY()
	
	
	UPROPERTY(EditAnywhere)
	FSphereTraceStruct SoftLockActorTrace = FSphereTraceStruct(300);

	UPROPERTY(EditAnywhere)
	float SoftLockInterpSpeed = 5;

	
protected:
	#if ENGINE_MAJOR_VERSION != 5
		virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration) override;
		virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override;
		virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime) override;
	#endif

		


	#if ENGINE_MAJOR_VERSION == 5
		virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
		virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
		virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;
	#endif
		

	void FindSoftLockActor(USkeletalMeshComponent* MeshComp);
private:

	AActor* SoftLockActor = nullptr;
};
