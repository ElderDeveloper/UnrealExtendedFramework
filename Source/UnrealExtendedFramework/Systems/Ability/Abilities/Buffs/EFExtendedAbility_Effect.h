// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "CoreMinimal.h"
#include "UnrealExtendedFramework/Systems/Ability/Abilities/EFExtendedAbility.h"
#include "EFExtendedAbility_Effect.generated.h"


UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFExtendedAbility_Effect : public UEFExtendedAbility
{
	GENERATED_UCLASS_BODY()
	
	void StartExtendedAbility_Implementation(AActor* Instigator) override;
	void StopExtendedAbility_Implementation(AActor* Instigator) override;

protected:

	UPROPERTY(EditDefaultsOnly , BlueprintReadOnly , Category="Effect")
	float Duration;

	// Time Between Ticks To Apply Effect
	UPROPERTY(EditDefaultsOnly , BlueprintReadOnly , Category="Effect")
	float Period;

	FTimerHandle PeriodHandle;
	FTimerHandle DurationHandle;

	UFUNCTION(BlueprintNativeEvent , Category="Effect")
	void ExecutePeriodEffect(AActor* Instigator);
};
