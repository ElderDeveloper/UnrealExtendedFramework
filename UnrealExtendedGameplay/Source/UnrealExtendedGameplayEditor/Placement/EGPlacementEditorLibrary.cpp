// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGPlacementEditorLibrary.h"

#include "Engine/StaticMesh.h"
#include "ScopedTransaction.h"
#include "UnrealExtendedGameplay/Systems/Placement/Profile/EGPlaceableDefinition.h"

#define LOCTEXT_NAMESPACE "EGPlacementEditorLibrary"

bool UEGPlacementEditorLibrary::FitShapeFromStaticMesh(UStaticMesh* Mesh, FTransform Relative, EEGPlacementFitSource Source, float Padding, FEGPlacementShape& OutShape, float& OutContactOffset)
{
	OutShape = FEGPlacementShape();
	OutContactOffset = 0.0f;
	if (!EGPlacementVisuals::FitBoxes(Mesh, Relative, Source, Padding, OutShape.Footprint))
	{
		return false;
	}
	OutContactOffset = static_cast<float>(-OutShape.GetLocalBounds().Min.Z);
	return true;
}

bool UEGPlacementEditorLibrary::FitDefinitionShape(UEGPlaceableDefinition* Definition, EEGPlacementFitSource Source, float Padding)
{
	if (!Definition)
	{
		return false;
	}
	const FScopedTransaction Transaction(LOCTEXT("FitShape", "Fit Placement Shape"));
	Definition->FitShape(Source, Padding);
	return !Definition->Spec.Shape.IsEmpty();
}

bool UEGPlacementEditorLibrary::FitSpecFromVisual(FEGPlacementSpec& Spec, const FEGPlacementVisual& Visual, EEGPlacementFitSource Source, float Padding)
{
	return EGPlacementVisuals::FitSpec(Spec, Visual, Source, Padding);
}

#undef LOCTEXT_NAMESPACE
