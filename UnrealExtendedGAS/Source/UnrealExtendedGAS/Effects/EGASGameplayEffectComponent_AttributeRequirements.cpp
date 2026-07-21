#include "Effects/EGASGameplayEffectComponent_AttributeRequirements.h"

#include "AbilitySystemComponent.h"

namespace EGASAttributeRequirements
{
	bool Compare(float Value, EEGASAttributeComparison Comparison, float Threshold)
	{
		switch (Comparison)
		{
		case EEGASAttributeComparison::EqualTo:
			return FMath::IsNearlyEqual(Value, Threshold);
		case EEGASAttributeComparison::NotEqualTo:
			return !FMath::IsNearlyEqual(Value, Threshold);
		case EEGASAttributeComparison::GreaterThan:
			return Value > Threshold;
		case EEGASAttributeComparison::GreaterThanOrEqualTo:
			return Value >= Threshold;
		case EEGASAttributeComparison::LessThan:
			return Value < Threshold;
		case EEGASAttributeComparison::LessThanOrEqualTo:
			return Value <= Threshold;
		default:
			return false;
		}
	}

	bool MeetsAttributeRequirements(const UAbilitySystemComponent* AbilitySystemComponent, const TArray<FEGASAttributeRequirement>& Requirements)
	{
		if (Requirements.IsEmpty())
		{
			return true;
		}

		if (!AbilitySystemComponent)
		{
			return false;
		}

		for (const FEGASAttributeRequirement& Requirement : Requirements)
		{
			if (!Requirement.Attribute.IsValid())
			{
				continue;
			}

			const float CurrentValue = AbilitySystemComponent->GetNumericAttribute(Requirement.Attribute);
			if (!Compare(CurrentValue, Requirement.Comparison, Requirement.Threshold))
			{
				return false;
			}
		}

		return true;
	}

	bool MeetsTagRequirements(const UAbilitySystemComponent* AbilitySystemComponent, const FGameplayTagRequirements& Requirements)
	{
		if (Requirements.IsEmpty())
		{
			return true;
		}

		if (!AbilitySystemComponent)
		{
			return false;
		}

		FGameplayTagContainer OwnedTags;
		AbilitySystemComponent->GetOwnedGameplayTags(OwnedTags);
		return Requirements.RequirementsMet(OwnedTags);
	}
}

bool UEGASGameplayEffectComponent_AttributeRequirements::CanGameplayEffectApply(const FActiveGameplayEffectsContainer& ActiveGEContainer, const FGameplayEffectSpec& GESpec) const
{
	if (!Super::CanGameplayEffectApply(ActiveGEContainer, GESpec))
	{
		return false;
	}

	const UAbilitySystemComponent* TargetASC = ActiveGEContainer.Owner;
	const UAbilitySystemComponent* SourceASC = GESpec.GetContext().GetInstigatorAbilitySystemComponent();

	return EGASAttributeRequirements::MeetsAttributeRequirements(TargetASC, TargetAttributeRequirements)
		&& EGASAttributeRequirements::MeetsTagRequirements(TargetASC, TargetTagRequirements)
		&& EGASAttributeRequirements::MeetsAttributeRequirements(SourceASC, SourceAttributeRequirements)
		&& EGASAttributeRequirements::MeetsTagRequirements(SourceASC, SourceTagRequirements);
}
