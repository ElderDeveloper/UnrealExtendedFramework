#pragma once

#include "Abilities/Tasks/AbilityTask.h"
#include "CoreMinimal.h"

#include "EGASAbilityTask_RotateActor.generated.h"

class UCurveFloat;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FEGASRotateActorDelegate);

/** Rotates an actor over time in world space, with optional curve-driven interpolation. */
UCLASS()
class UNREALEXTENDEDGAS_API UEGASAbilityTask_RotateActor : public UAbilityTask
{
	GENERATED_BODY()

public:
	UEGASAbilityTask_RotateActor();

	UFUNCTION(BlueprintCallable, Category = "Unreal Extended GAS|Ability Tasks", meta = (DisplayName = "Rotate Actor", HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UEGASAbilityTask_RotateActor* CreateRotateActorTask(
		UGameplayAbility* OwningAbility,
		FName TaskInstanceName,
		AActor* TargetActor,
		FRotator StartRotation,
		FRotator TargetRotation,
		float Duration,
		UCurveFloat* OptionalInterpolationCurve = nullptr);

	virtual void Activate() override;
	virtual void TickTask(float DeltaTime) override;

	UPROPERTY(BlueprintAssignable)
	FEGASRotateActorDelegate OnFinished;

	UPROPERTY(BlueprintAssignable)
	FEGASRotateActorDelegate OnInterrupted;

private:
	void UpdateRotation(float Alpha) const;
	void CompleteTask();

	UPROPERTY()
	TObjectPtr<AActor> TargetActor;

	UPROPERTY()
	TObjectPtr<UCurveFloat> Curve;

	FRotator StartRotation = FRotator::ZeroRotator;
	FRotator TargetRotation = FRotator::ZeroRotator;
	float Duration = 1.0f;
	float ElapsedTime = 0.0f;
};
