#include "Tasks/EGASAbilityTask_PlayMontageAndWaitEventTag.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Animation/AnimInstance.h"
#include "GameFramework/Character.h"

UEGASAbilityTask_PlayMontageAndWaitEventTag::UEGASAbilityTask_PlayMontageAndWaitEventTag()
{
}

UEGASAbilityTask_PlayMontageAndWaitEventTag* UEGASAbilityTask_PlayMontageAndWaitEventTag::PlayMontageAndWaitForEventTag(
	UGameplayAbility* OwningAbility,
	FName TaskInstanceName,
	UAnimMontage* InMontageToPlay,
	FGameplayTagContainer InEventTags,
	float InPlaybackRate,
	FName InStartSection,
	bool bInStopWhenAbilityEnds,
	float InAnimRootMotionTranslationScale)
{
	UAbilitySystemGlobals::NonShipping_ApplyGlobalAbilityScaler_Rate(InPlaybackRate);

	UEGASAbilityTask_PlayMontageAndWaitEventTag* Task =
		NewAbilityTask<UEGASAbilityTask_PlayMontageAndWaitEventTag>(OwningAbility, TaskInstanceName);
	Task->MontageToPlay = InMontageToPlay;
	Task->EventTags = MoveTemp(InEventTags);
	Task->PlaybackRate = InPlaybackRate;
	Task->StartSection = InStartSection;
	Task->bStopWhenAbilityEnds = bInStopWhenAbilityEnds;
	Task->AnimRootMotionTranslationScale = InAnimRootMotionTranslationScale;
	return Task;
}

void UEGASAbilityTask_PlayMontageAndWaitEventTag::Activate()
{
	if (!Ability || !MontageToPlay || !AbilitySystemComponent.IsValid())
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnMontageCancelled.Broadcast(FGameplayTag(), FGameplayEventData());
		}
		EndTask();
		return;
	}

	const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
	UAnimInstance* AnimInstance = ActorInfo ? ActorInfo->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnMontageCancelled.Broadcast(FGameplayTag(), FGameplayEventData());
		}
		EndTask();
		return;
	}

	MontageEventHandle = AbilitySystemComponent->AddGameplayEventTagContainerDelegate(
		EventTags,
		FGameplayEventTagMulticastDelegate::FDelegate::CreateUObject(
			this, &ThisClass::HandleGameplayEventReceived));

	const float MontageLength = AbilitySystemComponent->PlayMontage(
		Ability,
		Ability->GetCurrentActivationInfo(),
		MontageToPlay,
		PlaybackRate,
		StartSection);

	if (MontageLength <= 0.0f)
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnMontageCancelled.Broadcast(FGameplayTag(), FGameplayEventData());
		}
		EndTask();
		return;
	}

	MontageCancelledHandle = Ability->OnGameplayAbilityCancelled.AddUObject(
		this, &ThisClass::HandleAbilityCancelled);
	MontageBlendingOutDelegate.BindUObject(this, &ThisClass::HandleMontageBlendingOut);
	AnimInstance->Montage_SetBlendingOutDelegate(MontageBlendingOutDelegate, MontageToPlay);
	MontageEndedDelegate.BindUObject(this, &ThisClass::HandleMontageEnded);
	AnimInstance->Montage_SetEndDelegate(MontageEndedDelegate, MontageToPlay);

	if (ACharacter* Character = Cast<ACharacter>(GetAvatarActor()))
	{
		const bool bCanScaleRootMotion =
			Character->GetLocalRole() == ROLE_Authority ||
			(Character->GetLocalRole() == ROLE_AutonomousProxy &&
				Ability->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::LocalPredicted);
		if (bCanScaleRootMotion)
		{
			Character->SetAnimRootMotionTranslationScale(AnimRootMotionTranslationScale);
		}
	}

	SetWaitingOnAvatar();
}

void UEGASAbilityTask_PlayMontageAndWaitEventTag::ExternalCancel()
{
	HandleAbilityCancelled();
	Super::ExternalCancel();
}

FString UEGASAbilityTask_PlayMontageAndWaitEventTag::GetDebugString() const
{
	return FString::Printf(
		TEXT("PlayMontageAndWaitForEventTag. Montage: %s"),
		*GetNameSafe(MontageToPlay));
}

void UEGASAbilityTask_PlayMontageAndWaitEventTag::OnDestroy(bool bInOwnerFinished)
{
	if (Ability)
	{
		if (MontageCancelledHandle.IsValid())
		{
			Ability->OnGameplayAbilityCancelled.Remove(MontageCancelledHandle);
		}

		if (bInOwnerFinished && bStopWhenAbilityEnds)
		{
			StopPlayingMontage();
		}
	}

	if (AbilitySystemComponent.IsValid() && MontageEventHandle.IsValid())
	{
		AbilitySystemComponent->RemoveGameplayEventTagContainerDelegate(EventTags, MontageEventHandle);
	}

	ResetRootMotionScale();
	Super::OnDestroy(bInOwnerFinished);
}

bool UEGASAbilityTask_PlayMontageAndWaitEventTag::StopPlayingMontage() const
{
	if (!Ability || !AbilitySystemComponent.IsValid())
	{
		return false;
	}

	const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
	UAnimInstance* AnimInstance = ActorInfo ? ActorInfo->GetAnimInstance() : nullptr;
	if (!AnimInstance || AbilitySystemComponent->GetAnimatingAbility() != Ability ||
		AbilitySystemComponent->GetCurrentMontage() != MontageToPlay)
	{
		return false;
	}

	if (FAnimMontageInstance* MontageInstance = AnimInstance->GetActiveInstanceForMontage(MontageToPlay))
	{
		MontageInstance->OnMontageBlendingOutStarted.Unbind();
		MontageInstance->OnMontageEnded.Unbind();
	}

	AbilitySystemComponent->CurrentMontageStop();
	return true;
}

void UEGASAbilityTask_PlayMontageAndWaitEventTag::ResetRootMotionScale() const
{
	if (!Ability)
	{
		return;
	}

	if (ACharacter* Character = Cast<ACharacter>(GetAvatarActor()))
	{
		const bool bCanScaleRootMotion =
			Character->GetLocalRole() == ROLE_Authority ||
			(Character->GetLocalRole() == ROLE_AutonomousProxy &&
				Ability->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::LocalPredicted);
		if (bCanScaleRootMotion)
		{
			Character->SetAnimRootMotionTranslationScale(1.0f);
		}
	}
}

void UEGASAbilityTask_PlayMontageAndWaitEventTag::HandleMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted)
{
	if (!Ability || Ability->GetCurrentMontage() != MontageToPlay || Montage != MontageToPlay)
	{
		return;
	}

	AbilitySystemComponent->ClearAnimatingAbility(Ability);
	ResetRootMotionScale();

	if (!ShouldBroadcastAbilityTaskDelegates())
	{
		return;
	}

	if (bInterrupted)
	{
		bInterruptedBroadcast = true;
		OnMontageInterrupted.Broadcast(FGameplayTag(), FGameplayEventData());
	}
	else
	{
		OnMontageBlendOut.Broadcast(FGameplayTag(), FGameplayEventData());
	}
}

void UEGASAbilityTask_PlayMontageAndWaitEventTag::HandleAbilityCancelled()
{
	if (StopPlayingMontage() && ShouldBroadcastAbilityTaskDelegates())
	{
		OnMontageCancelled.Broadcast(FGameplayTag(), FGameplayEventData());
	}
}

void UEGASAbilityTask_PlayMontageAndWaitEventTag::HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage != MontageToPlay)
	{
		return;
	}

	if (ShouldBroadcastAbilityTaskDelegates())
	{
		if (bInterrupted && !bInterruptedBroadcast)
		{
			OnMontageInterrupted.Broadcast(FGameplayTag(), FGameplayEventData());
		}
		else if (!bInterrupted)
		{
			OnMontageCompleted.Broadcast(FGameplayTag(), FGameplayEventData());
		}
	}

	EndTask();
}

void UEGASAbilityTask_PlayMontageAndWaitEventTag::HandleGameplayEventReceived(
	FGameplayTag EventTag,
	const FGameplayEventData* Payload) const
{
	if (!Payload || !ShouldBroadcastAbilityTaskDelegates())
	{
		return;
	}

	FGameplayEventData EventData = *Payload;
	EventData.EventTag = EventTag;
	OnMontageEventReceived.Broadcast(EventTag, EventData);
}
