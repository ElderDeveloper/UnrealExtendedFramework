// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealExtendedGameplay/Systems/Placement/Core/EGPlacementTypes.h"

class UWorld;

/**
 * Debug drawing for placement, switched with console variables:
 *
 *     Extended.Placement.Debug 1     evaluations: footprint and clearance boxes, pose, normal, support, reasons
 *     Extended.Placement.Debug 2     also regions and anchors near the evaluated pose
 *
 * Works in game and in editor worlds. Compiled out where debug drawing is (Shipping).
 */
namespace EGPlacementDebug
{
	UNREALEXTENDEDGAMEPLAY_API int32 GetLevel();

	UNREALEXTENDEDGAMEPLAY_API void DrawEvaluation(const UWorld* World, const FEGPlacementEvaluation& Evaluation, float Duration = 0.0f);

	/** Regions and anchors within Radius of Center. */
	UNREALEXTENDEDGAMEPLAY_API void DrawSpatial(const UWorld* World, const FVector& Center, float Radius, float Duration = 0.0f);

	UNREALEXTENDEDGAMEPLAY_API void DrawOrientedBox(const UWorld* World, const FEGPlacementOrientedBox& Box, const FColor& Color, float Duration = 0.0f, float Thickness = 1.0f);
}
