// Fill out your copyright notice in the Description page of Project Settings.


#include "EGAAsyncAttributeChanged.h"



UEGAAsyncAttributeChanged* UEGAAsyncAttributeChanged::GAListenForAttributeChange(UAbilitySystemComponent* AbilitySystemComponent, FGameplayAttribute Attribute)
{
	UEGAAsyncAttributeChanged* WaitForAttributeChanged = NewObject<UEGAAsyncAttributeChanged>();
	WaitForAttributeChanged->ASC = AbilitySystemComponent;
	WaitForAttributeChanged->AttributeToListen = Attribute;

	if (!IsValid(AbilitySystemComponent) || !Attribute.IsValid())
	{
		WaitForAttributeChanged->RemoveFromRoot();
		return nullptr;
	}
	
	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(Attribute).AddUObject(WaitForAttributeChanged,&UEGAAsyncAttributeChanged::AttributeChanged);
	return WaitForAttributeChanged;
}

UEGAAsyncAttributeChanged* UEGAAsyncAttributeChanged::GAListenForAttributesChange(UAbilitySystemComponent* AbilitySystemComponent, TArray<FGameplayAttribute> Attributes)
{
	UEGAAsyncAttributeChanged* WaitForAttributeChanged = NewObject<UEGAAsyncAttributeChanged>();
	WaitForAttributeChanged->ASC = AbilitySystemComponent;
	WaitForAttributeChanged->AttributesToListen = Attributes;

	if (!IsValid(AbilitySystemComponent) || Attributes.Num() < 1)
	{
		WaitForAttributeChanged->RemoveFromRoot();
		return nullptr;
	}
	for (const auto attribute : Attributes)
	{
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(attribute).AddUObject(WaitForAttributeChanged,&UEGAAsyncAttributeChanged::AttributeChanged);
	}
	
	return WaitForAttributeChanged;
}



void UEGAAsyncAttributeChanged::AttributeChanged(const FOnAttributeChangeData& Data)
{
	OnAttributeChanged.Broadcast(Data.Attribute,Data.NewValue,Data.OldValue);
}


void UEGAAsyncAttributeChanged::EndTask()
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
	ConditionalBeginDestroy();
}