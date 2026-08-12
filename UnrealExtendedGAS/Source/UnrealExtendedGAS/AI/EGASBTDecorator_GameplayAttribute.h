#pragma once

#include "AI/EGASBTDecorator_Base.h"
#include "AttributeSet.h"
#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"

#include "EGASBTDecorator_GameplayAttribute.generated.h"

class UAbilitySystemComponent;

/** How an attribute is compared against the second operand. */
UENUM(BlueprintType)
enum class EEGASAttributeCompareOp : uint8
{
	Equal          UMETA(DisplayName = "Equal (==)"),
	NotEqual       UMETA(DisplayName = "Not Equal (!=)"),
	Greater        UMETA(DisplayName = "Greater (>)"),
	GreaterOrEqual UMETA(DisplayName = "Greater or Equal (>=)"),
	Less           UMETA(DisplayName = "Less (<)"),
	LessOrEqual    UMETA(DisplayName = "Less or Equal (<=)")
};

/**
 * Compares a gameplay attribute against either a fixed value or a second
 * attribute, on the AI itself or on the actor in TargetActorKey.
 *
 * With bTrackAttributeChanges the decorator observes both operands and asks the
 * tree to re-evaluate the branch when either changes. That only takes effect when
 * the node's Observer Aborts mode is set to something other than None.
 */
UCLASS(meta = (DisplayName = "Compare Gameplay Attribute"))
class UNREALEXTENDEDGAS_API UEGASBTDecorator_GameplayAttribute : public UEGASBTDecorator_Base
{
	GENERATED_BODY()

public:
	UEGASBTDecorator_GameplayAttribute(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;

	/** Attribute read from the target. */
	UPROPERTY(EditAnywhere, Category = "Attribute")
	FGameplayAttribute AttributeToCheck;

	/** How AttributeToCheck is compared against the second operand. */
	UPROPERTY(EditAnywhere, Category = "Attribute")
	EEGASAttributeCompareOp Comparison = EEGASAttributeCompareOp::Greater;

	/** Compare against a second attribute instead of the fixed ComparisonValue. */
	UPROPERTY(EditAnywhere, Category = "Attribute")
	bool bCompareWithAttribute = false;

	/** Fixed value to compare against. Used when bCompareWithAttribute is false. */
	UPROPERTY(EditAnywhere, Category = "Attribute", meta = (EditCondition = "!bCompareWithAttribute"))
	float ComparisonValue = 0.f;

	/** Observe both operands and re-evaluate the branch when either changes. */
	UPROPERTY(EditAnywhere, Category = "Attribute")
	bool bTrackAttributeChanges = true;

	/** Read the second attribute from the AI's own pawn. */
	UPROPERTY(EditAnywhere, Category = "Second Operand", meta = (EditCondition = "bCompareWithAttribute"))
	bool bSecondTargetSelf = true;

	/** Blackboard key holding the actor for the second attribute. */
	UPROPERTY(EditAnywhere, Category = "Second Operand", meta = (EditCondition = "bCompareWithAttribute && !bSecondTargetSelf"))
	FBlackboardKeySelector SecondTargetActorKey;

	/** Attribute used as the right-hand side of the comparison. */
	UPROPERTY(EditAnywhere, Category = "Second Operand", meta = (EditCondition = "bCompareWithAttribute"))
	FGameplayAttribute SecondAttributeToCheck;

protected:
	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;
	virtual FString GetStaticDescription() const override;

	virtual void OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

private:
	void HandleAttributeChanged(const FOnAttributeChangeData& Data);

	bool Compare(float Left, float Right) const;
	FString GetComparisonOperator() const;

	/** Ability system component holding SecondAttributeToCheck. */
	UAbilitySystemComponent* ResolveSecondAbilitySystem(const UBehaviorTreeComponent& OwnerComp) const;

	// Per-agent observer state — safe because the base forces node instancing.
	TWeakObjectPtr<UAbilitySystemComponent> ObservedAbilitySystem;
	TWeakObjectPtr<UAbilitySystemComponent> SecondObservedAbilitySystem;
	TWeakObjectPtr<UBehaviorTreeComponent> CachedOwnerComp;
	FDelegateHandle AttributeChangeHandle;
	FDelegateHandle SecondAttributeChangeHandle;
};
