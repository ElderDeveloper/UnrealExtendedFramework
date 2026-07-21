#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectComponent.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"

#include "EGASGameplayEffectComponent_AttributeRequirements.generated.h"

UENUM(BlueprintType)
enum class EEGASAttributeComparison : uint8
{
	EqualTo,
	NotEqualTo,
	GreaterThan,
	GreaterThanOrEqualTo,
	LessThan,
	LessThanOrEqualTo
};

USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAS_API FEGASAttributeRequirement
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Requirement")
	FGameplayAttribute Attribute;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Requirement")
	EEGASAttributeComparison Comparison = EEGASAttributeComparison::GreaterThanOrEqualTo;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Requirement")
	float Threshold = 0.0f;
};

UCLASS(DisplayName = "EGAS Attribute Requirements")
class UNREALEXTENDEDGAS_API UEGASGameplayEffectComponent_AttributeRequirements : public UGameplayEffectComponent
{
	GENERATED_BODY()

public:
	virtual bool CanGameplayEffectApply(const FActiveGameplayEffectsContainer& ActiveGEContainer, const FGameplayEffectSpec& GESpec) const override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Target Requirements")
	TArray<FEGASAttributeRequirement> TargetAttributeRequirements;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Target Requirements")
	FGameplayTagRequirements TargetTagRequirements;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Source Requirements")
	TArray<FEGASAttributeRequirement> SourceAttributeRequirements;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Source Requirements")
	FGameplayTagRequirements SourceTagRequirements;
};
