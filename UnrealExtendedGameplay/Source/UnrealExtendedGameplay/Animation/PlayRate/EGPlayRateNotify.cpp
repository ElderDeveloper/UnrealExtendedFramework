// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGPlayRateNotify.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"

void UEGPlayRateNotify::SetPlayRate(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float PlayRate)
{
	UAnimInstance* const AnimInstance = MeshComp ? MeshComp->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		return;
	}

	// Null (every active montage) only when the notify is not on a montage. Targeting the owning
	// montage keeps the end of the window from re-timing a montage that has since replaced it.
	AnimInstance->Montage_SetPlayRate(Cast<UAnimMontage>(Animation), PlayRate);
}

void UEGPlayRateNotify::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	SetPlayRate(MeshComp, Animation, BeginPlayRate);
}

void UEGPlayRateNotify::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	SetPlayRate(MeshComp, Animation, EndPlayRate);
}
