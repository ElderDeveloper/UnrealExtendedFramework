// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGStopMontageNotify.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"

void UEGStopMontageNotify::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	UAnimInstance* const AnimInstance = MeshComp ? MeshComp->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		return;
	}

	if (MontageToStop)
	{
		AnimInstance->Montage_Stop(BlendOutTime, MontageToStop);
	}
	else
	{
		AnimInstance->StopAllMontages(BlendOutTime);
	}
}
