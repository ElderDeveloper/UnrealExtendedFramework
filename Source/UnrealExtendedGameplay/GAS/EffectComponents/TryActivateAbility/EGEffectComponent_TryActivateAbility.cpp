// ... existing code ...
#include "EGEffectComponent_TryActivateAbility.h"
#include "AbilitySystemComponent.h"



void UEGEffectComponent_TryActivateAbility::OnGameplayEffectApplied(FActiveGameplayEffectsContainer& ActiveGEContainer,FGameplayEffectSpec& GESpec, FPredictionKey& PredictionKey) const
{
	Super::OnGameplayEffectApplied(ActiveGEContainer, GESpec, PredictionKey);
	
	// We usually want to activate the ability on the owner of the container (the Target of the GE)
	UAbilitySystemComponent* TargetASC = ActiveGEContainer.Owner;

	if (!TargetASC)
	{
		return;
	}
	
	switch (ActivationType)
	{
	case EEGEffectAbilityActivationType::Tag:
		if (AbilityTag.IsValid())
		{
			// Try to activate any ability matching this tag
			TargetASC->TryActivateAbilitiesByTag(FGameplayTagContainer(AbilityTag));
		}
		break;

	case EEGEffectAbilityActivationType::Class:
		if (AbilityClass)
		{
			// Try to activate the specific ability class
			TargetASC->TryActivateAbilityByClass(AbilityClass);
		}
		break;
	}
}
