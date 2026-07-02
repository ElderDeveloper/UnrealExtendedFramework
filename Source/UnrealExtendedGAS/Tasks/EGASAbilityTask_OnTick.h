#pragma once

#include "Abilities/Tasks/AbilityTask.h"
#include "CoreMinimal.h"

#include "EGASAbilityTask_OnTick.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEGASAbilityTaskTickDelegate, float, DeltaTime, float, ElapsedTime);

UCLASS()
class UNREALEXTENDEDGAS_API UEGASAbilityTask_OnTick : public UAbilityTask
{
	GENERATED_BODY()

public:
	UEGASAbilityTask_OnTick();

	UFUNCTION(BlueprintCallable, Category = "Unreal Extended GAS|Ability Tasks", meta = (DisplayName = "Wait On Tick", HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UEGASAbilityTask_OnTick* WaitOnTick(UGameplayAbility* OwningAbility, FName TaskInstanceName, float Duration = -1.0f);

	virtual void Activate() override;
	virtual void TickTask(float DeltaTime) override;

	UPROPERTY(BlueprintAssignable)
	FEGASAbilityTaskTickDelegate OnTick;

	UPROPERTY(BlueprintAssignable)
	FEGASAbilityTaskTickDelegate OnFinished;

private:
	float Duration = -1.0f;
	float ElapsedTime = 0.0f;
};
