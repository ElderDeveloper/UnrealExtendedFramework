// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "EFEQSTest_GroupFormation.generated.h"

UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFEQSTest_GroupFormation : public UEnvQueryTest
{
	GENERATED_BODY()

public:
	UEFEQSTest_GroupFormation(const FObjectInitializer& ObjectInitializer);

	virtual void RunTest(FEnvQueryInstance& QueryInstance) const override;

	virtual FText GetDescriptionTitle() const override;
	virtual FText GetDescriptionDetails() const override;

protected:
	/** Context for other group members */
	UPROPERTY(EditDefaultsOnly, Category = Test)
	TSubclassOf<UEnvQueryContext> GroupMembersContext;

	/** Minimum desired distance between group members */
	UPROPERTY(EditDefaultsOnly, Category = Test)
	float MinGroupDistance;

	/** Maximum allowed distance between group members */
	UPROPERTY(EditDefaultsOnly, Category = Test)
	float MaxGroupDistance;

	/** Weight for distance-based scoring (0-1) */
	UPROPERTY(EditDefaultsOnly, Category = Test, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DistanceWeight;

	/** Weight for formation pattern scoring (0-1) */
	UPROPERTY(EditDefaultsOnly, Category = Test, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FormationWeight;

	/** Weight for line of sight scoring (0-1) */
	UPROPERTY(EditDefaultsOnly, Category = Test, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LineOfSightWeight;

	/** Whether to check line of sight between group members */
	UPROPERTY(EditDefaultsOnly, Category = Test)
	bool bCheckLineOfSight;

	/** Height offset for line of sight checks */
	UPROPERTY(EditDefaultsOnly, Category = Test)
	float SightCheckHeight;
};