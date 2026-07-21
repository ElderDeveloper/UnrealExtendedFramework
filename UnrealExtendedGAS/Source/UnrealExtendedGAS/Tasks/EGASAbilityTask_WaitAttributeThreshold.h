#pragma once

#include "Abilities/Tasks/AbilityTask.h"
#include "AttributeSet.h"
#include "CoreMinimal.h"

#include "EGASAbilityTask_WaitAttributeThreshold.generated.h"

UENUM(BlueprintType)
enum class EEGASAttributeThresholdComparison : uint8
{
	LessThan,
	LessThanOrEqual,
	GreaterThan,
	GreaterThanOrEqual,
	Equal,
	NotEqual
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FEGASAttributeThresholdDelegate, FGameplayAttribute, Attribute, float, NewValue, float, Threshold);

UCLASS()
class UNREALEXTENDEDGAS_API UEGASAbilityTask_WaitAttributeThreshold : public UAbilityTask
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Unreal Extended GAS|Ability Tasks", meta = (DisplayName = "Wait Attribute Threshold", HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UEGASAbilityTask_WaitAttributeThreshold* WaitAttributeThreshold(UGameplayAbility* OwningAbility, FGameplayAttribute Attribute, float Threshold, EEGASAttributeThresholdComparison Comparison, bool bOnlyTriggerOnce = true, bool bTriggerIfAlreadySatisfied = true);

	virtual void Activate() override;
	virtual void OnDestroy(bool bInOwnerFinished) override;

	UPROPERTY(BlueprintAssignable)
	FEGASAttributeThresholdDelegate OnThresholdMet;

private:
	void HandleAttributeChanged(const FOnAttributeChangeData& ChangeData);
	bool IsThresholdMet(float Value) const;

	FGameplayAttribute Attribute;
	float Threshold = 0.0f;
	EEGASAttributeThresholdComparison Comparison = EEGASAttributeThresholdComparison::GreaterThanOrEqual;
	bool bOnlyTriggerOnce = true;
	bool bTriggerIfAlreadySatisfied = true;
	FDelegateHandle DelegateHandle;
};
