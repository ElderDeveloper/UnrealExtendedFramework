#include "EGASAbilitySet.h"

#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "GameplayEffect.h"

void FEGASAbilitySetGrantedHandles::AddAbilitySpecHandle(const FGameplayAbilitySpecHandle& Handle)
{
	if (Handle.IsValid())
	{
		AbilitySpecHandles.Add(Handle);
	}
}

void FEGASAbilitySetGrantedHandles::AddGameplayEffectHandle(const FActiveGameplayEffectHandle& Handle)
{
	if (Handle.IsValid())
	{
		GameplayEffectHandles.Add(Handle);
	}
}

void FEGASAbilitySetGrantedHandles::AddAttributeSet(UAttributeSet* AttributeSet)
{
	if (AttributeSet)
	{
		GrantedAttributeSets.Add(AttributeSet);
	}
}

void FEGASAbilitySetGrantedHandles::TakeFromAbilitySystem(UAbilitySystemComponent* AbilitySystemComponent)
{
	if (!AbilitySystemComponent || !AbilitySystemComponent->IsOwnerActorAuthoritative())
	{
		return;
	}

	for (const FGameplayAbilitySpecHandle& AbilitySpecHandle : AbilitySpecHandles)
	{
		if (AbilitySpecHandle.IsValid())
		{
			AbilitySystemComponent->ClearAbility(AbilitySpecHandle);
		}
	}

	for (const FActiveGameplayEffectHandle& GameplayEffectHandle : GameplayEffectHandles)
	{
		if (GameplayEffectHandle.IsValid())
		{
			AbilitySystemComponent->RemoveActiveGameplayEffect(GameplayEffectHandle);
		}
	}

	for (UAttributeSet* AttributeSet : GrantedAttributeSets)
	{
		if (AttributeSet)
		{
			AbilitySystemComponent->RemoveSpawnedAttribute(AttributeSet);
		}
	}

	AbilitySpecHandles.Reset();
	GameplayEffectHandles.Reset();
	GrantedAttributeSets.Reset();
}

void UEGASAbilitySet::GiveToAbilitySystem(UAbilitySystemComponent* AbilitySystemComponent, FEGASAbilitySetGrantedHandles& OutGrantedHandles, UObject* SourceObject) const
{
	if (!AbilitySystemComponent || !AbilitySystemComponent->IsOwnerActorAuthoritative())
	{
		return;
	}

	for (const FEGASAbilitySet_AttributeSet& AttributeSetToGrant : GrantedAttributeSets)
	{
		if (!AttributeSetToGrant.AttributeSet)
		{
			continue;
		}

		UObject* AttributeOuter = AbilitySystemComponent->GetOwner();
		if (!AttributeOuter)
		{
			AttributeOuter = AbilitySystemComponent;
		}

		UAttributeSet* NewAttributeSet = NewObject<UAttributeSet>(AttributeOuter, AttributeSetToGrant.AttributeSet);
		AbilitySystemComponent->AddAttributeSetSubobject(NewAttributeSet);
		OutGrantedHandles.AddAttributeSet(NewAttributeSet);
	}

	for (const FEGASAbilitySet_GameplayAbility& AbilityToGrant : GrantedGameplayAbilities)
	{
		if (!AbilityToGrant.Ability)
		{
			continue;
		}

		FGameplayAbilitySpec AbilitySpec(AbilityToGrant.Ability, AbilityToGrant.AbilityLevel, AbilityToGrant.InputID, SourceObject);
		const FGameplayAbilitySpecHandle AbilitySpecHandle = AbilitySystemComponent->GiveAbility(AbilitySpec);
		OutGrantedHandles.AddAbilitySpecHandle(AbilitySpecHandle);
	}

	for (const FEGASAbilitySet_GameplayEffect& EffectToGrant : GrantedGameplayEffects)
	{
		if (!EffectToGrant.GameplayEffect)
		{
			continue;
		}

		const UGameplayEffect* GameplayEffect = EffectToGrant.GameplayEffect->GetDefaultObject<UGameplayEffect>();
		const FActiveGameplayEffectHandle GameplayEffectHandle = AbilitySystemComponent->ApplyGameplayEffectToSelf(GameplayEffect, EffectToGrant.EffectLevel, AbilitySystemComponent->MakeEffectContext());
		OutGrantedHandles.AddGameplayEffectHandle(GameplayEffectHandle);
	}
}

void UEGASAbilitySet::TakeFromAbilitySystem(UAbilitySystemComponent* AbilitySystemComponent, FEGASAbilitySetGrantedHandles& GrantedHandles) const
{
	GrantedHandles.TakeFromAbilitySystem(AbilitySystemComponent);
}
