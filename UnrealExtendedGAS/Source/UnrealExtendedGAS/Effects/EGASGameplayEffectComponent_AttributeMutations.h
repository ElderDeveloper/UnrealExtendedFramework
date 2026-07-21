#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectComponent.h"
#include "GameplayEffectTypes.h"

#include "EGASGameplayEffectComponent_AttributeMutations.generated.h"

USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAS_API FEGASAttributeMutation
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mutation")
	FGameplayAttribute Attribute;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mutation")
	TEnumAsByte<EGameplayModOp::Type> ModifierOp = EGameplayModOp::Additive;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mutation")
	float Magnitude = 0.0f;
};

UCLASS(DisplayName = "EGAS Attribute Mutations")
class UNREALEXTENDEDGAS_API UEGASGameplayEffectComponent_AttributeMutations : public UGameplayEffectComponent
{
	GENERATED_BODY()

public:
	virtual void OnGameplayEffectApplied(FActiveGameplayEffectsContainer& ActiveGEContainer, FGameplayEffectSpec& GESpec, FPredictionKey& PredictionKey) const override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attribute Mutations")
	TArray<FEGASAttributeMutation> TargetMutations;
};
