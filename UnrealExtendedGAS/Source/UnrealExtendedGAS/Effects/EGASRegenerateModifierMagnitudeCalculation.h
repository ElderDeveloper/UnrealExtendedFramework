#pragma once

#include "CoreMinimal.h"
#include "GameplayModMagnitudeCalculation.h"
#include "GameplayTagContainer.h"
#include "ScalableFloat.h"

#include "EGASRegenerateModifierMagnitudeCalculation.generated.h"

UCLASS()
class UNREALEXTENDEDGAS_API UEGASRegenerateModifierMagnitudeCalculation : public UGameplayModMagnitudeCalculation
{
	GENERATED_BODY()

public:
	virtual float CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Regeneration")
	FScalableFloat RegenRate;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Regeneration")
	FGameplayTag SetByCallerRateTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Regeneration")
	float DefaultRate = 1.0f;
};
