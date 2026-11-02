// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGPlaceableDefinition.h"

#include "GameFramework/Actor.h"
#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "EGPlaceableDefinition"

FEGPlacementHandle UEGPlaceableDefinition::GetSourceHandle() const
{
	return Spec.Source.IsValid() ? Spec.Source : FEGPlacementHandle(TEXT("EGPlaceable"), GetName());
}

FPrimaryAssetId UEGPlaceableDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("EGPlaceable"), GetFName());
}

#if WITH_EDITOR
EDataValidationResult UEGPlaceableDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (Spec.Shape.IsEmpty())
	{
		Context.AddError(LOCTEXT("NoShape", "The spec has no footprint boxes; every evaluation refuses it. Use Fit Shape From Collision or author the boxes."));
		Result = EDataValidationResult::Invalid;
	}
	for (const FEGPlacementBox& Box : Spec.Shape.Footprint)
	{
		if (Box.Extent.GetMin() <= 0.0)
		{
			Context.AddWarning(LOCTEXT("FlatBox", "A footprint box has a zero extent; it cannot rest on anything."));
		}
	}
	if (Spec.AllowedOrientations == 0)
	{
		Context.AddError(LOCTEXT("NoOrientation", "The spec allows no surface orientation."));
		Result = EDataValidationResult::Invalid;
	}
	if (Visual.IsEmpty() && ActorClass.IsNull())
	{
		Context.AddWarning(LOCTEXT("NoVisual", "Neither a visual nor an actor class: previews show nothing."));
	}
	return Result;
}

void UEGPlaceableDefinition::FitShapeFromCollision()
{
	FitShape(EEGPlacementFitSource::SimpleCollision);
}

void UEGPlaceableDefinition::FitShapeFromBounds()
{
	FitShape(EEGPlacementFitSource::Bounds);
}

void UEGPlaceableDefinition::FitShape(EEGPlacementFitSource Source, float Padding)
{
	FEGPlacementVisual FitVisual = Visual;
	if (FitVisual.IsEmpty())
	{
		if (UClass* Class = ActorClass.LoadSynchronous())
		{
			FitVisual = EGPlacementVisuals::BuildFromClass(Class);
		}
	}
	FEGPlacementSpec Fitted = Spec;
	if (EGPlacementVisuals::FitSpec(Fitted, FitVisual, Source, Padding))
	{
		Modify();
		Spec = Fitted;
		MarkPackageDirty();
	}
}
#endif

#undef LOCTEXT_NAMESPACE
