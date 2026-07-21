#include "EGASAbilityFunctionLibrary.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Actor.h"
#include "GameplayEffect.h"

UAbilitySystemComponent* UEGASAbilityFunctionLibrary::GetAbilitySystemComponentFromActor(AActor* Actor)
{
	return UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Actor);
}

bool UEGASAbilityFunctionLibrary::GiveAbilitySetToActor(AActor* Actor, const UEGASAbilitySet* AbilitySet, FEGASAbilitySetGrantedHandles& OutGrantedHandles, UObject* SourceObject)
{
	UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActor(Actor);
	if (!AbilitySystemComponent || !AbilitySet || !AbilitySystemComponent->IsOwnerActorAuthoritative())
	{
		return false;
	}

	AbilitySet->GiveToAbilitySystem(AbilitySystemComponent, OutGrantedHandles, SourceObject ? SourceObject : Actor);
	return true;
}

bool UEGASAbilityFunctionLibrary::RemoveAbilitySetFromActor(AActor* Actor, const UEGASAbilitySet* AbilitySet, FEGASAbilitySetGrantedHandles& GrantedHandles)
{
	UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActor(Actor);
	if (!AbilitySystemComponent || !AbilitySet || !AbilitySystemComponent->IsOwnerActorAuthoritative())
	{
		return false;
	}

	AbilitySet->TakeFromAbilitySystem(AbilitySystemComponent, GrantedHandles);
	return true;
}

FActiveGameplayEffectHandle UEGASAbilityFunctionLibrary::ApplyGameplayEffectToActor(AActor* SourceActor, AActor* TargetActor, TSubclassOf<UGameplayEffect> GameplayEffectClass, float Level)
{
	UAbilitySystemComponent* TargetASC = GetAbilitySystemComponentFromActor(TargetActor);
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActor(SourceActor);

	if (!TargetASC || !GameplayEffectClass)
	{
		return FActiveGameplayEffectHandle();
	}

	if (!SourceASC)
	{
		SourceASC = TargetASC;
	}

	FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
	EffectContext.AddSourceObject(SourceActor ? SourceActor : TargetActor);

	const FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(GameplayEffectClass, Level, EffectContext);
	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		return FActiveGameplayEffectHandle();
	}

	return SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
}

float UEGASAbilityFunctionLibrary::GetNumericAttribute(AActor* Actor, FGameplayAttribute Attribute, bool& bFound)
{
	bFound = false;

	UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActor(Actor);
	if (!AbilitySystemComponent || !Attribute.IsValid())
	{
		return 0.0f;
	}

	bFound = true;
	return AbilitySystemComponent->GetNumericAttribute(Attribute);
}

bool UEGASAbilityFunctionLibrary::AddLooseGameplayTagsToActor(AActor* Actor, FGameplayTagContainer GameplayTags)
{
	UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActor(Actor);
	if (!AbilitySystemComponent)
	{
		return false;
	}

	AbilitySystemComponent->AddLooseGameplayTags(GameplayTags);
	return true;
}

bool UEGASAbilityFunctionLibrary::RemoveLooseGameplayTagsFromActor(AActor* Actor, FGameplayTagContainer GameplayTags)
{
	UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActor(Actor);
	if (!AbilitySystemComponent)
	{
		return false;
	}

	AbilitySystemComponent->RemoveLooseGameplayTags(GameplayTags);
	return true;
}

bool UEGASAbilityFunctionLibrary::HasMatchingGameplayTag(AActor* Actor, FGameplayTag GameplayTag)
{
	const UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActor(Actor);
	return AbilitySystemComponent && GameplayTag.IsValid() && AbilitySystemComponent->HasMatchingGameplayTag(GameplayTag);
}

bool UEGASAbilityFunctionLibrary::HasAnyMatchingGameplayTag(AActor* Actor, FGameplayTagContainer GameplayTags)
{
	const UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActor(Actor);
	return AbilitySystemComponent && AbilitySystemComponent->HasAnyMatchingGameplayTags(GameplayTags);
}

FString UEGASAbilityFunctionLibrary::GetDebugStringFromAttribute(FGameplayAttribute Attribute)
{
	return Attribute.GetName();
}

void UEGASAbilityFunctionLibrary::GetAllAttributes(TSubclassOf<UAttributeSet> AttributeSetClass, TArray<FGameplayAttribute>& OutAttributes)
{
	OutAttributes.Reset();

	const UClass* Class = AttributeSetClass.Get();
	if (!Class)
	{
		return;
	}

	for (TFieldIterator<FProperty> It(Class); It; ++It)
	{
		if (FFloatProperty* FloatProperty = CastField<FFloatProperty>(*It))
		{
			OutAttributes.Emplace(FloatProperty);
		}
		else if (FGameplayAttribute::IsGameplayAttributeDataProperty(*It))
		{
			OutAttributes.Emplace(*It);
		}
	}
}

bool UEGASAbilityFunctionLibrary::NotEqual_GameplayAttributeGameplayAttribute(FGameplayAttribute Attribute, FString AttributeName)
{
	return Attribute.GetName() != AttributeName;
}

void UEGASAbilityFunctionLibrary::ExecuteGameplayCueForActor(AActor* Actor, FGameplayTag GameplayCueTag, FGameplayEffectContextHandle Context)
{
	if (UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActor(Actor))
	{
		AbilitySystemComponent->ExecuteGameplayCue(GameplayCueTag, Context);
	}
}

void UEGASAbilityFunctionLibrary::ExecuteGameplayCueWithParams(AActor* Actor, FGameplayTag GameplayCueTag, const FGameplayCueParameters& GameplayCueParameters)
{
	if (UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActor(Actor))
	{
		AbilitySystemComponent->ExecuteGameplayCue(GameplayCueTag, GameplayCueParameters);
	}
}

void UEGASAbilityFunctionLibrary::AddGameplayCue(AActor* Actor, FGameplayTag GameplayCueTag, FGameplayEffectContextHandle Context)
{
	if (UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActor(Actor))
	{
		AbilitySystemComponent->AddGameplayCue(GameplayCueTag, Context);
	}
}

void UEGASAbilityFunctionLibrary::AddGameplayCueWithParams(AActor* Actor, FGameplayTag GameplayCueTag, const FGameplayCueParameters& GameplayCueParameters)
{
	if (UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActor(Actor))
	{
		AbilitySystemComponent->AddGameplayCue(GameplayCueTag, GameplayCueParameters);
	}
}

void UEGASAbilityFunctionLibrary::RemoveGameplayCue(AActor* Actor, FGameplayTag GameplayCueTag)
{
	if (UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActor(Actor))
	{
		AbilitySystemComponent->RemoveGameplayCue(GameplayCueTag);
	}
}

void UEGASAbilityFunctionLibrary::RemoveAllGameplayCues(AActor* Actor)
{
	if (UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActor(Actor))
	{
		AbilitySystemComponent->RemoveAllGameplayCues();
	}
}
