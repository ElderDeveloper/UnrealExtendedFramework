// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealExtendedGameplay/Systems/Placement/Core/EGPlacementTypes.h"

#include "EGPlacementVisuals.generated.h"

class AActor;
class UStaticMesh;

/** What the shape fitter reads from a mesh. */
UENUM(BlueprintType)
enum class EEGPlacementFitSource : uint8
{
	/** One box around the render bounds. */
	Bounds,
	/** One box per simple collision element; the render bounds when there are none. */
	SimpleCollision,
};

/** Building preview visuals and spec shapes ahead of time, from classes, actors and meshes. */
namespace EGPlacementVisuals
{
	/**
	 * The mesh components of an actor class without spawning it: the class default object's
	 * components and the Blueprint construction script templates of the class and its parents,
	 * with inherited-component overrides. Cached per class. Transforms are relative to the actor.
	 */
	UNREALEXTENDEDGAMEPLAY_API const FEGPlacementVisual& BuildFromClass(UClass* ActorClass);

	/** The visible mesh components of a live actor, relative to it, with their current materials. */
	UNREALEXTENDEDGAMEPLAY_API FEGPlacementVisual BuildFromActor(const AActor* Actor);

	/** Boxes around a mesh, in the space Relative maps the mesh into. Padding grows every box. */
	UNREALEXTENDEDGAMEPLAY_API bool FitBoxes(const UStaticMesh* Mesh, const FTransform& Relative, EEGPlacementFitSource Source, float Padding, TArray<FEGPlacementBox>& OutBoxes);

	/**
	 * Writes footprint boxes fitted to every static mesh of Visual into Spec, clears its clearance
	 * list (so clearance uses the footprint) and sets ContactOffset to the bottom of the boxes. Loads
	 * the meshes synchronously: for authoring, not gameplay.
	 */
	UNREALEXTENDEDGAMEPLAY_API bool FitSpec(FEGPlacementSpec& Spec, const FEGPlacementVisual& Visual, EEGPlacementFitSource Source, float Padding);

	/** Drops the class cache, for example after a Blueprint recompiles. */
	UNREALEXTENDEDGAMEPLAY_API void ClearClassCache();
}
