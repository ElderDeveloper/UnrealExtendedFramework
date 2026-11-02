// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealExtendedGameplay/Systems/Placement/Solvers/EGPlacementPoseSolver.h"

#include "EGPlacementSolvers.generated.h"

class UEGPlacementAnchorComponent;

/**
 * Probes along the intent's normal, classifies the surface, snaps in the grid frame on the two
 * axes across the surface, grounds again along the third, builds the surface frame, applies the
 * spin and the contact offset. Floors, walls and ceilings.
 */
UCLASS(meta = (DisplayName = "Surface"))
class UNREALEXTENDEDGAMEPLAY_API UEGPlacementSolver_Surface : public UEGPlacementPoseSolver
{
	GENERATED_BODY()

public:
	virtual void Solve_Implementation(const UEGPlacementProfile* Profile, const FEGPlacementSpec& Spec, const FEGPlacementIntent& Intent,
		const FEGPlacementContext& Context, FEGPlacementState& State) const override;
};

/** Takes the intent's pose unchanged: slots, scripted placement, editor tools. */
UCLASS(meta = (DisplayName = "Exact pose"))
class UNREALEXTENDEDGAMEPLAY_API UEGPlacementSolver_ExactPose : public UEGPlacementPoseSolver
{
	GENERATED_BODY()

public:
	/** Probe below the contact point to measure the support and normal, without moving the pose. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	bool bMeasureSupport = true;

	/** How far below the contact point the measurement looks. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement", meta = (ClampMin = "0.1", EditCondition = "bMeasureSupport"))
	float MeasureDistance = 5.0f;

	virtual void Solve_Implementation(const UEGPlacementProfile* Profile, const FEGPlacementSpec& Spec, const FEGPlacementIntent& Intent,
		const FEGPlacementContext& Context, FEGPlacementState& State) const override;
};

/**
 * Snaps to the best compatible, available anchor near the intent: the intent's target anchor when
 * it names one, otherwise the nearest one whose snap radius reaches the intent's pivot.
 */
UCLASS(meta = (DisplayName = "Anchor"))
class UNREALEXTENDEDGAMEPLAY_API UEGPlacementSolver_Anchor : public UEGPlacementPoseSolver
{
	GENERATED_BODY()

public:
	/** Anchors mark the contact point rather than the pivot: move the pivot ContactOffset along the anchor's up. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	bool bAnchorsMarkContact = false;

	/** Largest snap radius any anchor uses; bounds the search. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement", meta = (ClampMin = "1.0"))
	float MaxSearchRadius = 200.0f;

	virtual void Solve_Implementation(const UEGPlacementProfile* Profile, const FEGPlacementSpec& Spec, const FEGPlacementIntent& Intent,
		const FEGPlacementContext& Context, FEGPlacementState& State) const override;

protected:
	FTransform MakeAnchorPose(const UEGPlacementAnchorComponent& Anchor, const FEGPlacementSpec& Spec, const FEGPlacementIntent& Intent, float Scale) const;
};

/**
 * For uneven ground such as rock floors. Probes straight down under the footprint's corners and
 * centre, fits the contact plane by least squares and rests the object on it, raised to the
 * highest contact so it never sinks into a bump. Spin and grid are measured level, so a re-solve
 * of its own pose lands on the same answer.
 */
UCLASS(meta = (DisplayName = "Ground conform"))
class UNREALEXTENDEDGAMEPLAY_API UEGPlacementSolver_GroundConform : public UEGPlacementPoseSolver
{
	GENERATED_BODY()

public:
	/** Raise the fitted plane to the highest probe so no contact is above it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	bool bRestOnHighestContact = true;

	/** Also probe the middle of each footprint edge, for long footprints on bumpy ground. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	bool bProbeEdgeMidpoints = true;

	/** How far below the contact point the probes reach. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement", meta = (ClampMin = "1.0"))
	float ProbeDepth = 120.0f;

	virtual void Solve_Implementation(const UEGPlacementProfile* Profile, const FEGPlacementSpec& Spec, const FEGPlacementIntent& Intent,
		const FEGPlacementContext& Context, FEGPlacementState& State) const override;
};
