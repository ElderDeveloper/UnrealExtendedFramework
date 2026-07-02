// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "EFEQSTest_CoverQuality.generated.h"

UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFEQSTest_CoverQuality : public UEnvQueryTest
{
	GENERATED_BODY()

public:
	UEFEQSTest_CoverQuality(const FObjectInitializer& ObjectInitializer);

	virtual void RunTest(FEnvQueryInstance& QueryInstance) const override;

	virtual FText GetDescriptionTitle() const override;
	virtual FText GetDescriptionDetails() const override;

protected:
	/** Context for the threat/target to check cover against */
	UPROPERTY(EditDefaultsOnly, Category = Test)
	TSubclassOf<UEnvQueryContext> ThreatContext;

	/** Minimum height required for cover */
	UPROPERTY(EditDefaultsOnly, Category = Test)
	float MinCoverHeight;

	/** Maximum distance to consider for cover */
	UPROPERTY(EditDefaultsOnly, Category = Test)
	float MaxDistance;

	/** Weight for line of sight scoring (0-1) */
	UPROPERTY(EditDefaultsOnly, Category = Test, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LineOfSightWeight;

	/** Weight for cover height scoring (0-1) */
	UPROPERTY(EditDefaultsOnly, Category = Test, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HeightWeight;

	/** Weight for distance scoring (0-1) */
	UPROPERTY(EditDefaultsOnly, Category = Test, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DistanceWeight;

	/** Height to check for line of sight (e.g., character eye height) */
	UPROPERTY(EditDefaultsOnly, Category = Test)
	float SightCheckHeight;

	/** Number of height points to check for cover quality */
	UPROPERTY(EditDefaultsOnly, Category = Test)
	int32 NumHeightChecks;
};