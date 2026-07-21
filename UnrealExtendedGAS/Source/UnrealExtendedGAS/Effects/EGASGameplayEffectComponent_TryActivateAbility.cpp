#include "Effects/EGASGameplayEffectComponent_TryActivateAbility.h"

#include "AbilitySystemComponent.h"

void UEGASGameplayEffectComponent_TryActivateAbility::OnGameplayEffectApplied(
	FActiveGameplayEffectsContainer& ActiveGEContainer,
	FGameplayEffectSpec& GESpec,
	FPredictionKey& PredictionKey) const
{
	Super::OnGameplayEffectApplied(ActiveGEContainer, GESpec, PredictionKey);

	UAbilitySystemComponent* TargetAbilitySystem = ActiveGEContainer.Owner;
	if (!TargetAbilitySystem)
	{
		return;
	}

	switch (ActivationType)
	{
	case EEGASAbilityActivationType::Tag:
		if (AbilityTag.IsValid())
		{
			TargetAbilitySystem->TryActivateAbilitiesByTag(FGameplayTagContainer(AbilityTag));
		}
		break;

	case EEGASAbilityActivationType::Class:
		if (AbilityClass)
		{
			TargetAbilitySystem->TryActivateAbilityByClass(AbilityClass);
		}
		break;
	}
}
