#pragma once

#include "Abilities/Tasks/AbilityTask.h"
#include "AttributeSet.h"
#include "CoreMinimal.h"

#include "EGASAbilityTask_WaitAttributeChange.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FEGASAttributeChangedDelegate, FGameplayAttribute, Attribute, float, NewValue, float, OldValue);

UCLASS()
class UNREALEXTENDEDGAS_API UEGASAbilityTask_WaitAttributeChange : public UAbilityTask
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Unreal Extended GAS|Ability Tasks", meta = (DisplayName = "Wait Attribute Change", HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UEGASAbilityTask_WaitAttributeChange* WaitAttributeChange(UGameplayAbility* OwningAbility, FGameplayAttribute Attribute, bool bOnlyTriggerOnce = false);

	virtual void Activate() override;
	virtual void OnDestroy(bool bInOwnerFinished) override;

	UPROPERTY(BlueprintAssignable)
	FEGASAttributeChangedDelegate OnChanged;

private:
	void HandleAttributeChanged(const FOnAttributeChangeData& ChangeData);

	FGameplayAttribute Attribute;
	bool bOnlyTriggerOnce = false;
	FDelegateHandle DelegateHandle;
};
