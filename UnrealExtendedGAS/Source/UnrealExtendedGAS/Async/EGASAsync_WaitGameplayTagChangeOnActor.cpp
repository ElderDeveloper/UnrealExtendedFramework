// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Async/EGASAsync_WaitGameplayTagChangeOnActor.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"

UEGASAsync_WaitGameplayTagChangeOnActor* UEGASAsync_WaitGameplayTagChangeOnActor::WaitGameplayTagChangeOnActor(AActor* TargetActor, FGameplayTag Tag, bool bOnlyTriggerOnce)
{
	UEGASAsync_WaitGameplayTagChangeOnActor* AsyncNode = NewObject<UEGASAsync_WaitGameplayTagChangeOnActor>();
	AsyncNode->TargetActor = TargetActor;
	AsyncNode->Tag = Tag;
	AsyncNode->bOnlyTriggerOnce = bOnlyTriggerOnce;
	// Kept alive by the actor's game instance until it ends, so a listener is not collected mid-wait.
	AsyncNode->InitAsyncAction(TargetActor);
	return AsyncNode;
}

void UEGASAsync_WaitGameplayTagChangeOnActor::ExecuteAction()
{
	if (!TargetActor || !Tag.IsValid())
	{
		FailInvalidArgument(TEXT("TargetActor and Tag must be valid"));
		return;
	}

	AbilitySystemComponent = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
	if (!AbilitySystemComponent.IsValid())
	{
		FailAction(FEFResult::NotAvailable(GetResultSource(), TEXT("an ability system on TargetActor")));
		return;
	}

	DelegateHandle = AbilitySystemComponent->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &UEGASAsync_WaitGameplayTagChangeOnActor::HandleTagChanged);
}

void UEGASAsync_WaitGameplayTagChangeOnActor::EndTask()
{
	CancelWork();
	FinishAction();
}

void UEGASAsync_WaitGameplayTagChangeOnActor::CancelWork()
{
	if (AbilitySystemComponent.IsValid() && Tag.IsValid() && DelegateHandle.IsValid())
	{
		AbilitySystemComponent->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::NewOrRemoved).Remove(DelegateHandle);
	}
	DelegateHandle.Reset();
}

void UEGASAsync_WaitGameplayTagChangeOnActor::HandleTagChanged(const FGameplayTag ChangedTag, int32 NewCount)
{
	if (IsFinished())
	{
		return;
	}

	if (bOnlyTriggerOnce)
	{
		CancelWork();
		if (!FinishAction())
		{
			return;
		}
	}

	OnChanged.Broadcast(ChangedTag, NewCount);
}
