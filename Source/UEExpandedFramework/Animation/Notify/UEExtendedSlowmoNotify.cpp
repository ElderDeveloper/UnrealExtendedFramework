// Fill out your copyright notice in the Description page of Project Settings.


#include "UEExtendedSlowmoNotify.h"

#include "Kismet/GameplayStatics.h"

UUEExtendedSlowmoNotify::UUEExtendedSlowmoNotify() : BeginSpeed(0.2) , EndSpeed(1)
{
	
}

void UUEExtendedSlowmoNotify::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,float TotalDuration)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration);

	UGameplayStatics::SetGlobalTimeDilation(MeshComp->GetWorld(),BeginSpeed);
}

void UUEExtendedSlowmoNotify::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation)
{
	Super::NotifyEnd(MeshComp, Animation);
	UGameplayStatics::SetGlobalTimeDilation(MeshComp->GetWorld(),EndSpeed);
}
