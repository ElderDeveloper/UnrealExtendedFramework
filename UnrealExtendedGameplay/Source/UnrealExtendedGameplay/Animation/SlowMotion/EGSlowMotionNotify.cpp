// Fill out your copyright notice in the Description page of Project Settings.


#include "EGSlowMotionNotify.h"

#include "Kismet/GameplayStatics.h"

UEGSlowMotionNotify::UEGSlowMotionNotify() : BeginSpeed(0.2) , EndSpeed(1)
{
	
}

#if ENGINE_MAJOR_VERSION != 5

void UEGSlowMotionNotify::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,float TotalDuration)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration);

	UGameplayStatics::SetGlobalTimeDilation(MeshComp->GetWorld(),BeginSpeed);
}

void UEGSlowMotionNotify::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation)
{
	Super::NotifyEnd(MeshComp, Animation);
	UGameplayStatics::SetGlobalTimeDilation(MeshComp->GetWorld(),EndSpeed);
}

#endif



#if ENGINE_MAJOR_VERSION == 5
void UEGSlowMotionNotify::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	UGameplayStatics::SetGlobalTimeDilation(MeshComp->GetWorld(),BeginSpeed);
}

void UEGSlowMotionNotify::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	UGameplayStatics::SetGlobalTimeDilation(MeshComp->GetWorld(),EndSpeed);
}

#endif