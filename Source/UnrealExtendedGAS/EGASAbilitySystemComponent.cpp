#include "EGASAbilitySystemComponent.h"

#include "Abilities/GameplayAbility.h"
#include "GameplayEffect.h"

UEGASAbilitySystemComponent::UEGASAbilitySystemComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);
	SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
}

FGameplayAbilitySpecHandle UEGASAbilitySystemComponent::GrantAbility(TSubclassOf<UGameplayAbility> AbilityClass, int32 AbilityLevel, int32 InputID, UObject* SourceObject)
{
	if (!HasAuthorityToGrant() || !AbilityClass)
	{
		return FGameplayAbilitySpecHandle();
	}

	FGameplayAbilitySpec AbilitySpec(AbilityClass, AbilityLevel, InputID, SourceObject);
	return GiveAbility(AbilitySpec);
}

FActiveGameplayEffectHandle UEGASAbilitySystemComponent::GrantGameplayEffect(TSubclassOf<UGameplayEffect> GameplayEffectClass, float EffectLevel)
{
	if (!HasAuthorityToGrant() || !GameplayEffectClass)
	{
		return FActiveGameplayEffectHandle();
	}

	const UGameplayEffect* GameplayEffect = GameplayEffectClass->GetDefaultObject<UGameplayEffect>();
	return ApplyGameplayEffectToSelf(GameplayEffect, EffectLevel, MakeEffectContext());
}

UAttributeSet* UEGASAbilitySystemComponent::GrantAttributeSet(TSubclassOf<UAttributeSet> AttributeSetClass)
{
	if (!HasAuthorityToGrant() || !AttributeSetClass)
	{
		return nullptr;
	}

	UObject* AttributeOuter = GetOwner();
	if (!AttributeOuter)
	{
		AttributeOuter = this;
	}

	UAttributeSet* NewAttributeSet = NewObject<UAttributeSet>(AttributeOuter, AttributeSetClass);
	AddAttributeSetSubobject(NewAttributeSet);
	return NewAttributeSet;
}

void UEGASAbilitySystemComponent::RevokeAbility(FGameplayAbilitySpecHandle AbilityHandle)
{
	if (HasAuthorityToGrant() && AbilityHandle.IsValid())
	{
		ClearAbility(AbilityHandle);
	}
}

void UEGASAbilitySystemComponent::RevokeGameplayEffect(FActiveGameplayEffectHandle EffectHandle, int32 StacksToRemove)
{
	if (HasAuthorityToGrant() && EffectHandle.IsValid())
	{
		RemoveActiveGameplayEffect(EffectHandle, StacksToRemove);
	}
}

void UEGASAbilitySystemComponent::RevokeAttributeSet(UAttributeSet* AttributeSet)
{
	if (HasAuthorityToGrant() && AttributeSet)
	{
		RemoveSpawnedAttribute(AttributeSet);
	}
}

bool UEGASAbilitySystemComponent::HasAuthorityToGrant() const
{
	return IsOwnerActorAuthoritative();
}
