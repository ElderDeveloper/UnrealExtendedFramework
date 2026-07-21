#pragma once

#include "Abilities/Tasks/AbilityTask.h"
#include "CoreMinimal.h"

#include "EGASAbilityTask_FloatCurve.generated.h"

class UCurveFloat;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEGASFloatCurveDelegate, float, Value, float, ElapsedTime);

UCLASS()
class UNREALEXTENDEDGAS_API UEGASAbilityTask_FloatCurve : public UAbilityTask
{
	GENERATED_BODY()

public:
	UEGASAbilityTask_FloatCurve();

	UFUNCTION(BlueprintCallable, Category = "Unreal Extended GAS|Ability Tasks", meta = (DisplayName = "Play Ability Float Curve", HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UEGASAbilityTask_FloatCurve* PlayFloatCurve(UGameplayAbility* OwningAbility, UCurveFloat* Curve, float Duration = 1.0f, bool bUseCurveTimeRange = true);

	virtual void Activate() override;
	virtual void TickTask(float DeltaTime) override;

	UPROPERTY(BlueprintAssignable)
	FEGASFloatCurveDelegate OnUpdate;

	UPROPERTY(BlueprintAssignable)
	FEGASFloatCurveDelegate OnFinished;

private:
	UPROPERTY()
	TObjectPtr<UCurveFloat> Curve;

	float Duration = 1.0f;
	float ElapsedTime = 0.0f;
	float MinCurveTime = 0.0f;
	float MaxCurveTime = 1.0f;
	bool bUseCurveTimeRange = true;
};
