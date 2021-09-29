// Fill out your copyright notice in the Description page of Project Settings.


#include "UEExtendedCameraShakeNotify.h"

#include "Kismet/GameplayStatics.h"

UUEExtendedCameraShakeNotify::UUEExtendedCameraShakeNotify() : OuterRadius(1000.f) , Falloff(1.f)
{
}

FString UUEExtendedCameraShakeNotify::GetNotifyName_Implementation() const
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

void UUEExtendedCameraShakeNotify::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation)
{
	Super::Notify(MeshComp, Animation);

	if (CameraShake)
	{
		UGameplayStatics::PlayWorldCameraShake(MeshComp->GetWorld(),CameraShake,MeshComp->GetComponentLocation(),InnerRadius,OuterRadius,Falloff);
	}

}
