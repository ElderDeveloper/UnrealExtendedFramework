// Fill out your copyright notice in the Description page of Project Settings.

#include "EFAsyncAttributeChanged.h"
#include "UnrealExtendedFramework/Systems/Attribute/EFAttributeComponent.h"


UEFAsyncAttributeChanged* UEFAsyncAttributeChanged::EFAsyncAttributeChanged(UEFAttributeComponent* AttributeComponent, FGameplayTag AttributeTag, bool OnlyTriggerOnce)
{
	if (AttributeComponent && AttributeTag.IsValid())
	{
		if (const auto ActionObject = NewObject<UEFAsyncAttributeChanged>())
		{
			ActionObject->OwningAttributeComponent = AttributeComponent;
			ActionObject->bOnlyTriggerOnce = OnlyTriggerOnce;
			ActionObject->CheckAttributeTag = AttributeTag;
			return ActionObject;
		}
	}
	return nullptr;
}


void UEFAsyncAttributeChanged::OnAttributeChanged(const FExtendedAttribute& Attribute)
{
	if (Attribute.AttributeTag == CheckAttributeTag)
	{
		AsyncAttributeChanged.Broadcast(Attribute.AttributeValue);

		if (bOnlyTriggerOnce && OwningAttributeComponent)
		{
			OwningAttributeComponent->OnAttributeModified.RemoveDynamic(this,&UEFAsyncAttributeChanged::OnAttributeChanged);
			MarkAsGarbage();
		}
	}
}

void UEFAsyncAttributeChanged::Activate()
{
	Super::Activate();

	if(OwningAttributeComponent)
	{
		OwningAttributeComponent->OnAttributeModified.AddDynamic(this,&UEFAsyncAttributeChanged::OnAttributeChanged);
	}
}
