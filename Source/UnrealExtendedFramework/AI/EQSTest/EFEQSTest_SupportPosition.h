// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "EFEQSTest_SupportPosition.generated.h"

UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFEQSTest_SupportPosition : public UEnvQueryTest
{
	GENERATED_BODY()

public:
	UEFEQSTest_SupportPosition(const FObjectInitializer& ObjectInitializer);

	virtual void RunTest(FEnvQueryInstance& QueryInstance) const override;

	virtual FText GetDescriptionTitle() const override;
	virtual FText GetDescriptionDetails() const override;

protected:
	/** Context for ally units to support */
	UPROPERTY(EditDefaultsOnly, Category = Test)
	TSubclassOf<UEnvQueryContext> AllyContext;

	/** Context for potential threats */
	UPROPERTY(EditDefaultsOnly, Category = Test)
	TSubclassOf<UEnvQueryContext> ThreatContext;

	/** Optimal distance to maintain from allies */
	UPROPERTY(EditDefaultsOnly, Category = Test)
	float OptimalSupportDistance;

	/** Maximum distance to consider from allies */
	UPROPERTY(EditDefaultsOnly, Category = Test)
	float MaxSupportDistance;

	/** Weight for distance scoring (0-1) */
	UPROPERTY(EditDefaultsOnly, Category = Test, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DistanceWeight;

	/** Weight for line of sight scoring (0-1) */
	UPROPERTY(EditDefaultsOnly, Category = Test, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LineOfSightWeight;

	/** Weight for threat avoidance scoring (0-1) */
	UPROPERTY(EditDefaultsOnly, Category = Test, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ThreatAvoidanceWeight;

	/** Height for line of sight checks */
	UPROPERTY(EditDefaultsOnly, Category = Test)
	float SightCheckHeight;

	/** Minimum safe distance from threats */
	UPROPERTY(EditDefaultsOnly, Category = Test)
	float MinThreatDistance;
};