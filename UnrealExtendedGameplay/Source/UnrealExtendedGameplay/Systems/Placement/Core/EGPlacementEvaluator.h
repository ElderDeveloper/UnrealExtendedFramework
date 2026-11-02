// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealExtendedGameplay/Systems/Placement/Core/EGPlacementTypes.h"

class UEGPlacementProfile;
class UEGPlacementSpatialSubsystem;
class UPrimitiveComponent;

/**
 * The evaluator. Reads the world and changes nothing: it never spawns, moves, consumes, writes,
 * loads synchronously or calls a back end. Missing data is reported as Pending or as a reason.
 *
 *     Evaluate     = SolvePose, then ValidatePose
 *     SolvePose    -> State       (the profile's solver)
 *     ValidatePose -> Evaluation  (the profile's rules)
 *
 * A project that gets its pose elsewhere (a slot, a scripted spawn, its own solver) fills a
 * state with MakeStateFromPose and calls ValidatePose to run every rule.
 */
namespace EGPlacement
{
	UNREALEXTENDEDGAMEPLAY_API FEGPlacementEvaluation Evaluate(const UEGPlacementProfile* Profile, const FEGPlacementSpec& Spec, const FEGPlacementIntent& Intent, const FEGPlacementContext& Context);

	UNREALEXTENDEDGAMEPLAY_API FEGPlacementState SolvePose(const UEGPlacementProfile* Profile, const FEGPlacementSpec& Spec, const FEGPlacementIntent& Intent, const FEGPlacementContext& Context);

	UNREALEXTENDEDGAMEPLAY_API FEGPlacementEvaluation ValidatePose(const UEGPlacementProfile* Profile, const FEGPlacementSpec& Spec, const FEGPlacementIntent& Intent, const FEGPlacementState& State, const FEGPlacementContext& Context);

	/** A state for a pose obtained elsewhere, with the derived fields filled in. Support is optional. */
	UNREALEXTENDEDGAMEPLAY_API FEGPlacementState MakeStateFromPose(const UEGPlacementProfile* Profile, const FEGPlacementSpec& Spec, const FTransform& Pose, UPrimitiveComponent* Support, const FEGPlacementContext& Context);

	/**
	 * Fills what follows from the pose and the hit: normal, orientation, slope, support actor and
	 * handle, world boxes, regions, area availability and revisions. Runs after every solver.
	 */
	UNREALEXTENDEDGAMEPLAY_API void CompleteState(const UEGPlacementProfile* Profile, const FEGPlacementSpec& Spec, const FEGPlacementContext& Context, FEGPlacementState& State);

	/** From IEGPlacementSupportInterface, or the component's path inside its level (without the PIE prefix). */
	UNREALEXTENDEDGAMEPLAY_API FEGPlacementHandle GetSupportHandle(const UPrimitiveComponent* Component);

	/** The current revision of a region, an anchor or an interface support. 0 is untracked. */
	UNREALEXTENDEDGAMEPLAY_API int32 GetRevision(const UObject* Source);

	/** Surface tags at the state's contact: component and actor tags, the profile's physical material table, the support interface and include regions. */
	UNREALEXTENDEDGAMEPLAY_API void GatherSurfaceTags(const UEGPlacementProfile* Profile, const FEGPlacementSpec& Spec, const FEGPlacementState& State, FGameplayTagContainer& OutTags);

	UNREALEXTENDEDGAMEPLAY_API UEGPlacementSpatialSubsystem* ResolveSpatial(const FEGPlacementContext& Context);

	/** The stable frame grids and spins are measured in. Support frames drop scale. */
	UNREALEXTENDEDGAMEPLAY_API FTransform GetGridFrame(const FEGPlacementGrid& Grid, const UPrimitiveComponent* Support);

	/** A context for World and Requester, ignoring the requester. */
	UNREALEXTENDEDGAMEPLAY_API FEGPlacementContext MakeContext(UWorld* World, AActor* Requester, EEGPlacementScope Scope);
}

/** Process-wide counters for Extended.Status. Game thread only. */
struct UNREALEXTENDEDGAMEPLAY_API FEGPlacementStats
{
	static int32 ActiveSessions;
	static int64 Evaluations;
	static double EvaluationSeconds;
	static int64 CacheHits;
	static int64 CacheMisses;
	static int64 Commits;
};
