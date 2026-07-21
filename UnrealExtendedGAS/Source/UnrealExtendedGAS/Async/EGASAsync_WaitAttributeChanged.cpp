#include "Async/EGASAsync_WaitAttributeChanged.h"

#include "AbilitySystemComponent.h"

UEGASAsync_WaitAttributeChanged* UEGASAsync_WaitAttributeChanged::GAListenForAttributeChange(
	UAbilitySystemComponent* AbilitySystemComponent, FGameplayAttribute Attribute)
{
	UEGASAsync_WaitAttributeChanged* AsyncAction = NewObject<UEGASAsync_WaitAttributeChanged>();
	AsyncAction->SetAbilitySystemComponent(AbilitySystemComponent);
	AsyncAction->AttributesToListen.Add(Attribute);
	return AsyncAction;
}

UEGASAsync_WaitAttributeChanged* UEGASAsync_WaitAttributeChanged::GAListenForAttributesChange(
	UAbilitySystemComponent* AbilitySystemComponent, TArray<FGameplayAttribute> Attributes)
{
	UEGASAsync_WaitAttributeChanged* AsyncAction = NewObject<UEGASAsync_WaitAttributeChanged>();
	AsyncAction->SetAbilitySystemComponent(AbilitySystemComponent);
	AsyncAction->AttributesToListen = MoveTemp(Attributes);
	return AsyncAction;
}

void UEGASAsync_WaitAttributeChanged::Activate()
{
	Super::Activate();

	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC || AttributesToListen.IsEmpty())
	{
		EndAction();
		return;
	}

	TArray<FGameplayAttribute> UniqueAttributes;
	for (const FGameplayAttribute& Attribute : AttributesToListen)
	{
		if (Attribute.IsValid())
		{
			UniqueAttributes.AddUnique(Attribute);
		}
	}
	AttributesToListen = MoveTemp(UniqueAttributes);

	if (AttributesToListen.IsEmpty())
	{
		EndAction();
		return;
	}

	for (const FGameplayAttribute& Attribute : AttributesToListen)
	{
		FDelegateHandle Handle = ASC->GetGameplayAttributeValueChangeDelegate(Attribute).AddUObject(
			this, &ThisClass::HandleAttributeChanged);
		DelegateHandles.Add(Attribute, Handle);
	}
}

void UEGASAsync_WaitAttributeChanged::EndAction()
{
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		for (const TPair<FGameplayAttribute, FDelegateHandle>& Pair : DelegateHandles)
		{
			ASC->GetGameplayAttributeValueChangeDelegate(Pair.Key).Remove(Pair.Value);
		}
	}

	DelegateHandles.Reset();
	Super::EndAction();
}

void UEGASAsync_WaitAttributeChanged::EndTask()
{
	EndAction();
}

void UEGASAsync_WaitAttributeChanged::HandleAttributeChanged(const FOnAttributeChangeData& ChangeData)
{
	if (ShouldBroadcastDelegates())
	{
		OnAttributeChanged.Broadcast(ChangeData.Attribute, ChangeData.NewValue, ChangeData.OldValue);
	}
	else
	{
		EndAction();
	}
}
