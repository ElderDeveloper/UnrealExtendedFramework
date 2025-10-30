// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/Generators/EnvQueryGenerator_ProjectedPoints.h"
#include "EFEQSGeneratorCube.generated.h"

/**
 * Generates points in a 3D cube shape.
 */
UCLASS(meta = (DisplayName = "Points: Cube"))
class UNREALEXTENDEDFRAMEWORK_API UEFEQSGeneratorCube : public UEnvQueryGenerator_ProjectedPoints
{
	GENERATED_BODY()

public:
	UEFEQSGeneratorCube();

	/** Half-dimensions of the cube in which to generate points. */
	UPROPERTY(EditDefaultsOnly, Category = Generator)
	FVector CubeHalfExtents;

	/** The distance between generated points. */
	UPROPERTY(EditDefaultsOnly, Category = Generator, meta = (UIMin = "1.0"))
	float SpaceBetween;

	/** The context to generate points around. */
	UPROPERTY(EditDefaultsOnly, Category = Generator)
	TSubclassOf<UEnvQueryContext> Center;

	virtual void GenerateItems(FEnvQueryInstance& QueryInstance) const override;

	/** Returns the title of the generator on the corresponding node in the EQS Editor window */
	virtual FText GetDescriptionTitle() const override;

	/** Returns the details of the generator on the corresponding node in the EQS Editor window */
	virtual FText GetDescriptionDetails() const override;

};