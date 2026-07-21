#pragma once

#include "Abilities/Tasks/AbilityTask.h"
#include "AttributeSet.h"
#include "CoreMinimal.h"

#include "EGASAbilityTask_Timer.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FEGASAbilityTaskTimerDelegate);

/**
 * Ability timer that can fire once or loop. Interval can be a fixed Time or an ASC
 * attribute (re-read each wait when looping). Call StopTimer() to cancel early.
 */
UCLASS()
class UNREALEXTENDEDGAS_API UEGASAbilityTask_Timer : public UAbilityTask
{
	GENERATED_BODY()

public:
	/** Fired each time the timer elapses (and immediately if bTriggerImmediately). */
	UPROPERTY(BlueprintAssignable)
	FEGASAbilityTaskTimerDelegate OnTimer;

	/** Fired when a one-shot timer completes, or when StopTimer / task teardown ends it. */
	UPROPERTY(BlueprintAssignable)
	FEGASAbilityTaskTimerDelegate OnFinished;

	UFUNCTION(BlueprintCallable, Category = "Unreal Extended GAS|Ability Tasks", meta = (DisplayName = "Wait Timer", HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UEGASAbilityTask_Timer* WaitTimer(UGameplayAbility* OwningAbility, float Time, bool bLooping = false, bool bTriggerImmediately = false);

	UFUNCTION(BlueprintCallable, Category = "Unreal Extended GAS|Ability Tasks", meta = (DisplayName = "Wait Timer From Attribute", HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UEGASAbilityTask_Timer* WaitTimerFromAttribute(UGameplayAbility* OwningAbility, FGameplayAttribute IntervalAttribute, bool bLooping = true, bool bTriggerImmediately = false);

	/** Stops the timer and ends the task (broadcasts OnFinished). */
	UFUNCTION(BlueprintCallable, Category = "Unreal Extended GAS|Ability Tasks")
	void StopTimer();

	virtual void Activate() override;
	virtual void OnDestroy(bool bInOwnerFinished) override;

protected:
	float Time = 0.0f;
	FGameplayAttribute IntervalAttribute;
	bool bUseAttributeInterval = false;
	bool bLooping = false;
	bool bTriggerImmediately = false;
	bool bStopped = false;
	FTimerHandle TimerHandle;

	float GetNextInterval() const;
	void ClearTimer();
	void ScheduleNext();
	void BroadcastFinished();

	UFUNCTION()
	void HandleTimer();
};
