// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EGAreaDamage.h"
#include "EGAreaDamageBox.generated.h"

/**
 * 
 */
UCLASS()
class UNREALEXTENDEDGAMEPLAY_API AEGAreaDamageBox : public AEGAreaDamage
{
	GENERATED_BODY()

public:
	AEGAreaDamageBox();
	
	UPROPERTY(EditDefaultsOnly)
	class UBoxComponent* BoxComponent;


	UFUNCTION(BlueprintCallable)
	void InitializeDamage(float damageAmount , FVector damageExtend , bool isLooping , float loopTime , float lifetime , TSubclassOf<UDamageType> damageType = nullptr);
	
	FVector DamageExtend;
	
	UFUNCTION()
	void ApplyAreaDamage();
};
