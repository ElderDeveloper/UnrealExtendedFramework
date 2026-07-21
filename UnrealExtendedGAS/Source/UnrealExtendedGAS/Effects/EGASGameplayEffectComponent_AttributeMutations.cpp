#include "Effects/EGASGameplayEffectComponent_AttributeMutations.h"

#include "AbilitySystemComponent.h"

void UEGASGameplayEffectComponent_AttributeMutations::OnGameplayEffectApplied(FActiveGameplayEffectsContainer& ActiveGEContainer, FGameplayEffectSpec& GESpec, FPredictionKey& PredictionKey) const
{
	Super::OnGameplayEffectApplied(ActiveGEContainer, GESpec, PredictionKey);

	UAbilitySystemComponent* TargetASC = ActiveGEContainer.Owner;
	if (!TargetASC || !TargetASC->IsOwnerActorAuthoritative())
	{
		return;
	}

	for (const FEGASAttributeMutation& Mutation : TargetMutations)
	{
		if (Mutation.Attribute.IsValid())
		{
			TargetASC->ApplyModToAttribute(Mutation.Attribute, Mutation.ModifierOp, Mutation.Magnitude);
		}
	}
}
