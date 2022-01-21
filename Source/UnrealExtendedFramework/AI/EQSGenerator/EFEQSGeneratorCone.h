// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/Generators/EnvQueryGenerator_Cone.h"
#include "EFEQSGeneratorCone.generated.h"

/**
 * 
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFEQSGeneratorCone : public UEnvQueryGenerator_Cone
{
	GENERATED_BODY()
	
	virtual void GenerateItems(FEnvQueryInstance& QueryInstance) const override;
	
	UPROPERTY(EditAnywhere,Category="Cone Properties")
	float ExtendedPointsDistance = 50.f;

	UPROPERTY(EditAnywhere,Category="Cone Properties")
	float ExtendedConeDegrees  = 20.f;

	UPROPERTY(EditAnywhere,Category="Cone Properties")
	float ExtendedAngleStep;

	UPROPERTY(EditAnywhere,Category="Cone Properties")
	float ExtendedConeRadius = 150.f;

	
};
