#pragma once

#include "Abilities/Tasks/AbilityTask.h"
#include "CoreMinimal.h"

#include "EGASAbilityTask_LookToComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FEGASLookToComponentFinishedDelegate);

UCLASS()
class UNREALEXTENDEDGAS_API UEGASAbilityTask_LookToComponent : public UAbilityTask
{
	GENERATED_BODY()

public:
	UEGASAbilityTask_LookToComponent();

	UFUNCTION(BlueprintCallable, Category = "Unreal Extended GAS|Ability Tasks", meta = (DisplayName = "Look To Component", HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UEGASAbilityTask_LookToComponent* LookToComponent(UGameplayAbility* OwningAbility, USceneComponent* TargetComponent, float Duration = 0.25f, bool bYawOnly = true);

	virtual void Activate() override;
	virtual void TickTask(float DeltaTime) override;

	UPROPERTY(BlueprintAssignable)
	FEGASLookToComponentFinishedDelegate OnFinished;

private:
	UPROPERTY()
	TObjectPtr<USceneComponent> TargetComponent;

	float Duration = 0.25f;
	float ElapsedTime = 0.0f;
	bool bYawOnly = true;
	FRotator StartRotation = FRotator::ZeroRotator;
};
