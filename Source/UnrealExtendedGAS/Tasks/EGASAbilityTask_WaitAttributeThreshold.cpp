#include "Tasks/EGASAbilityTask_WaitAttributeThreshold.h"

#include "AbilitySystemComponent.h"

UEGASAbilityTask_WaitAttributeThreshold* UEGASAbilityTask_WaitAttributeThreshold::WaitAttributeThreshold(UGameplayAbility* OwningAbility, FGameplayAttribute Attribute, float Threshold, EEGASAttributeThresholdComparison Comparison, bool bOnlyTriggerOnce, bool bTriggerIfAlreadySatisfied)
{
	UEGASAbilityTask_WaitAttributeThreshold* Task = NewAbilityTask<UEGASAbilityTask_WaitAttributeThreshold>(OwningAbility);
	Task->Attribute = Attribute;
	Task->Threshold = Threshold;
	Task->Comparison = Comparison;
	Task->bOnlyTriggerOnce = bOnlyTriggerOnce;
	Task->bTriggerIfAlreadySatisfied = bTriggerIfAlreadySatisfied;
	return Task;
}

void UEGASAbilityTask_WaitAttributeThreshold::Activate()
{
	if (!AbilitySystemComponent.IsValid() || !Attribute.IsValid())
	{
		EndTask();
		return;
	}

	DelegateHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(Attribute).AddUObject(this, &UEGASAbilityTask_WaitAttributeThreshold::HandleAttributeChanged);

	if (bTriggerIfAlreadySatisfied)
	{
		const float CurrentValue = AbilitySystemComponent->GetNumericAttribute(Attribute);
		if (IsThresholdMet(CurrentValue))
		{
			if (ShouldBroadcastAbilityTaskDelegates())
			{
				OnThresholdMet.Broadcast(Attribute, CurrentValue, Threshold);
			}
			if (bOnlyTriggerOnce)
			{
				EndTask();
			}
		}
	}
}

void UEGASAbilityTask_WaitAttributeThreshold::OnDestroy(bool bInOwnerFinished)
{
	if (AbilitySystemComponent.IsValid() && Attribute.IsValid() && DelegateHandle.IsValid())
	{
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(Attribute).Remove(DelegateHandle);
	}

	Super::OnDestroy(bInOwnerFinished);
}

void UEGASAbilityTask_WaitAttributeThreshold::HandleAttributeChanged(const FOnAttributeChangeData& ChangeData)
{
	if (!IsThresholdMet(ChangeData.NewValue))
	{
		return;
	}

	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnThresholdMet.Broadcast(Attribute, ChangeData.NewValue, Threshold);
	}

	if (bOnlyTriggerOnce)
	{
		EndTask();
	}
}

bool UEGASAbilityTask_WaitAttributeThreshold::IsThresholdMet(float Value) const
{
	switch (Comparison)
	{
	case EEGASAttributeThresholdComparison::LessThan:
		return Value < Threshold;
	case EEGASAttributeThresholdComparison::LessThanOrEqual:
		return Value <= Threshold;
	case EEGASAttributeThresholdComparison::GreaterThan:
		return Value > Threshold;
	case EEGASAttributeThresholdComparison::GreaterThanOrEqual:
		return Value >= Threshold;
	case EEGASAttributeThresholdComparison::Equal:
		return FMath::IsNearlyEqual(Value, Threshold);
	case EEGASAttributeThresholdComparison::NotEqual:
		return !FMath::IsNearlyEqual(Value, Threshold);
	default:
		return false;
	}
}
