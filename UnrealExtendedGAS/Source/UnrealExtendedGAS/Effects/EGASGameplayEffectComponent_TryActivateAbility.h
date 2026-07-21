#pragma once

#include "Abilities/GameplayAbility.h"
#include "CoreMinimal.h"
#include "GameplayEffectComponent.h"
#include "GameplayTagContainer.h"

#include "EGASGameplayEffectComponent_TryActivateAbility.generated.h"

UENUM(BlueprintType)
enum class EEGASAbilityActivationType : uint8
{
	Tag UMETA(DisplayName = "Activate by Tag"),
	Class UMETA(DisplayName = "Activate by Class")
};

/** Attempts to activate an ability on the target after this Gameplay Effect is applied. */
UCLASS(DisplayName = "Try Activate Ability")
class UNREALEXTENDEDGAS_API UEGASGameplayEffectComponent_TryActivateAbility : public UGameplayEffectComponent
{
	GENERATED_BODY()

public:
	virtual void OnGameplayEffectApplied(
		FActiveGameplayEffectsContainer& ActiveGEContainer,
		FGameplayEffectSpec& GESpec,
		FPredictionKey& PredictionKey) const override;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Activation")
	EEGASAbilityActivationType ActivationType = EEGASAbilityActivationType::Tag;

	UPROPERTY(EditDefaultsOnly, Category = "Activation", meta = (EditCondition = "ActivationType == EEGASAbilityActivationType::Tag", EditConditionHides))
	FGameplayTag AbilityTag;

	UPROPERTY(EditDefaultsOnly, Category = "Activation", meta = (EditCondition = "ActivationType == EEGASAbilityActivationType::Class", EditConditionHides))
	TSubclassOf<UGameplayAbility> AbilityClass;
};
