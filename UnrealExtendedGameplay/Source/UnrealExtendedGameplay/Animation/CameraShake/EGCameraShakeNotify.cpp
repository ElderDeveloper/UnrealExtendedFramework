// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGCameraShakeNotify.h"

#include "Camera/CameraShakeBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"

FString UEGCameraShakeNotify::GetNotifyName_Implementation() const
{
	return CameraShake ? CameraShake->GetName() : Super::GetNotifyName_Implementation();
}

void UEGCameraShakeNotify::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (MeshComp && CameraShake)
	{
		UGameplayStatics::PlayWorldCameraShake(MeshComp, CameraShake, MeshComp->GetComponentLocation(), InnerRadius, OuterRadius, Falloff);
	}
}
