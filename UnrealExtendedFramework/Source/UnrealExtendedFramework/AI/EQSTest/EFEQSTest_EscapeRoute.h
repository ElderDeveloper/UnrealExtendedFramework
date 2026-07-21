// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "EFEQSTest_EscapeRoute.generated.h"

UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFEQSTest_EscapeRoute : public UEnvQueryTest
{
	GENERATED_BODY()

public:
	UEFEQSTest_EscapeRoute(const FObjectInitializer& ObjectInitializer);

	virtual void RunTest(FEnvQueryInstance& QueryInstance) const override;

	virtual FText GetDescriptionTitle() const override;
	virtual FText GetDescriptionDetails() const override;

protected:
	/** Context to measure distance from (usually the player/threat) */
	UPROPERTY(EditDefaultsOnly, Category = Test)
	TSubclassOf<UEnvQueryContext> DistanceContext;

	/** Minimum desired distance from the context */
	UPROPERTY(EditDefaultsOnly, Category = Test)
	float MinDesiredDistance;

	/** Maximum distance to consider */
	UPROPERTY(EditDefaultsOnly, Category = Test)
	float MaxDistance;

	/** Weight for direct distance scoring (0-1) */
	UPROPERTY(EditDefaultsOnly, Category = Test, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DirectDistanceWeight;

	/** Weight for path options scoring (0-1) */
	UPROPERTY(EditDefaultsOnly, Category = Test, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PathOptionsWeight;

	/** Distance to check for alternative paths */
	UPROPERTY(EditDefaultsOnly, Category = Test)
	float PathCheckRadius;

	/** Number of directions to check for alternative paths */
	UPROPERTY(EditDefaultsOnly, Category = Test)
	int32 NumPathChecks;
};
