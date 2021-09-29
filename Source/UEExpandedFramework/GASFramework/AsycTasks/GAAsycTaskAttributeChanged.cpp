// Fill out your copyright notice in the Description page of Project Settings.


#include "GAAsycTaskAttributeChanged.h"

UGAAsycTaskAttributeChanged* UGAAsycTaskAttributeChanged::GAListenForAttributeChange(UAbilitySystemComponent* AbilitySystemComponent, FGameplayAttribute Attribute)
{
	UGAAsycTaskAttributeChanged* WaitForAttributeChanged = NewObject<UGAAsycTaskAttributeChanged>();
	WaitForAttributeChanged->ASC = AbilitySystemComponent;
	WaitForAttributeChanged->AttributeToListen = Attribute;

	if (!IsValid(AbilitySystemComponent) || !Attribute.IsValid())
	{
		WaitForAttributeChanged->RemoveFromRoot();
		return nullptr;
	}
	
	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(Attribute).AddUObject(WaitForAttributeChanged,&UGAAsycTaskAttributeChanged::AttributeChanged);
	return WaitForAttributeChanged;
}

UGAAsycTaskAttributeChanged* UGAAsycTaskAttributeChanged::GAListenForAttributesChange(UAbilitySystemComponent* AbilitySystemComponent, TArray<FGameplayAttribute> Attributes)
{
	UGAAsycTaskAttributeChanged* WaitForAttributeChanged = NewObject<UGAAsycTaskAttributeChanged>();
	WaitForAttributeChanged->ASC = AbilitySystemComponent;
	WaitForAttributeChanged->AttributesToListen = Attributes;

	if (!IsValid(AbilitySystemComponent) || Attributes.Num() < 1)
	{
		WaitForAttributeChanged->RemoveFromRoot();
		return nullptr;
	}
	for (const auto attribute : Attributes)
	{
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(attribute).AddUObject(WaitForAttributeChanged,&UGAAsycTaskAttributeChanged::AttributeChanged);
	}
	
	return WaitForAttributeChanged;
}



void UGAAsycTaskAttributeChanged::AttributeChanged(const FOnAttributeChangeData& Data)
{
	OnAttributeChanged.Broadcast(Data.Attribute,Data.NewValue,Data.OldValue);
}


void UGAAsycTaskAttributeChanged::EndTask()
{
	if (IsValid(ASC))
	{
		ASC->GetGameplayAttributeValueChangeDelegate(AttributeToListen).RemoveAll(this);

		for (auto Attribute : AttributesToListen)
		{
			ASC->GetGameplayAttributeValueChangeDelegate(Attribute).RemoveAll(this);
		}
	}
	SetReadyToDestroy();
	MarkPendingKill();
}