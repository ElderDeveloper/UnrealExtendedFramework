// Fill out your copyright notice in the Description page of Project Settings.


#include "EGCameraShakeNotify.h"

#include "Kismet/GameplayStatics.h"

FString UEGCameraShakeNotify::GetNotifyName_Implementation() const
{
	if (CameraShake)
	{
		return CameraShake->GetName();
	}
	else
	{
		return Super::GetNotifyName_Implementation();
	}
}

void UEGCameraShakeNotify::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation)
{
	Super::Notify(MeshComp, Animation);

	if (CameraShake)
	{
		UGameplayStatics::PlayWorldCameraShake(MeshComp->GetWorld(),CameraShake,MeshComp->GetComponentLocation(),InnerRadius,OuterRadius,Falloff);
	}

}
