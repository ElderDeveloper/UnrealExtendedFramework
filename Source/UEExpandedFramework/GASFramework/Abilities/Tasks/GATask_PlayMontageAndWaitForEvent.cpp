// Fill out your copyright notice in the Description page of Project Settings.


#include "GATask_PlayMontageAndWaitForEvent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Character.h"


// Sets default values
UGATask_PlayMontageAndWaitForEvent::UGATask_PlayMontageAndWaitForEvent()
{
	PlaybackRate = 1.f;	bStopWhenAbilityEnds = true;
}

void UGATask_PlayMontageAndWaitForEvent::Activate()
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
				(	this,&UGATask_PlayMontageAndWaitForEvent::OnGameplayEventReceived	)
			);


			if (AbilitySystemComponent-> PlayMontage(Ability,Ability->GetCurrentActivationInfo(),MontageToPlay,PlaybackRate,StartSection) > 0.f)
			{
				if (ShouldBroadcastAbilityTaskDelegates() == false)	return;
				
				MontageCancelledHandle = Ability->OnGameplayAbilityCancelled.AddUObject(this,&UGATask_PlayMontageAndWaitForEvent::OnAbilityCanceled);
				
				MontageBlendingOutDelegate.BindUObject(this,&UGATask_PlayMontageAndWaitForEvent::OnMontageBlendingOut);
				AnimInstance->Montage_SetBlendingOutDelegate(MontageBlendingOutDelegate,MontageToPlay);

				MontageEndedDelegate.BindUObject(this,&UGATask_PlayMontageAndWaitForEvent::OnMontageEnded);
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
			ABILITY_LOG(Warning, TEXT(" UGATask_PlayMontageAndWaitForEvent call to PlayMontage failed!"));
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

void UGATask_PlayMontageAndWaitForEvent::ExternalCancel()
{
	check(AbilitySystemComponent);
	OnAbilityCanceled();
	Super::ExternalCancel();
}

FString UGATask_PlayMontageAndWaitForEvent::GetDebugString() const
{
	return Super::GetDebugString();
}

void UGATask_PlayMontageAndWaitForEvent::OnDestroy(bool bInOwnerFinished)
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

UGATask_PlayMontageAndWaitForEvent* UGATask_PlayMontageAndWaitForEvent::PlayMontageAndWaitForEvent(
	UGameplayAbility* OwningAbility, FName TaskInstanceName, UAnimMontage* MontageToPlay,
	FGameplayTagContainer EventTags, float PlaybackRate, FName StartSection, bool bStopWhenAbilityEnds,
	float AnimRootMotionTranslationScale)
{

	UAbilitySystemGlobals::NonShipping_ApplyGlobalAbilityScaler_Rate(PlaybackRate);

	UGATask_PlayMontageAndWaitForEvent* MyObj = NewAbilityTask<UGATask_PlayMontageAndWaitForEvent>(OwningAbility, TaskInstanceName);
	MyObj->MontageToPlay = MontageToPlay;
	MyObj->EventTags = EventTags;
	MyObj->PlaybackRate = PlaybackRate;
	MyObj->StartSection = StartSection;
	MyObj->AnimRootMotionTranslationScale = AnimRootMotionTranslationScale;
	MyObj->bStopWhenAbilityEnds = bStopWhenAbilityEnds;

	return MyObj;
}

bool UGATask_PlayMontageAndWaitForEvent::StopPlayingMontage() const
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


void UGATask_PlayMontageAndWaitForEvent::OnMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted) const
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

void UGATask_PlayMontageAndWaitForEvent::OnAbilityCanceled() const
{
	if (StopPlayingMontage())
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnMontageCancelled.Broadcast(FGameplayTag(),FGameplayEventData());
		}
	}
}

void UGATask_PlayMontageAndWaitForEvent::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
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

void UGATask_PlayMontageAndWaitForEvent::OnGameplayEventReceived(FGameplayTag EventTag,
	const FGameplayEventData* Payload) const
{
	if(ShouldBroadcastAbilityTaskDelegates())
	{
		FGameplayEventData TempData = *Payload;
		TempData.EventTag = EventTag;

		OnMontageEventReceived.Broadcast(EventTag,TempData);
	}
}

