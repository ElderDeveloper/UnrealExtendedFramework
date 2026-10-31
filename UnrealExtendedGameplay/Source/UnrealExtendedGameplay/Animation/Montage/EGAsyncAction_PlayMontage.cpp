// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGAsyncAction_PlayMontage.h"

#include "Animation/AnimMontage.h"
#include "EGAnimationSet.h"
#include "EGMontageLibrary.h"
#include "EGMontagePlayerInterface.h"
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

	// Pick now (so a Sequential pick advances once), load in Activate. Same lookup as the library.
	TArray<const UEGAnimationSet*, TInlineAllocator<2>> Sets;
	UEGMontageLibrary::GetAnimationSets(Actor, FallbackSet, Sets);
	FEGAnimationSetEntry Entry;
	TSoftObjectPtr<UAnimMontage> Picked;
	if (UEGAnimationSet::PickVariantInSets(Sets, AnimationTag, false, VariantIndex, Picked, Entry))
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

	if (Result == EEGMontagePlayResult::Played)
	{
		// We started this instance, so its single-slot delegates are ours.
		FOnMontageBlendingOutStarted BlendingOut = FOnMontageBlendingOutStarted::CreateUObject(this, &UEGAsyncAction_PlayMontage::HandleBlendingOut);
		Instance->Montage_SetBlendingOutDelegate(BlendingOut, Montage);

		FOnMontageEnded Ended = FOnMontageEnded::CreateUObject(this, &UEGAsyncAction_PlayMontage::HandleEnded);
		Instance->Montage_SetEndDelegate(Ended, Montage);
	}
	else
	{
		// The owner started it and may have bound those delegates; listen on the shared events.
		bUsesSharedDelegates = true;
		Instance->OnMontageBlendingOut.AddUniqueDynamic(this, &UEGAsyncAction_PlayMontage::HandleSharedBlendingOut);
		Instance->OnMontageEnded.AddUniqueDynamic(this, &UEGAsyncAction_PlayMontage::HandleSharedEnded);
	}

	Instance->OnPlayMontageNotifyBegin.AddUniqueDynamic(this, &UEGAsyncAction_PlayMontage::HandleNotifyBegin);
	Instance->OnPlayMontageNotifyEnd.AddUniqueDynamic(this, &UEGAsyncAction_PlayMontage::HandleNotifyEnd);
	Instance->OnAllMontageInstancesEnded.AddUniqueDynamic(this, &UEGAsyncAction_PlayMontage::HandleAllMontagesEnded);
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

bool UEGAsyncAction_PlayMontage::IsEventForOurInstance(const UAnimMontage* InMontage) const
{
	if (InMontage != Montage)
	{
		return false;
	}
	// Blending-out and ended instances are no longer "active", so while ours still is, the event
	// came from an older play of the same montage.
	const UAnimInstance* const Instance = AnimInstance.Get();
	const FAnimMontageInstance* Active = Instance ? Instance->GetActiveInstanceForMontage(Montage) : nullptr;
	return !(Active && Active->GetInstanceID() == MontageInstanceId);
}

void UEGAsyncAction_PlayMontage::HandleSharedBlendingOut(UAnimMontage* InMontage, bool bInterrupted)
{
	if (IsEventForOurInstance(InMontage))
	{
		HandleBlendingOut(InMontage, bInterrupted);
	}
}

void UEGAsyncAction_PlayMontage::HandleSharedEnded(UAnimMontage* InMontage, bool bInterrupted)
{
	if (IsEventForOurInstance(InMontage))
	{
		HandleEnded(InMontage, bInterrupted);
	}
}

void UEGAsyncAction_PlayMontage::HandleEnded(UAnimMontage* InMontage, bool bInterrupted)
{
	if (bFinished || InMontage != Montage)
	{
		return;
	}

	// Finish before broadcasting: a listener that destroys the owner uninitializes the anim
	// instance, and HandleAllMontagesEnded must not report the end a second time.
	const bool bReportInterrupted = bInterrupted && !bInterruptedReported;
	Finish();
	if (!bInterrupted)
	{
		OnCompleted.Broadcast(NAME_None);
	}
	else if (bReportInterrupted)
	{
		OnInterrupted.Broadcast(NAME_None);
	}
}

void UEGAsyncAction_PlayMontage::HandleAllMontagesEnded()
{
	// Any other way of ending has delivered our ended event (and finished the node) before the
	// last instance is deleted; only an uninitialized anim instance deletes them without one.
	if (bFinished)
	{
		return;
	}
	const bool bReportInterrupted = !bInterruptedReported;
	Finish();
	if (bReportInterrupted)
	{
		OnInterrupted.Broadcast(NAME_None);
	}
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
		// The owner played it (GAS, a replicated montage system), so the owner stops it: stopping it
		// here only would leave it playing wherever the owner replicated it.
		AActor* const Target = Actor.Get();
		const bool bStoppedByOwner = bUsesSharedDelegates && Target && Target->Implements<UEGMontagePlayerInterface>()
			&& IEGMontagePlayerInterface::Execute_StopMontageOverride(Target, Montage, BlendOutTime, MeshPurpose);
		if (!bStoppedByOwner)
		{
			Instance->Montage_Stop(BlendOutTime, Montage);
		}
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
		Instance->OnAllMontageInstancesEnded.RemoveDynamic(this, &UEGAsyncAction_PlayMontage::HandleAllMontagesEnded);

		if (bUsesSharedDelegates)
		{
			Instance->OnMontageBlendingOut.RemoveDynamic(this, &UEGAsyncAction_PlayMontage::HandleSharedBlendingOut);
			Instance->OnMontageEnded.RemoveDynamic(this, &UEGAsyncAction_PlayMontage::HandleSharedEnded);
		}
		// Only detach the per-montage delegates when we bound them and they are still ours: a newer
		// play of the same montage has its own, and an owner-played montage's belong to the owner.
		else if (Montage)
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
