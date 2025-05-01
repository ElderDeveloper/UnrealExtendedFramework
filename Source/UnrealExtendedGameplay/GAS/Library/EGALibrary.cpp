// Fill out your copyright notice in the Description page of Project Settings.


#include "EGALibrary.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"






UAbilitySystemComponent* UEGALibrary::GetAbilitySystemComponentFromActor(AActor* Actor)
{
	if (const auto Interface = Cast<IAbilitySystemInterface>(Actor))
	{
		if (const auto AbilityComponent = Interface->GetAbilitySystemComponent())
		{
			return AbilityComponent;
		}
	}
	return nullptr;
}









bool UEGALibrary::AddLooseGameplayTagsToActor(AActor* Actor, const FGameplayTagContainer GameplayTags)
{
	UAbilitySystemComponent* AbilitySystemComponent = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Actor);

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->AddLooseGameplayTags(GameplayTags);
		return true;
	}

	return false;
}




bool UEGALibrary::RemoveLooseGameplayTagsFromActor(AActor* Actor,const FGameplayTagContainer GameplayTags)
{
	UAbilitySystemComponent* AbilitySystemComponent = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Actor);

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->RemoveLooseGameplayTags(GameplayTags);
		return true;
	}

	return false;
}




bool UEGALibrary::HasMatchingGameplayTag(AActor* Actor, const FGameplayTag GameplayTag)
{
	if (const auto AbilitySystemComponent= GetAbilitySystemComponentFromActor(Actor))
	{
		return AbilitySystemComponent->HasMatchingGameplayTag(GameplayTag);
	}
	return false;
}




bool UEGALibrary::HasAnyMatchingGameplayTag(AActor* Actor, const FGameplayTagContainer GameplayTags)
{
	if (const auto AbilitySystemComponent= GetAbilitySystemComponentFromActor(Actor))
	{
		return AbilitySystemComponent->HasAnyMatchingGameplayTags(GameplayTags);
	}
	return false;
}




FString UEGALibrary::GetDebugStringFromAttribute(FGameplayAttribute Attribute)
{
	return Attribute.GetName();
}




void UEGALibrary::GetAllAttributes(TSubclassOf<UAttributeSet> AttributeSetClass,TArray<FGameplayAttribute>& OutAttributes)
{
	const UClass* Class = AttributeSetClass.Get();
	for (TFieldIterator<FProperty> It(Class); It; ++It)
	{
		if (FFloatProperty* FloatProperty = CastField<FFloatProperty>(*It))
		{
			OutAttributes.Push(FGameplayAttribute(FloatProperty));
		}
		else if (FGameplayAttribute::IsGameplayAttributeDataProperty(*It))
		{
			OutAttributes.Push(FGameplayAttribute(*It));
		}
	}
}




bool UEGALibrary::NotEqual_GameplayAttributeGameplayAttribute(FGameplayAttribute A, FString B)
{
	return A.GetName() != B;
}






#pragma region Gameplay Cue Functions


void UEGALibrary::ExecuteGameplayCueForActor(AActor* Actor, FGameplayTag GameplayCueTag,FGameplayEffectContextHandle Context)
{
	if (const auto AbilitySystemComponent= GetAbilitySystemComponentFromActor(Actor))
	{
		AbilitySystemComponent->ExecuteGameplayCue(GameplayCueTag, Context);
	}
}




void UEGALibrary::ExecuteGameplayCueWithParams(AActor* Actor, FGameplayTag GameplayCueTag,const FGameplayCueParameters& GameplayCueParameters)
{
	if (const auto AbilitySystemComponent= GetAbilitySystemComponentFromActor(Actor))
	{
		AbilitySystemComponent->ExecuteGameplayCue(GameplayCueTag, GameplayCueParameters);
	}
}




void UEGALibrary::AddGameplayCue(AActor* Actor, FGameplayTag GameplayCueTag,FGameplayEffectContextHandle Context)
{
	if (const auto AbilitySystemComponent= GetAbilitySystemComponentFromActor(Actor))
	{
		AbilitySystemComponent->AddGameplayCue(GameplayCueTag, Context);
	}
}




void UEGALibrary::AddGameplayCueWithParams(AActor* Actor, FGameplayTag GameplayCueTag,const FGameplayCueParameters& GameplayCueParameter)
{
	if (const auto AbilitySystemComponent= GetAbilitySystemComponentFromActor(Actor))
	{
		AbilitySystemComponent->AddGameplayCue(GameplayCueTag, GameplayCueParameter);
	}
}




void UEGALibrary::RemoveGameplayCue(AActor* Actor, FGameplayTag GameplayCueTag)
{
	if (const auto AbilitySystemComponent= GetAbilitySystemComponentFromActor(Actor))
	{
		AbilitySystemComponent->RemoveGameplayCue(GameplayCueTag);
	}
}




void UEGALibrary::RemoveAllGameplayCues(AActor* Actor)
{
	if (const auto AbilitySystemComponent= GetAbilitySystemComponentFromActor(Actor))
	{
		AbilitySystemComponent->RemoveAllGameplayCues();
	}
}


#pragma endregion Gameplay Cue Functions

