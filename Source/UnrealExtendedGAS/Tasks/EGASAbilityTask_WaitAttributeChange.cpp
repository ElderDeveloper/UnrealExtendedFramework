#include "Tasks/EGASAbilityTask_WaitAttributeChange.h"

#include "AbilitySystemComponent.h"

UEGASAbilityTask_WaitAttributeChange* UEGASAbilityTask_WaitAttributeChange::WaitAttributeChange(UGameplayAbility* OwningAbility, FGameplayAttribute Attribute, bool bOnlyTriggerOnce)
{
	UEGASAbilityTask_WaitAttributeChange* Task = NewAbilityTask<UEGASAbilityTask_WaitAttributeChange>(OwningAbility);
	Task->Attribute = Attribute;
	Task->bOnlyTriggerOnce = bOnlyTriggerOnce;
	return Task;
}

void UEGASAbilityTask_WaitAttributeChange::Activate()
{
	if (!AbilitySystemComponent.IsValid() || !Attribute.IsValid())
	{
		EndTask();
		return;
	}

	DelegateHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(Attribute).AddUObject(this, &UEGASAbilityTask_WaitAttributeChange::HandleAttributeChanged);
}

void UEGASAbilityTask_WaitAttributeChange::OnDestroy(bool bInOwnerFinished)
{
	if (AbilitySystemComponent.IsValid() && Attribute.IsValid() && DelegateHandle.IsValid())
	{
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(Attribute).Remove(DelegateHandle);
	}

	Super::OnDestroy(bInOwnerFinished);
}

void UEGASAbilityTask_WaitAttributeChange::HandleAttributeChanged(const FOnAttributeChangeData& ChangeData)
{
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnChanged.Broadcast(Attribute, ChangeData.NewValue, ChangeData.OldValue);
	}

	if (bOnlyTriggerOnce)
	{
		EndTask();
	}
}
