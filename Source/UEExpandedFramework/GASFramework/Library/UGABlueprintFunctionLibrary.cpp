// Fill out your copyright notice in the Description page of Project Settings.


#include "UGABlueprintFunctionLibrary.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"


bool UUGABlueprintFunctionLibrary::IsInEditor()
{
	return GIsEditor;
}




UAbilitySystemComponent* UUGABlueprintFunctionLibrary::GetAbilitySystemComponentFromActor(AActor* Actor)
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




FString UUGABlueprintFunctionLibrary::GetProjectVersion()
{
	FString ProjectVersion;

	GConfig->GetString(
		TEXT("/Script/EngineSettings.GeneralProjectSettings"),
		TEXT("ProjectVersion"),
		ProjectVersion,
		GGameIni
	);

	return ProjectVersion;
}




bool UUGABlueprintFunctionLibrary::AddLooseGameplayTagsToActor(AActor* Actor, const FGameplayTagContainer GameplayTags)
{
	UAbilitySystemComponent* AbilitySystemComponent = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Actor);

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->AddLooseGameplayTags(GameplayTags);
		return true;
	}

	return false;
}




bool UUGABlueprintFunctionLibrary::RemoveLooseGameplayTagsFromActor(AActor* Actor,const FGameplayTagContainer GameplayTags)
{
	UAbilitySystemComponent* AbilitySystemComponent = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Actor);

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->RemoveLooseGameplayTags(GameplayTags);
		return true;
	}

	return false;
}




bool UUGABlueprintFunctionLibrary::HasMatchingGameplayTag(AActor* Actor, const FGameplayTag GameplayTag)
{
	if (const auto AbilitySystemComponent= GetAbilitySystemComponentFromActor(Actor))
	{
		return AbilitySystemComponent->HasMatchingGameplayTag(GameplayTag);
	}
	return false;
}




bool UUGABlueprintFunctionLibrary::HasAnyMatchingGameplayTag(AActor* Actor, const FGameplayTagContainer GameplayTags)
{
	if (const auto AbilitySystemComponent= GetAbilitySystemComponentFromActor(Actor))
	{
		return AbilitySystemComponent->HasAnyMatchingGameplayTags(GameplayTags);
	}
	return false;
}




FString UUGABlueprintFunctionLibrary::GetDebugStringFromAttribute(FGameplayAttribute Attribute)
{
	return Attribute.GetName();
}




void UUGABlueprintFunctionLibrary::GetAllAttributes(TSubclassOf<UAttributeSet> AttributeSetClass,TArray<FGameplayAttribute>& OutAttributes)
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




bool UUGABlueprintFunctionLibrary::NotEqual_GameplayAttributeGameplayAttribute(FGameplayAttribute A, FString B)
{
	return A.GetName() != B;
}






#pragma region Gameplay Cue Functions


void UUGABlueprintFunctionLibrary::ExecuteGameplayCueForActor(AActor* Actor, FGameplayTag GameplayCueTag,FGameplayEffectContextHandle Context)
{
	if (const auto AbilitySystemComponent= GetAbilitySystemComponentFromActor(Actor))
	{
		AbilitySystemComponent->ExecuteGameplayCue(GameplayCueTag, Context);
	}
}




void UUGABlueprintFunctionLibrary::ExecuteGameplayCueWithParams(AActor* Actor, FGameplayTag GameplayCueTag,const FGameplayCueParameters& GameplayCueParameters)
{
	if (const auto AbilitySystemComponent= GetAbilitySystemComponentFromActor(Actor))
	{
		AbilitySystemComponent->ExecuteGameplayCue(GameplayCueTag, GameplayCueParameters);
	}
}




void UUGABlueprintFunctionLibrary::AddGameplayCue(AActor* Actor, FGameplayTag GameplayCueTag,FGameplayEffectContextHandle Context)
{
	if (const auto AbilitySystemComponent= GetAbilitySystemComponentFromActor(Actor))
	{
		AbilitySystemComponent->AddGameplayCue(GameplayCueTag, Context);
	}
}




void UUGABlueprintFunctionLibrary::AddGameplayCueWithParams(AActor* Actor, FGameplayTag GameplayCueTag,const FGameplayCueParameters& GameplayCueParameter)
{
	if (const auto AbilitySystemComponent= GetAbilitySystemComponentFromActor(Actor))
	{
		AbilitySystemComponent->AddGameplayCue(GameplayCueTag, GameplayCueParameter);
	}
}




void UUGABlueprintFunctionLibrary::RemoveGameplayCue(AActor* Actor, FGameplayTag GameplayCueTag)
{
	if (const auto AbilitySystemComponent= GetAbilitySystemComponentFromActor(Actor))
	{
		AbilitySystemComponent->RemoveGameplayCue(GameplayCueTag);
	}
}




void UUGABlueprintFunctionLibrary::RemoveAllGameplayCues(AActor* Actor)
{
	if (const auto AbilitySystemComponent= GetAbilitySystemComponentFromActor(Actor))
	{
		AbilitySystemComponent->RemoveAllGameplayCues();
	}
}


#pragma endregion Gameplay Cue Functions

