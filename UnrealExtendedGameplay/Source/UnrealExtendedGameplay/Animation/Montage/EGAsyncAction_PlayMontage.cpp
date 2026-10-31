// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGAsyncAction_PlayMontage.h"

#include "Animation/AnimMontage.h"
#include "EGAnimationSet.h"
#include "EGMontageLibrary.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"

UEGAsyncAction_PlayMontage* UEGAsyncAction_PlayMontage::PlayMontageAndWaitForNotify(AActor* Actor, UAnimMontage* Montage, float PlayRate, FName StartSection, float StartTime, FGameplayTag MeshPurpose)
{
	UEGAsyncAction_PlayMontage* Action = NewObject<UEGAsyncAction_PlayMontage>();
	Action->Actor = Actor;
	Action->Montage = Montage;
	Action->PlayRate = PlayRate;
	Action->StartSection = StartSection;
	Action->StartTime = StartTime;
	Action->MeshPurpose = MeshPurpose;
	Action->RegisterWithGameInstance(Actor);
	return Action;
}

UEGAsyncAction_PlayMontage* UEGAsyncAction_PlayMontage::PlayAnimationAndWaitForNotify(AActor* Actor, FGameplayTag AnimationTag, int32 VariantIndex, FGameplayTag MeshPurpose, UEGAnimationSet* FallbackSet)
{
	UEGAsyncAction_PlayMontage* Action = NewObject<UEGAsyncAction_PlayMontage>();
	Action->Actor = Actor;
	Action->MeshPurpose = MeshPurpose;

	// Pick now (so a Sequential pick advances once), load in Activate.
	const UEGAnimationSet* Set = UEGMontageLibrary::GetAnimationSet(Actor, FallbackSet);
	FEGAnimationSetEntry Entry;
	TSoftObjectPtr<UAnimMontage> Picked;
	bool bPicked = Set && Set->PickVariant(AnimationTag, VariantIndex, Picked, Entry);
	if (!bPicked && FallbackSet && FallbackSet != Set)
	{
		bPicked = FallbackSet->PickVariant(AnimationTag, VariantIndex, Picked, Entry);
	}
	if (bPicked)
	{
		Action->PendingMontage = Picked;
		Action->PlayRate = Entry.PlayRate;
		Action->StartSection = Entry.StartSection;
		Action->StartTime = Entry.StartTime;
	}

	Action->RegisterWithGameInstance(Actor);
	return Action;
}

void UEGAsyncAction_PlayMontage::Activate()
{
	Super::Activate();

	if (!Montage && !PendingMontage.IsNull())
	{
		Montage = PendingMontage.Get();
		if (!Montage)
		{
			LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
				PendingMontage.ToSoftObjectPath(),
				FStreamableDelegate::CreateUObject(this, &UEGAsyncAction_PlayMontage::HandleLoaded));
			if (LoadHandle.IsValid())
			{
				return;
			}
		}
	}

	StartPlayback();
}

void UEGAsyncAction_PlayMontage::HandleLoaded()
{
	if (bFinished)
	{
		return;
	}
	Montage = PendingMontage.Get();
	LoadHandle.Reset();
	StartPlayback();
}

void UEGAsyncAction_PlayMontage::StartPlayback()
{
	AActor* const Target = Actor.Get();
	UAnimInstance* Instance = nullptr;
	const EEGMontagePlayResult Result = Target && Montage
		? UEGMontageLibrary::PlayMontage(Target, Montage, PlayRate, StartSection, StartTime, MeshPurpose, Instance)
		: EEGMontagePlayResult::Failed;

	if (Result == EEGMontagePlayResult::Failed)
	{
		OnFailed.Broadcast(NAME_None);
		Finish();
		return;
	}

	// Handled by the owner: follow it only if it ended up on the anim instance we can see.
	const FAnimMontageInstance* MontageInstance = Instance ? Instance->GetActiveInstanceForMontage(Montage) : nullptr;
	if (!MontageInstance)
	{
		OnCompleted.Broadcast(NAME_None);
		Finish();
		return;
	}

	AnimInstance = Instance;
	MontageInstanceId = MontageInstance->GetInstanceID();

	FOnMontageBlendingOutStarted BlendingOut = FOnMontageBlendingOutStarted::CreateUObject(this, &UEGAsyncAction_PlayMontage::HandleBlendingOut);
	Instance->Montage_SetBlendingOutDelegate(BlendingOut, Montage);

	FOnMontageEnded Ended = FOnMontageEnded::CreateUObject(this, &UEGAsyncAction_PlayMontage::HandleEnded);
	Instance->Montage_SetEndDelegate(Ended, Montage);

	Instance->OnPlayMontageNotifyBegin.AddUniqueDynamic(this, &UEGAsyncAction_PlayMontage::HandleNotifyBegin);
	Instance->OnPlayMontageNotifyEnd.AddUniqueDynamic(this, &UEGAsyncAction_PlayMontage::HandleNotifyEnd);
}

void UEGAsyncAction_PlayMontage::HandleNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& Payload)
{
	if (!bFinished && Payload.MontageInstanceID == MontageInstanceId)
	{
		OnNotifyBegin.Broadcast(NotifyName);
	}
}

void UEGAsyncAction_PlayMontage::HandleNotifyEnd(FName NotifyName, const FBranchingPointNotifyPayload& Payload)
{
	if (!bFinished && Payload.MontageInstanceID == MontageInstanceId)
	{
		OnNotifyEnd.Broadcast(NotifyName);
	}
}

void UEGAsyncAction_PlayMontage::HandleBlendingOut(UAnimMontage* InMontage, bool bInterrupted)
{
	if (bFinished || InMontage != Montage)
	{
		return;
	}

	if (bInterrupted)
	{
		bInterruptedReported = true;
		OnInterrupted.Broadcast(NAME_None);
	}
	else
	{
		OnBlendOut.Broadcast(NAME_None);
	}
}

void UEGAsyncAction_PlayMontage::HandleEnded(UAnimMontage* InMontage, bool bInterrupted)
{
	if (bFinished || InMontage != Montage)
	{
		return;
	}

	if (!bInterrupted)
	{
		OnCompleted.Broadcast(NAME_None);
	}
	else if (!bInterruptedReported)
	{
		OnInterrupted.Broadcast(NAME_None);
	}
	Finish();
}

void UEGAsyncAction_PlayMontage::Cancel(float BlendOutTime)
{
	if (bFinished)
	{
		return;
	}

	UAnimInstance* const Instance = AnimInstance.Get();
	const FAnimMontageInstance* MontageInstance = Instance && Montage ? Instance->GetActiveInstanceForMontage(Montage) : nullptr;
	const bool bStillOurs = MontageInstance && MontageInstance->GetInstanceID() == MontageInstanceId;

	// Finish first so the stop does not report an interruption.
	Finish();
	if (bStillOurs)
	{
		Instance->Montage_Stop(BlendOutTime, Montage);
	}
}

void UEGAsyncAction_PlayMontage::Finish()
{
	if (bFinished)
	{
		return;
	}
	bFinished = true;

	if (LoadHandle.IsValid())
	{
		LoadHandle->CancelHandle();
		LoadHandle.Reset();
	}

	if (UAnimInstance* const Instance = AnimInstance.Get())
	{
		Instance->OnPlayMontageNotifyBegin.RemoveDynamic(this, &UEGAsyncAction_PlayMontage::HandleNotifyBegin);
		Instance->OnPlayMontageNotifyEnd.RemoveDynamic(this, &UEGAsyncAction_PlayMontage::HandleNotifyEnd);

		// Only detach the montage delegates while they are still ours: a newer play of the same
		// montage has its own.
		if (Montage)
		{
			const FAnimMontageInstance* MontageInstance = Instance->GetActiveInstanceForMontage(Montage);
			if (MontageInstance && MontageInstance->GetInstanceID() == MontageInstanceId)
			{
				FOnMontageBlendingOutStarted EmptyBlendOut;
				Instance->Montage_SetBlendingOutDelegate(EmptyBlendOut, Montage);
				FOnMontageEnded EmptyEnded;
				Instance->Montage_SetEndDelegate(EmptyEnded, Montage);
			}
		}
	}
	AnimInstance.Reset();
	SetReadyToDestroy();
}
