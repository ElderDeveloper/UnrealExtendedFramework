// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGInteractionTask_PlayMontageAndWait.h"

#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"

UEGInteractionTask_PlayMontageAndWait* UEGInteractionTask_PlayMontageAndWait::PlayMontageAndWait(UEGInteraction* OwningInteraction, UAnimMontage* Montage, float PlayRate, FName StartSection, bool bStopWhenInteractionEnds, USkeletalMeshComponent* MeshOverride)
{
	UEGInteractionTask_PlayMontageAndWait* Task = NewInteractionTask<UEGInteractionTask_PlayMontageAndWait>(OwningInteraction);
	if (Task)
	{
		Task->Montage = Montage;
		Task->PlayRate = PlayRate;
		Task->StartSection = StartSection;
		Task->bStopWhenInteractionEnds = bStopWhenInteractionEnds;
		Task->Mesh = MeshOverride ? MeshOverride : (OwningInteraction ? OwningInteraction->GetActorInfo().SkeletalMesh.Get() : nullptr);
	}
	return Task;
}

UAnimInstance* UEGInteractionTask_PlayMontageAndWait::GetAnimInstance() const
{
	return Mesh ? Mesh->GetAnimInstance() : nullptr;
}

void UEGInteractionTask_PlayMontageAndWait::Activate()
{
	Super::Activate();

	UAnimInstance* AnimInstance = GetAnimInstance();
	if (!AnimInstance || !Montage)
	{
		if (ShouldBroadcast())
		{
			OnCancelled.Broadcast();
		}
		EndTask();
		return;
	}

	const float Length = AnimInstance->Montage_Play(Montage, PlayRate, EMontagePlayReturnType::MontageLength, 0.0f, true);
	if (Length <= 0.0f)
	{
		if (ShouldBroadcast())
		{
			OnCancelled.Broadcast();
		}
		EndTask();
		return;
	}

	bPlayed = true;
	if (StartSection != NAME_None)
	{
		AnimInstance->Montage_JumpToSection(StartSection, Montage);
	}

	BlendingOutDelegate.BindUObject(this, &UEGInteractionTask_PlayMontageAndWait::OnMontageBlendingOut);
	AnimInstance->Montage_SetBlendingOutDelegate(BlendingOutDelegate, Montage);

	MontageEndedDelegate.BindUObject(this, &UEGInteractionTask_PlayMontageAndWait::OnMontageEnded);
	AnimInstance->Montage_SetEndDelegate(MontageEndedDelegate, Montage);
}

void UEGInteractionTask_PlayMontageAndWait::OnMontageBlendingOut(UAnimMontage* InMontage, bool bInterrupted)
{
	if (InMontage != Montage || !ShouldBroadcast())
	{
		return;
	}

	if (bInterrupted)
	{
		OnInterrupted.Broadcast();
	}
	else
	{
		OnBlendOut.Broadcast();
	}
}

void UEGInteractionTask_PlayMontageAndWait::OnMontageEnded(UAnimMontage* InMontage, bool bInterrupted)
{
	if (InMontage != Montage)
	{
		return;
	}

	if (!bInterrupted && ShouldBroadcast())
	{
		OnCompleted.Broadcast();
	}
	EndTask();
}

void UEGInteractionTask_PlayMontageAndWait::StopMontage()
{
	UAnimInstance* AnimInstance = GetAnimInstance();
	if (AnimInstance && bPlayed && Montage && AnimInstance->Montage_IsPlaying(Montage))
	{
		AnimInstance->Montage_Stop(Montage->BlendOut.GetBlendTime(), Montage);
	}
}

void UEGInteractionTask_PlayMontageAndWait::ExternalCancel()
{
	if (ShouldBroadcast())
	{
		OnCancelled.Broadcast();
	}
	StopMontage();
	EndTask();
}

void UEGInteractionTask_PlayMontageAndWait::OnDestroy(bool bInOwnerFinished)
{
	if (UAnimInstance* AnimInstance = GetAnimInstance())
	{
		if (bPlayed && Montage)
		{
			// Detach before the object goes away so a late montage callback cannot reach it.
			FOnMontageBlendingOutStarted EmptyBlendOut;
			AnimInstance->Montage_SetBlendingOutDelegate(EmptyBlendOut, Montage);
			FOnMontageEnded EmptyEnded;
			AnimInstance->Montage_SetEndDelegate(EmptyEnded, Montage);
		}
	}

	if (bInOwnerFinished && bStopWhenInteractionEnds)
	{
		StopMontage();
	}

	Super::OnDestroy(bInOwnerFinished);
}
