// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UEExtendedAreaDamage.h"
#include "UEExtendedAreaDamageBox.generated.h"

UCLASS()
class UEEXPANDEDFRAMEWORK_API AUEExtendedAreaDamageBox : public AUEExtendedAreaDamage
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AUEExtendedAreaDamageBox();

	UPROPERTY(EditDefaultsOnly)
	class UBoxComponent* BoxComponent;


	UFUNCTION(BlueprintCallable)
	void InitializeDamage(float damageAmount , FVector damageExtend , bool isLooping , float loopTime , float lifetime , TSubclassOf<UDamageType> damageType = nullptr);
	
	FVector DamageExtend;
	
	UFUNCTION()
	void ApplyAreaDamage();
};
