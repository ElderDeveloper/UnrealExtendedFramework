// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "EFEQSTest_JumpScarePosition.generated.h"

UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFEQSTest_JumpScarePosition : public UEnvQueryTest
{
	GENERATED_BODY()

public:
	UEFEQSTest_JumpScarePosition(const FObjectInitializer& ObjectInitializer);

	virtual void RunTest(FEnvQueryInstance& QueryInstance) const override;

	virtual FText GetDescriptionTitle() const override;
	virtual FText GetDescriptionDetails() const override;

protected:
	/** Context for the target (usually the player) */
	UPROPERTY(EditDefaultsOnly, Category = Test, meta = (ToolTip = "The target (usually player) to calculate jump scare positions relative to"))
	TSubclassOf<UEnvQueryContext> TargetContext;

	/** Minimum distance required from target for a jump scare */
	UPROPERTY(EditDefaultsOnly, Category = Test, meta = (ClampMin = "0.0", ToolTip = "Minimum distance from target for an effective jump scare. Too close may not be scary or may be too obvious"))
	float MinDistance;

	/** Maximum distance allowed from target for a jump scare */
	UPROPERTY(EditDefaultsOnly, Category = Test, meta = (ClampMin = "0.0", ToolTip = "Maximum distance from target for the jump scare to be effective. Too far may not create the desired impact"))
	float MaxDistance;

	/** Optimal distance from target for maximum scare effect */
	UPROPERTY(EditDefaultsOnly, Category = Test, meta = (ClampMin = "0.0", ToolTip = "The ideal distance from target for maximum scare effect. Positions at this distance will score highest"))
	float OptimalDistance;

	/** Weight for the distance score component */
	UPROPERTY(EditDefaultsOnly, Category = Test, meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "How much the distance factor influences the final score (0-1)"))
	float DistanceWeight;

	/** Weight for the approach angle score component */
	UPROPERTY(EditDefaultsOnly, Category = Test, meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "How much the approach angle influences the final score (0-1)"))
	float ApproachAngleWeight;

	/** Weight for the visibility score component */
	UPROPERTY(EditDefaultsOnly, Category = Test, meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "How much the visibility factor influences the final score (0-1)"))
	float VisibilityWeight;

	/** Optimal angle for approaching target (in degrees) */
	UPROPERTY(EditDefaultsOnly, Category = Test, meta = (ClampMin = "0.0", ClampMax = "180.0", ToolTip = "The ideal angle of approach relative to target's forward vector (0-180 degrees). 0 means from front, 180 means from behind"))
	float OptimalApproachAngle;

	/** Height to check for visibility */
	UPROPERTY(EditDefaultsOnly, Category = Test, meta = (ClampMin = "0.0", ToolTip = "Height above ground to perform visibility checks"))
	float SightCheckHeight;

	/** Whether to check if position is currently visible to target */
	UPROPERTY(EditDefaultsOnly, Category = Test, meta = (ToolTip = "If true, will check if the position is currently visible to the target. Positions in view might be less effective for scares"))
	bool bCheckTargetVisibility;

	/** Penalty for positions currently visible to target */
	UPROPERTY(EditDefaultsOnly, Category = Test, meta = (EditCondition = "bCheckTargetVisibility", ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Score multiplier penalty for positions currently visible to target (0-1). Lower values make visible positions score worse"))
	float VisibilityPenalty;
};