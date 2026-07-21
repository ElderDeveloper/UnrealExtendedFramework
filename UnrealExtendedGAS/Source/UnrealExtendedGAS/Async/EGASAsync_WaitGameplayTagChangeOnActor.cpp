#include "Async/EGASAsync_WaitGameplayTagChangeOnActor.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"

UEGASAsync_WaitGameplayTagChangeOnActor* UEGASAsync_WaitGameplayTagChangeOnActor::WaitGameplayTagChangeOnActor(AActor* TargetActor, FGameplayTag Tag, bool bOnlyTriggerOnce)
{
	UEGASAsync_WaitGameplayTagChangeOnActor* AsyncNode = NewObject<UEGASAsync_WaitGameplayTagChangeOnActor>();
	AsyncNode->TargetActor = TargetActor;
	AsyncNode->Tag = Tag;
	AsyncNode->bOnlyTriggerOnce = bOnlyTriggerOnce;
	return AsyncNode;
}

void UEGASAsync_WaitGameplayTagChangeOnActor::Activate()
{
	if (!TargetActor || !Tag.IsValid())
	{
		EndTask();
		return;
	}

	AbilitySystemComponent = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
	if (!AbilitySystemComponent.IsValid())
	{
		EndTask();
		return;
	}

	DelegateHandle = AbilitySystemComponent->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &UEGASAsync_WaitGameplayTagChangeOnActor::HandleTagChanged);
}

void UEGASAsync_WaitGameplayTagChangeOnActor::EndTask()
{
	if (AbilitySystemComponent.IsValid() && Tag.IsValid() && DelegateHandle.IsValid())
	{
		AbilitySystemComponent->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::NewOrRemoved).Remove(DelegateHandle);
	}

	SetReadyToDestroy();
}

void UEGASAsync_WaitGameplayTagChangeOnActor::HandleTagChanged(const FGameplayTag ChangedTag, int32 NewCount)
{
	OnChanged.Broadcast(ChangedTag, NewCount);

	if (bOnlyTriggerOnce)
	{
		EndTask();
	}
}
