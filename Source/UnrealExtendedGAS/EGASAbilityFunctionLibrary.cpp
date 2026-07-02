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
