// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/Generators/EnvQueryGenerator_Cone.h"
#include "UObject/Object.h"
#include "UEExtendedEQSConeGeneratorDirection.generated.h"

/**
 * 
 */
UCLASS()
class UEEXPANDEDFRAMEWORK_API UUEExtendedEQSConeGeneratorDirection : public UEnvQueryGenerator_Cone
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

	/** The actor (or actors) that will generate a cone in their facing direction */
	UPROPERTY(EditAnywhere,Category="Cone Properties")
	TSubclassOf<UEnvQueryContext> CenterDirectionActor;

	UPROPERTY(EditAnywhere,Category="Cone Properties")
	bool BaseLocationIsContext = false;


	/** Returns the title of the generator on the corresponding node in the EQS Editor window */
	virtual FText GetDescriptionTitle() const override;

	/** Returns the details of the generator on the corresponding node in the EQS Editor window */
	virtual FText GetDescriptionDetails() const override;
};
