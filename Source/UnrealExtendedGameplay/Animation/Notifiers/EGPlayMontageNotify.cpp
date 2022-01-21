// Fill out your copyright notice in the Description page of Project Settings.


#include "EGPlayMontageNotify.h"


void UEGPlayMontageNotify::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation)
{
	if (MeshComp && MontageToPlay)
	{
		if(const auto animInstance = MeshComp->GetAnimInstance())
		{
			animInstance->Montage_Play(MontageToPlay,InPlayRate,EMontagePlayReturnType::MontageLength,InTimeToStartMontageAt);
		}
	}
}
