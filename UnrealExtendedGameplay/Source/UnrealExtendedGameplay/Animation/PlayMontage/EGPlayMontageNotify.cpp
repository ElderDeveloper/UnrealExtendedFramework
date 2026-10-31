// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGPlayMontageNotify.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"

void UEGPlayMontageNotify::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	UAnimInstance* const AnimInstance = MeshComp ? MeshComp->GetAnimInstance() : nullptr;
	if (AnimInstance && MontageToPlay)
	{
		AnimInstance->Montage_Play(MontageToPlay, InPlayRate, EMontagePlayReturnType::MontageLength, InTimeToStartMontageAt);
	}
}
