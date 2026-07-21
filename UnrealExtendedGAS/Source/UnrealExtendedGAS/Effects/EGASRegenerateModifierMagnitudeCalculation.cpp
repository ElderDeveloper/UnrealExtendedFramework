#include "Effects/EGASRegenerateModifierMagnitudeCalculation.h"

#include "GameplayEffect.h"

float UEGASRegenerateModifierMagnitudeCalculation::CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const
{
	if (SetByCallerRateTag.IsValid())
	{
		const float SetByCallerRate = Spec.GetSetByCallerMagnitude(SetByCallerRateTag, false, DefaultRate);
		if (!FMath::IsNearlyEqual(SetByCallerRate, DefaultRate))
		{
			return SetByCallerRate;
		}
	}

	const float ScaledRate = RegenRate.GetValueAtLevel(Spec.GetLevel());
	if (!FMath::IsNearlyZero(ScaledRate))
	{
		return ScaledRate;
	}

	return DefaultRate;
}
