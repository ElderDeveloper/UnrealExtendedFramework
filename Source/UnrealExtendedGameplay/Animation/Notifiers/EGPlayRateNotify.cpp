// Fill out your copyright notice in the Description page of Project Settings.


#include "EGPlayRateNotify.h"


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
