// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTDecorator.h"
#include "AttributeSet.h"
#include "BehaviorTree/Decorators/BTDecorator_BlackboardBase.h"
#include "GameplayEffectTypes.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "EFBTDecorator_GameplayAttribute.generated.h"

/**
 * Decorator for comparing Gameplay Attributes with static values or other attributes
 * Supports multiple comparison types and can track attribute changes for dynamic re-evaluation
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFBTDecorator_GameplayAttribute : public UBTDecorator
{
	GENERATED_BODY()

public:
	UEFBTDecorator_GameplayAttribute();

	// Whether to track attribute changes and re-evaluate the decorator dynamically
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	bool bTrackAttributeChanges = true;

	// Target actor to check the attribute on
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	FBlackboardKeySelector TargetActorKey;

	// The attribute to check
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	FGameplayAttribute AttributeToCheck;

	// Comparison type: 0=Equal, 1=NotEqual, 2=Greater, 3=GreaterOrEqual, 4=Less, 5=LessOrEqual
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (ClampMin = "0", ClampMax = "5"))
	uint8 ComparisonType = 2; // Default to Greater

	// Whether to compare with another attribute instead of a static value
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	bool bCompareWithAttribute = false;

	// Static value to compare against (used when bCompareWithAttribute is false)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (EditCondition = "!bCompareWithAttribute"))
	float ComparisonValue = 0.0f;

	// Second actor for attribute comparison (used when bCompareWithAttribute is true)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (EditCondition = "bCompareWithAttribute"))
	FBlackboardKeySelector SecondTargetActorKey;

	// Second attribute to compare with (used when bCompareWithAttribute is true)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (EditCondition = "bCompareWithAttribute"))
	FGameplayAttribute SecondAttributeToCheck;

protected:
	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;
	virtual FString GetStaticDescription() const override;

	virtual void OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

private:
	// Callback for when attribute values change
	void OnAttributeChanged(const FOnAttributeChangeData& Data);

	// Perform the comparison based on the comparison type
	bool PerformComparison(float Value1, float Value2) const;

	// Get the comparison operator string for display
	FString GetComparisonOperatorString() const;

	// Store weak reference to the ability system component(s) we're observing
	UPROPERTY()
	TWeakObjectPtr<class UAbilitySystemComponent> ObservedAbilitySystemComponent;

	UPROPERTY()
	TWeakObjectPtr<class UAbilitySystemComponent> SecondObservedAbilitySystemComponent;

	// Store weak reference to the behavior tree component for re-evaluation
	UPROPERTY()
	TWeakObjectPtr<UBehaviorTreeComponent> CachedOwnerComp;

	// Delegate handles for cleanup
	FDelegateHandle AttributeChangeDelegateHandle;
	FDelegateHandle SecondAttributeChangeDelegateHandle;
};
