// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "EFEQSTest_AmbushPoint.generated.h"

UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFEQSTest_AmbushPoint : public UEnvQueryTest
{
	GENERATED_BODY()

public:
	UEFEQSTest_AmbushPoint(const FObjectInitializer& ObjectInitializer);

	virtual void RunTest(FEnvQueryInstance& QueryInstance) const override;

	virtual FText GetDescriptionTitle() const override;
	virtual FText GetDescriptionDetails() const override;

protected:
	/** Context for target path (usually the expected path of the target) */
	UPROPERTY(EditDefaultsOnly, Category = Test)
	TSubclassOf<UEnvQueryContext> TargetPathContext;

	/** Optimal distance from target path */
	UPROPERTY(EditDefaultsOnly, Category = Test)
	float OptimalDistance;

	/** Maximum distance to consider */
	UPROPERTY(EditDefaultsOnly, Category = Test)
	float MaxDistance;

	/** Height to check for line of sight */
	UPROPERTY(EditDefaultsOnly, Category = Test)
	float SightCheckHeight;

	/** Weight for distance scoring (0-1) */
	UPROPERTY(EditDefaultsOnly, Category = Test, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DistanceWeight;

	/** Weight for line of sight scoring (0-1) */
	UPROPERTY(EditDefaultsOnly, Category = Test, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LineOfSightWeight;

	/** Weight for cover quality scoring (0-1) */
	UPROPERTY(EditDefaultsOnly, Category = Test, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CoverWeight;

	/** Weight for escape options scoring (0-1) */
	UPROPERTY(EditDefaultsOnly, Category = Test, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EscapeWeight;

	/** Radius to check for cover */
	UPROPERTY(EditDefaultsOnly, Category = Test)
	float CoverCheckRadius;

	/** Number of directions to check for cover */
	UPROPERTY(EditDefaultsOnly, Category = Test)
	int32 NumCoverChecks;

	/** Radius to check for escape routes */
	UPROPERTY(EditDefaultsOnly, Category = Test)
	float EscapeCheckRadius;

	/** Number of directions to check for escape routes */
	UPROPERTY(EditDefaultsOnly, Category = Test)
	int32 NumEscapeChecks;
};