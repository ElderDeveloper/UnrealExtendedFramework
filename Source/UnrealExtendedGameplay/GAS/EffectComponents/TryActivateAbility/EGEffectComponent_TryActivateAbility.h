// ... existing code ...
#include "CoreMinimal.h"
#include "GameplayEffectComponent.h"
#include "GameplayTagContainer.h"
#include "Abilities/GameplayAbility.h"
#include "EGEffectComponent_TryActivateAbility.generated.h"

UENUM(BlueprintType)
enum class EEGEffectAbilityActivationType : uint8
{
	Tag		UMETA(DisplayName = "Activate by Tag"),
	Class	UMETA(DisplayName = "Activate by Class")
};

/**
 * 
 */
UCLASS(DisplayName = "Try Activate Ability")
class UNREALEXTENDEDGAMEPLAY_API UEGEffectComponent_TryActivateAbility : public UGameplayEffectComponent
{
	GENERATED_BODY()

public:
	virtual void OnGameplayEffectApplied(FActiveGameplayEffectsContainer& ActiveGEContainer, FGameplayEffectSpec& GESpec, FPredictionKey& PredictionKey) const override;

protected:
	/** How should we try to activate the ability? */
	UPROPERTY(EditDefaultsOnly, Category = "Activation")
	EEGEffectAbilityActivationType ActivationType = EEGEffectAbilityActivationType::Tag;

	/** The Tag of the ability to activate (if using Tag mode) */
	UPROPERTY(EditDefaultsOnly, Category = "Activation", meta = (EditCondition = "ActivationType == EEGEffectAbilityActivationType::Tag", EditConditionHides))
	FGameplayTag AbilityTag;

	/** The Class of the ability to activate (if using Class mode) */
	UPROPERTY(EditDefaultsOnly, Category = "Activation", meta = (EditCondition = "ActivationType == EEGEffectAbilityActivationType::Class", EditConditionHides))
	TSubclassOf<UGameplayAbility> AbilityClass;
};