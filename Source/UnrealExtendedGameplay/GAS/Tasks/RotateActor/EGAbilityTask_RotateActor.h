#pragma once
#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "EGAbilityTask_RotateActor.generated.h"

class UCurveFloat;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRotateActorDelegate);

/**
 * 
 */
UCLASS()
class UNREALEXTENDEDGAMEPLAY_API UEGAbilityTask_RotateActor : public UAbilityTask
{
	GENERATED_BODY()

public:
	UEGAbilityTask_RotateActor();

	UPROPERTY(BlueprintAssignable)
	FRotateActorDelegate OnFinished;

	UPROPERTY(BlueprintAssignable)
	FRotateActorDelegate OnInterrupted;

	/**
	 * Rotates an actor from a starting rotation to a target rotation over a set duration, optionally using a curve.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UEGAbilityTask_RotateActor* CreateRotateActorTask(
		UGameplayAbility* OwningAbility,
		FName TaskInstanceName,
		AActor* TargetActor,
		FRotator StartRotation,
		FRotator TargetRotation,
		float Duration,
		UCurveFloat* OptionalInterpolationCurve = nullptr
	);

	virtual void Activate() override;
	virtual void OnDestroy(bool AbilityEnded) override;
	virtual void TickTask(float DeltaTime) override;

protected:
	UPROPERTY()
	TObjectPtr<AActor> TargetActor;

	UPROPERTY()
	TObjectPtr<UCurveFloat> Curve;

	FRotator StartRotation;
	FRotator TargetRotation;
	float Duration;
	float CurrentTime;

private:
	void UpdateRotation(float Alpha);
	void CompleteTask();
};