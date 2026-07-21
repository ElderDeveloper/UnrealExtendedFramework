// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "EFEQSTest_FlankingPosition.generated.h"

/**
 * EFEQSTest_FlankingPosition evaluates tactical positions around a target for flanking maneuvers.
 * 
 * This test scores potential positions based on multiple factors:
 * - Distance from the target (with configurable ideal range)
 * - Angle relative to target's forward vector (to find side/rear positions)
 * - Line of sight checks to ensure viable attack positions
 * 
 * Ideal flanking positions will typically be:
 * - At a tactically advantageous distance from the target
 * - To the sides or rear of the target
 * - Have clear line of sight to the target
 * 
 * Usage:
 * 1. Add this test to your Environment Query
 * 2. Set the target context (usually the enemy to flank)
 * 3. Configure distance and angle preferences
 * 4. Optionally enable line of sight checking
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFEQSTest_FlankingPosition : public UEnvQueryTest
{
    GENERATED_BODY()

public:
    UEFEQSTest_FlankingPosition(const FObjectInitializer& ObjectInitializer);

    virtual void RunTest(FEnvQueryInstance& QueryInstance) const override;
    virtual FText GetDescriptionTitle() const override;
    virtual FText GetDescriptionDetails() const override;

protected:
    /** Context for the target to flank */
    UPROPERTY(EditDefaultsOnly, Category = Test)
    TSubclassOf<UEnvQueryContext> TargetContext;

    /** Minimum effective distance from target */
    UPROPERTY(EditDefaultsOnly, Category = Test)
    float MinDistance;

    /** Maximum effective distance from target */
    UPROPERTY(EditDefaultsOnly, Category = Test)
    float MaxDistance;

    /** Optimal distance from target (highest score) */
    UPROPERTY(EditDefaultsOnly, Category = Test)
    float OptimalDistance;

    /** Weight for distance scoring (0-1) */
    UPROPERTY(EditDefaultsOnly, Category = Test, meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float DistanceWeight;

    /** Weight for angle scoring (0-1) */
    UPROPERTY(EditDefaultsOnly, Category = Test, meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float AngleWeight;

    /** Optimal flanking angle in degrees (90 for side, 180 for rear) */
    UPROPERTY(EditDefaultsOnly, Category = Test)
    float OptimalFlankingAngle;

    /** Whether to perform line of sight checks */
    UPROPERTY(EditDefaultsOnly, Category = Test)
    bool bCheckLineOfSight;

    /** Weight for line of sight scoring (0-1) */
    UPROPERTY(EditDefaultsOnly, Category = Test, meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bCheckLineOfSight"))
    float LineOfSightWeight;
};
