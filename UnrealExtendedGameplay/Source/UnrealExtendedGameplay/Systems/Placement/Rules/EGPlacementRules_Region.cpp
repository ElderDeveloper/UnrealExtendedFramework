// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGPlacementRules_Region.h"

#include "UnrealExtendedGameplay/Systems/Placement/Core/EGPlacementMath.h"
#include "UnrealExtendedGameplay/Systems/Placement/Core/EGPlacementTags.h"
#include "UnrealExtendedGameplay/Systems/Placement/Spatial/EGPlacementRegionComponent.h"

#define LOCTEXT_NAMESPACE "EGPlacementRules"

namespace EGPlacementRegionRulePrivate
{
	constexpr double ParallelDot = 1.0 - 1.e-4;

	void CollectRegions(const FEGPlacementState& State, EEGPlacementRegionMode Mode, const FGameplayTagQuery& Filter, TArray<FEGPlacementOrientedBox>& OutBoxes)
	{
		for (const TWeakObjectPtr<UEGPlacementRegionComponent>& Weak : State.Regions)
		{
			const UEGPlacementRegionComponent* Region = Weak.Get();
			if (Region && Region->Mode == Mode && (Filter.IsEmpty() || Filter.Matches(Region->RegionTags)))
			{
				OutBoxes.Add(Region->GetWorldBox());
			}
		}
	}

	/** Index of the region box axis parallel to Direction, or INDEX_NONE. */
	int32 ParallelAxis(const FEGPlacementOrientedBox& Box, const FVector& Direction)
	{
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			if (FMath::Abs(FVector::DotProduct(Box.GetAxis(Axis), Direction)) >= ParallelDot)
			{
				return Axis;
			}
		}
		return INDEX_NONE;
	}

	enum class ECoverage : uint8 { Inside, Outside, NotPlanar };

	/**
	 * Exact test against a union of regions that are prisms along one of the box's own axes and
	 * span the box's whole height along it. NotPlanar when the regions touching the box do not.
	 */
	ECoverage PlanarCoverage(const FEGPlacementOrientedBox& Box, const TArray<FEGPlacementOrientedBox>& Regions, const FVector& Up)
	{
		const int32 NormalAxis = EGPlacementMath::DominantAxis(Box.Rotation, Up);
		const FVector Normal = Box.GetAxis(NormalAxis);
		const double BoxLow = FVector::DotProduct(Box.Center, Normal) - Box.Extent[NormalAxis];
		const double BoxHigh = FVector::DotProduct(Box.Center, Normal) + Box.Extent[NormalAxis];

		FVector U;
		FVector V;
		EGPlacementMath::MakePlaneBasis(Normal, U, V);
		EGPlacementMath::FPolygon2D Subject;
		EGPlacementMath::ProjectBoxToPlane(Box, Box.Center, U, V, Subject);

		TArray<EGPlacementMath::FPolygon2D> Covers;
		for (const FEGPlacementOrientedBox& Region : Regions)
		{
			if (!EGPlacementMath::BoxesIntersect(Box, Region))
			{
				continue;
			}
			const int32 RegionAxis = ParallelAxis(Region, Normal);
			if (RegionAxis == INDEX_NONE)
			{
				return ECoverage::NotPlanar;
			}
			const double Center = FVector::DotProduct(Region.Center, Normal);
			const double Low = Center - Region.Extent[RegionAxis];
			const double High = Center + Region.Extent[RegionAxis];
			if (Low > BoxLow + EGPlacementMath::PoseTolerance || High < BoxHigh - EGPlacementMath::PoseTolerance)
			{
				// Covers only part of the box's height: points of the box escape above or below it.
				return ECoverage::NotPlanar;
			}
			EGPlacementMath::ProjectBoxToPlane(Region, Box.Center, U, V, Covers.AddDefaulted_GetRef());
		}
		return EGPlacementMath::IsCoveredByUnion(Subject, Covers) ? ECoverage::Inside : ECoverage::Outside;
	}

	bool SampledCoverage(const FEGPlacementOrientedBox& Box, const TArray<FEGPlacementOrientedBox>& Regions, double Spacing, int32 MaxSamples)
	{
		int32 Counts[3];
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			Counts[Axis] = FMath::Max(2, FMath::CeilToInt32(2.0 * Box.Extent[Axis] / Spacing) + 1);
		}
		// Widen the spacing evenly until the sample count fits the cap.
		while (int64(Counts[0]) * Counts[1] * Counts[2] > MaxSamples)
		{
			int32 Largest = 0;
			for (int32 Axis = 1; Axis < 3; ++Axis)
			{
				Largest = Counts[Axis] > Counts[Largest] ? Axis : Largest;
			}
			if (Counts[Largest] <= 2)
			{
				break;
			}
			Counts[Largest] = FMath::Max(2, Counts[Largest] * 3 / 4);
		}
		for (int32 X = 0; X < Counts[0]; ++X)
		{
			for (int32 Y = 0; Y < Counts[1]; ++Y)
			{
				for (int32 Z = 0; Z < Counts[2]; ++Z)
				{
					const FVector Local(
						FMath::Lerp(-Box.Extent.X, Box.Extent.X, X / double(Counts[0] - 1)),
						FMath::Lerp(-Box.Extent.Y, Box.Extent.Y, Y / double(Counts[1] - 1)),
						FMath::Lerp(-Box.Extent.Z, Box.Extent.Z, Z / double(Counts[2] - 1)));
					const FVector Point = Box.Center + Box.Rotation.RotateVector(Local);
					if (!Regions.ContainsByPredicate([&Point](const FEGPlacementOrientedBox& Region) { return EGPlacementMath::BoxContainsPoint(Region, Point); }))
					{
						return false;
					}
				}
			}
		}
		return true;
	}
}

EEGPlacementRuleResult UEGPlacementRule_RegionContainment::Check_Implementation(const UEGPlacementProfile* Profile, const FEGPlacementSpec& Spec, const FEGPlacementIntent& Intent,
	const FEGPlacementContext& Context, const FEGPlacementState& State, FEGPlacementRuleOutput& Output) const
{
	using namespace EGPlacementRegionRulePrivate;

	TArray<FEGPlacementOrientedBox> Regions;
	CollectRegions(State, EEGPlacementRegionMode::Include, RegionFilter, Regions);

	bool bInside = Regions.Num() > 0;
	const TArray<FEGPlacementOrientedBox>& Boxes = bUseClearance ? State.ClearanceBoxes : State.FootprintBoxes;
	for (int32 Index = 0; bInside && Index < Boxes.Num(); ++Index)
	{
		const FEGPlacementOrientedBox& Box = Boxes[Index];
		if (Regions.ContainsByPredicate([&Box](const FEGPlacementOrientedBox& Region) { return EGPlacementMath::BoxContainsBox(Region, Box); }))
		{
			continue;
		}
		switch (PlanarCoverage(Box, Regions, State.GetUp()))
		{
		case ECoverage::Inside:
			break;
		case ECoverage::Outside:
			bInside = false;
			break;
		case ECoverage::NotPlanar:
			Output.bApproximate = true;
			bInside = SampledCoverage(Box, Regions, SampleSpacing, MaxSamples);
			break;
		}
	}
	if (bInside)
	{
		return EEGPlacementRuleResult::Pass;
	}
	if (!State.bAreaAvailable)
	{
		return Wait(Output, TAG_EGPlacement_Reason_Pending_Region, LOCTEXT("ContainmentPending", "The area's regions are still loading."));
	}
	return Fail(Output, TAG_EGPlacement_Reason_OutsideRegion, LOCTEXT("Outside", "The object must stay inside the placement area."));
}

EEGPlacementRuleResult UEGPlacementRule_RegionExclusion::Check_Implementation(const UEGPlacementProfile* Profile, const FEGPlacementSpec& Spec, const FEGPlacementIntent& Intent,
	const FEGPlacementContext& Context, const FEGPlacementState& State, FEGPlacementRuleOutput& Output) const
{
	using namespace EGPlacementRegionRulePrivate;

	TArray<FEGPlacementOrientedBox> Regions;
	CollectRegions(State, EEGPlacementRegionMode::Exclude, RegionFilter, Regions);
	const TArray<FEGPlacementOrientedBox>& Boxes = bUseClearance ? State.ClearanceBoxes : State.FootprintBoxes;
	for (const FEGPlacementOrientedBox& Box : Boxes)
	{
		for (const FEGPlacementOrientedBox& Region : Regions)
		{
			if (EGPlacementMath::BoxesIntersect(Box, Region))
			{
				return Fail(Output, TAG_EGPlacement_Reason_InExclusion, LOCTEXT("Excluded", "Nothing can be placed here."));
			}
		}
	}
	if (!State.bAreaAvailable)
	{
		return Wait(Output, TAG_EGPlacement_Reason_Pending_Region, LOCTEXT("ExclusionPending", "The area's regions are still loading."));
	}
	return EEGPlacementRuleResult::Pass;
}

#undef LOCTEXT_NAMESPACE
