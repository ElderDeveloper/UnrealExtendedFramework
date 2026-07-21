// Fill out your copyright notice in the Description page of Project Settings.


#include "EFAsyncWaitGameplayTag.h"
#include "UnrealExtendedFramework/Systems/Ability/EFExtendedAbilityComponent.h"
#include "UnrealExtendedFramework/Systems/Ability/Abilities/EFExtendedAbility.h"

UEFAsyncWaitGameplayTagAdded* UEFAsyncWaitGameplayTagAdded::EFAsyncWaitGameplayTagAdded(UEFExtendedAbility* OwningAbility, FGameplayTag Tag, bool OnlyTriggerOnce)
{
	if (OwningAbility && Tag.IsValid())
	{
		if (const auto ActionObject = NewObject<UEFAsyncWaitGameplayTagAdded>())
		{
			ActionObject->SearchTag = Tag;
			ActionObject->bShouldDestroyOnce = OnlyTriggerOnce;
			ActionObject->Ability = OwningAbility;
			return ActionObject;
		}
	}
	return nullptr;
}

void UEFAsyncWaitGameplayTagAdded::OnGameplayTagAdded(UEFExtendedAbility* Instigator , FGameplayTag ChangedTag)
{
	if (ChangedTag == SearchTag)
	{
		Added.Broadcast(this);
		if (bShouldDestroyOnce)
		{
			MarkAsGarbage();
		}
	}
}

void UEFAsyncWaitGameplayTagAdded::Activate()
{
	Super::Activate();

	if (Ability)
	{
		Ability->GetOwnerExtendedComponent()->OnExtendedGameplayTagAdded.AddDynamic(this,&UEFAsyncWaitGameplayTagAdded::OnGameplayTagAdded);
	}
}



UEFAsyncWaitGameplayTagRemoved* UEFAsyncWaitGameplayTagRemoved::EFAsyncWaitGameplayTagRemoved(UEFExtendedAbility* OwningAbility, FGameplayTag Tag, bool OnlyTriggerOnce)
{
	if (OwningAbility && Tag.IsValid())
	{
		if (const auto ActionObject = NewObject<UEFAsyncWaitGameplayTagRemoved>())
		{
			ActionObject->SearchTag = Tag;
			ActionObject->bShouldDestroyOnce = OnlyTriggerOnce;
			ActionObject->Ability = OwningAbility;
			return ActionObject;
		}
	}
	return nullptr;
}

void UEFAsyncWaitGameplayTagRemoved::OnGameplayTagRemoved(UEFExtendedAbility* Instigator , FGameplayTag ChangedTag)
{
	if (ChangedTag == SearchTag)
	{
		Removed.Broadcast(this);
		if (bShouldDestroyOnce)
		{
			MarkAsGarbage();
		}
	}
}

void UEFAsyncWaitGameplayTagRemoved::Activate()
{
	Super::Activate();
	
	if (Ability)
	{
		Ability->GetOwnerExtendedComponent()->OnExtendedGameplayTagAdded.AddDynamic(this,&UEFAsyncWaitGameplayTagRemoved::OnGameplayTagRemoved);
	}
}
