// Fill out your copyright notice in the Description page of Project Settings.


#include "EGPlayRateNotify.h"


#if ENGINE_MAJOR_VERSION != 5
void UEGPlayRateNotify::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration);

	MeshComp->GetAnimInstance()->Montage_SetPlayRate(nullptr,BeginPlayRate);
}
	

void UEGPlayRateNotify::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation)
{
	Super::NotifyEnd(MeshComp, Animation);
	
	MeshComp->GetAnimInstance()->Montage_SetPlayRate(nullptr,EndPlayRate);
}
#endif




#if ENGINE_MAJOR_VERSION == 5
void UEGPlayRateNotify::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration,const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (MeshComp)
	{
		if (MeshComp->GetAnimInstance())
			MeshComp->GetAnimInstance()->Montage_SetPlayRate(nullptr,BeginPlayRate);
	}

}

void UEGPlayRateNotify::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (MeshComp)
	{
		if (MeshComp->GetAnimInstance())
			MeshComp->GetAnimInstance()->Montage_SetPlayRate(nullptr,EndPlayRate);
	}
	
}
#endif
