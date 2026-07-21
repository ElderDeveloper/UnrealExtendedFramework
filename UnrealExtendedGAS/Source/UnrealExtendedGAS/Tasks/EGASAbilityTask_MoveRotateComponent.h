#pragma once

#include "Abilities/Tasks/AbilityTask.h"
#include "CoreMinimal.h"

#include "EGASAbilityTask_MoveRotateComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FEGASMoveRotateComponentFinishedDelegate);

UCLASS()
class UNREALEXTENDEDGAS_API UEGASAbilityTask_MoveRotateComponent : public UAbilityTask
{
	GENERATED_BODY()

public:
	UEGASAbilityTask_MoveRotateComponent();

	UFUNCTION(BlueprintCallable, Category = "Unreal Extended GAS|Ability Tasks", meta = (DisplayName = "Move Rotate Component", HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UEGASAbilityTask_MoveRotateComponent* MoveRotateComponent(UGameplayAbility* OwningAbility, USceneComponent* Component, FVector TargetRelativeLocation, FRotator TargetRelativeRotation, float Duration = 0.25f);

	virtual void Activate() override;
	virtual void TickTask(float DeltaTime) override;

	UPROPERTY(BlueprintAssignable)
	FEGASMoveRotateComponentFinishedDelegate OnFinished;

private:
	UPROPERTY()
	TObjectPtr<USceneComponent> Component;

	FVector StartRelativeLocation = FVector::ZeroVector;
	FRotator StartRelativeRotation = FRotator::ZeroRotator;
	FVector TargetRelativeLocation = FVector::ZeroVector;
	FRotator TargetRelativeRotation = FRotator::ZeroRotator;
	float Duration = 0.25f;
	float ElapsedTime = 0.0f;
};
