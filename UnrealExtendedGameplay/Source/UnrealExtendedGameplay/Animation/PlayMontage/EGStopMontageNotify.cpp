// Fill out your copyright notice in the Description page of Project Settings.


#include "EGStopMontageNotify.h"


#if ENGINE_MAJOR_VERSION != 5
void UEGStopMontageNotify::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation)
{
	Super::Notify(MeshComp, Animation);

	if (MontageToStop) MeshComp->GetAnimInstance()->Montage_Stop(BlendOutTime,MontageToStop);
	else MeshComp->GetAnimInstance()->StopAllMontages(BlendOutTime);
}
#endif

#if ENGINE_MAJOR_VERSION == 5
void UEGStopMontageNotify::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	if (MontageToStop) MeshComp->GetAnimInstance()->Montage_Stop(BlendOutTime,MontageToStop);
	else MeshComp->GetAnimInstance()->StopAllMontages(BlendOutTime);
}
#endif