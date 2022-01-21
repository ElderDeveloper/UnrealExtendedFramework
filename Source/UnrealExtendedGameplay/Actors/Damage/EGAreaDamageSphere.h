// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EGAreaDamage.h"
#include "EGAreaDamageSphere.generated.h"

UCLASS()
class UNREALEXTENDEDGAMEPLAY_API AEGAreaDamageSphere : public AEGAreaDamage
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AEGAreaDamageSphere();

public:

	UFUNCTION(BlueprintCallable)
	void InitializeDamage(float damageAmount , float radius , bool isLooping , float loopTime , float lifetime , TSubclassOf<UDamageType> damageType = nullptr);
	
	float DamageSphereRadius;
	
	UFUNCTION()
	void ApplyAreaDamage();
};
