// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UEExtendedAreaDamage.h"
#include "UEExtendedAreaDamageSphere.generated.h"

/**
 * 
 */
class UNiagaraSystem;
class UDamageType;
UCLASS()
class UEEXPANDEDFRAMEWORK_API AUEExtendedAreaDamageSphere : public AUEExtendedAreaDamage
{
	GENERATED_BODY()

	AUEExtendedAreaDamageSphere();

public:

	UFUNCTION(BlueprintCallable)
	void InitializeDamage(float damageAmount , float radius , bool isLooping , float loopTime , float lifetime , TSubclassOf<UDamageType> damageType = nullptr);
	
	float DamageSphereRadius;
	
	UFUNCTION()
	void ApplyAreaDamage();

	
};
