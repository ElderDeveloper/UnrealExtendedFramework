// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UnrealExtendedGameplay/Systems/Placement/Core/EGPlacementTypes.h"

#include "EGPlacementPoseSolver.generated.h"

class UEGPlacementProfile;

/**
 * UEGPlacementPoseSolver
 * Turns an intent into a pose. Instanced into a profile and shared, so a solver keeps no per-call
 * state and never changes the world.
 *
 * The evaluator has checked the inputs (profile, world, finite pose, positive uniform scale, a
 * non-empty footprint) and set State.Pose to the intent's pose and State.Scale to its scale before
 * Solve runs. A solver sets the pose, and the surface hit or the support and anchor it found; the
 * evaluator derives the rest (orientation, slope, boxes, regions, revisions). A solver that cannot
 * answer sets SolveStatus to Invalid or Pending with a reason, and still leaves the best pose it has
 * so a preview can show where the player aims.
 *
 * Solving is idempotent: solving the pose a solve produced gives the same pose within
 * EGPlacementMath::PoseTolerance. That is what lets an authority solve a client's pose again.
 */
UCLASS(Abstract, EditInlineNew, DefaultToInstanced, CollapseCategories, Blueprintable)
class UNREALEXTENDEDGAMEPLAY_API UEGPlacementPoseSolver : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Extended|Placement")
	void Solve(const UEGPlacementProfile* Profile, const FEGPlacementSpec& Spec, const FEGPlacementIntent& Intent,
		const FEGPlacementContext& Context, UPARAM(ref) FEGPlacementState& State) const;
	virtual void Solve_Implementation(const UEGPlacementProfile* Profile, const FEGPlacementSpec& Spec, const FEGPlacementIntent& Intent,
		const FEGPlacementContext& Context, FEGPlacementState& State) const;

protected:
	/** Marks the state refused with a reason raised by this solver. */
	void Refuse(FEGPlacementState& State, EEGPlacementStatus Status, const FGameplayTag& Tag, FText Text) const;

	/** The intent's estimated contact point: the pivot moved ContactOffset down the pose's up. */
	static FVector GetIntentContact(const FEGPlacementSpec& Spec, const FEGPlacementIntent& Intent, float Scale);

	/** The probe direction (towards the surface is its negative): the intent's surface hint, aimed hit or pose up. */
	static FVector GetIntentNormal(const FEGPlacementIntent& Intent);

	/** Traces the intent's target component only, when it names one; otherwise the profile's query. */
	static bool TraceSurface(const UEGPlacementProfile* Profile, const FEGPlacementIntent& Intent, const FEGPlacementContext& Context,
		const FVector& Start, const FVector& End, FHitResult& OutHit);

	/** Builds the placed rotation for Up from the intent's facing, the requested spin and the spec's rotation policy. */
	static FQuat SolveRotation(const UEGPlacementProfile* Profile, const FEGPlacementSpec& Spec, const FEGPlacementIntent& Intent,
		const FTransform& GridFrame, const FVector& Up);
};
