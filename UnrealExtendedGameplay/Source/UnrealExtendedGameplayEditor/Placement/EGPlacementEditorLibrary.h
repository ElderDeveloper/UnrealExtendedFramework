// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UnrealExtendedGameplay/Systems/Placement/Core/EGPlacementTypes.h"
#include "UnrealExtendedGameplay/Systems/Placement/Preview/EGPlacementVisuals.h"

#include "EGPlacementEditorLibrary.generated.h"

class UEGPlaceableDefinition;
class UStaticMesh;

/**
 * Editor tools for placement specs: fit footprint and clearance boxes from meshes ahead of time,
 * so shapes come from data and never from a preview's bounds at runtime. For editor utility
 * widgets and Python.
 */
UCLASS(meta = (ScriptName = "EGPlacementEditorLibrary"))
class UNREALEXTENDEDGAMEPLAYEDITOR_API UEGPlacementEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Boxes around Mesh (one per simple collision element, or one around its bounds) in the space
	 * Relative maps the mesh into, and the contact offset that puts their bottom at the contact point.
	 */
	UFUNCTION(BlueprintCallable, Category = "Extended|Placement|Editor")
	static bool FitShapeFromStaticMesh(UStaticMesh* Mesh, FTransform Relative, EEGPlacementFitSource Source, float Padding, FEGPlacementShape& OutShape, float& OutContactOffset);

	/** Fits a definition's footprint to its visual (or actor class) and saves it in the asset, with undo. */
	UFUNCTION(BlueprintCallable, Category = "Extended|Placement|Editor")
	static bool FitDefinitionShape(UEGPlaceableDefinition* Definition, EEGPlacementFitSource Source, float Padding = 0.0f);

	/** Writes fitted boxes into a spec: replaces the footprint, clears clearance, sets the contact offset. */
	UFUNCTION(BlueprintCallable, Category = "Extended|Placement|Editor")
	static bool FitSpecFromVisual(UPARAM(ref) FEGPlacementSpec& Spec, const FEGPlacementVisual& Visual, EEGPlacementFitSource Source, float Padding = 0.0f);
};
