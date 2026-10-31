// Copyright Kemal Erdem YILMAZ. All Rights Reserved.


#include "EGPlayMontageNotify.h"


void UEGPlayMontageNotify::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	(void)Animation;
	(void)EventReference;

	if (MeshComp && MontageToPlay)
	{
		if(const auto animInstance = MeshComp->GetAnimInstance())
		{
			animInstance->Montage_Play(MontageToPlay,InPlayRate,EMontagePlayReturnType::MontageLength,InTimeToStartMontageAt);
		}
	}
}
