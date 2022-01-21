// Fill out your copyright notice in the Description page of Project Settings.


#include "EGATaskPlayMontageAndWaitEvent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Character.h"


// Sets default values
UEGATaskPlayMontageAndWaitEvent::UEGATaskPlayMontageAndWaitEvent()
{
	PlaybackRate = 1.f;	bStopWhenAbilityEnds = true;
}

void UEGATaskPlayMontageAndWaitEvent::Activate()
{
	if (Ability == nullptr) return;

	bool bPlayedMontage = false;

	if (AbilitySystemComponent)
	{
		const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();

		if(const auto AnimInstance = ActorInfo->GetAnimInstance())
		{
			MontageEventHandle = AbilitySystemComponent->AddGameplayEventTagContainerDelegate
			(
				EventTags,FGameplayEventTagMulticastDelegate::FDelegate::CreateUObject
				(	this,&UEGATaskPlayMontageAndWaitEvent::OnGameplayEventReceived	)
			);


			if (AbilitySystemComponent-> PlayMontage(Ability,Ability->GetCurrentActivationInfo(),MontageToPlay,PlaybackRate,StartSection) > 0.f)
			{
				if (ShouldBroadcastAbilityTaskDelegates() == false)	return;
				
				MontageCancelledHandle = Ability->OnGameplayAbilityCancelled.AddUObject(this,&UEGATaskPlayMontageAndWaitEvent::OnAbilityCanceled);
				
				MontageBlendingOutDelegate.BindUObject(this,&UEGATaskPlayMontageAndWaitEvent::OnMontageBlendingOut);
				AnimInstance->Montage_SetBlendingOutDelegate(MontageBlendingOutDelegate,MontageToPlay);

				MontageEndedDelegate.BindUObject(this,&UEGATaskPlayMontageAndWaitEvent::OnMontageEnded);
				AnimInstance->Montage_SetEndDelegate(MontageEndedDelegate,MontageToPlay);

				if(auto const Character = Cast<ACharacter>(GetAvatarActor()))
				{
					if (Character->GetLocalRole() == ROLE_Authority || Character->GetLocalRole() == ROLE_AutonomousProxy
						&&	Ability->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::LocalPredicted)
					{
						Character->SetAnimRootMotionTranslationScale(AnimRootMotionTranslationScale);
					}

					bPlayedMontage = true;
				}
			}
		}
		else
		{
			ABILITY_LOG(Warning, TEXT(" UEGATaskPlayMontageAndWaitEvent call to PlayMontage failed!"));
		}
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("UGSFAbilityTask_PlayMontageAndWaitForEvent called on invalid AbilitySystemComponent"));
	}

	if (!bPlayedMontage)
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnMontageCancelled.Broadcast(FGameplayTag(), FGameplayEventData());
		}
	}
}

void UEGATaskPlayMontageAndWaitEvent::ExternalCancel()
{
	check(AbilitySystemComponent);
	OnAbilityCanceled();
	Super::ExternalCancel();
}

FString UEGATaskPlayMontageAndWaitEvent::GetDebugString() const
{
	return Super::GetDebugString();
}

void UEGATaskPlayMontageAndWaitEvent::OnDestroy(bool bInOwnerFinished)
{
	if (Ability)
	{
		Ability->OnGameplayAbilityCancelled.Remove(MontageCancelledHandle);
		if (bInOwnerFinished && bStopWhenAbilityEnds)
		{
			StopPlayingMontage();
		}
	}

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->RemoveGameplayEventTagContainerDelegate(EventTags,MontageEventHandle);
	}

	Super::OnDestroy(bInOwnerFinished);
}

UEGATaskPlayMontageAndWaitEvent* UEGATaskPlayMontageAndWaitEvent::PlayMontageAndWaitForEvent(
	UGameplayAbility* OwningAbility, FName TaskInstanceName, UAnimMontage* MontageToPlay,
	FGameplayTagContainer EventTags, float PlaybackRate, FName StartSection, bool bStopWhenAbilityEnds,
	float AnimRootMotionTranslationScale)
{

	UAbilitySystemGlobals::NonShipping_ApplyGlobalAbilityScaler_Rate(PlaybackRate);

	UEGATaskPlayMontageAndWaitEvent* MyObj = NewAbilityTask<UEGATaskPlayMontageAndWaitEvent>(OwningAbility, TaskInstanceName);
	MyObj->MontageToPlay = MontageToPlay;
	MyObj->EventTags = EventTags;
	MyObj->PlaybackRate = PlaybackRate;
	MyObj->StartSection = StartSection;
	MyObj->AnimRootMotionTranslationScale = AnimRootMotionTranslationScale;
	MyObj->bStopWhenAbilityEnds = bStopWhenAbilityEnds;

	return MyObj;
}

bool UEGATaskPlayMontageAndWaitEvent::StopPlayingMontage() const
{
	if(const auto ActorInfo = Ability->GetCurrentActorInfo())
	{
		if (const auto AnimInstance = ActorInfo->GetAnimInstance())
		{
			if (AbilitySystemComponent && Ability)
			{
				if (AbilitySystemComponent->GetAnimatingAbility() == Ability
					&& AbilitySystemComponent->GetCurrentMontage() == MontageToPlay )
				{
					if (const auto MontageInstance = AnimInstance->GetActiveInstanceForMontage(MontageToPlay))
					{
						MontageInstance->OnMontageBlendingOutStarted.Unbind();
						MontageInstance->OnMontageEnded.Unbind();
					}

					AbilitySystemComponent->CurrentMontageStop();
					return true;
				}
			}
		}
	}

	return false;
}


void UEGATaskPlayMontageAndWaitEvent::OnMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted) const
{
	if (Ability)
	{
		if (Ability->GetCurrentMontage() == MontageToPlay && Montage == MontageToPlay)
		{
			AbilitySystemComponent->ClearAnimatingAbility(Ability);
			
			if (const auto Character = Cast<ACharacter>(GetAvatarActor()))
			{
				if (Character->GetLocalRole() == ROLE_Authority || Character->GetLocalRole() == ROLE_AutonomousProxy
					&& Ability->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::LocalPredicted)
				{
					Character->SetAnimRootMotionTranslationScale(1.f);
				}
			}
		}
	}
}

void UEGATaskPlayMontageAndWaitEvent::OnAbilityCanceled() const
{
	if (StopPlayingMontage())
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnMontageCancelled.Broadcast(FGameplayTag(),FGameplayEventData());
		}
	}
}

void UEGATaskPlayMontageAndWaitEvent::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (!bInterrupted)
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnMontageCompleted.Broadcast(FGameplayTag(),FGameplayEventData());
		}
	}
	EndTask();
}

void UEGATaskPlayMontageAndWaitEvent::OnGameplayEventReceived(FGameplayTag EventTag,
	const FGameplayEventData* Payload) const
{
	if(ShouldBroadcastAbilityTaskDelegates())
	{
		FGameplayEventData TempData = *Payload;
		TempData.EventTag = EventTag;

		OnMontageEventReceived.Broadcast(EventTag,TempData);
	}
}

