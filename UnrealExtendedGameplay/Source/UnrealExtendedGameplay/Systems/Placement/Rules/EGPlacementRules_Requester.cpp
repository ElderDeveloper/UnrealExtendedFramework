// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGPlacementRules_Requester.h"

#include "Engine/World.h"
#include "UnrealExtendedGameplay/Systems/Placement/Core/EGPlacementMath.h"
#include "UnrealExtendedGameplay/Systems/Placement/Core/EGPlacementTags.h"
#include "UnrealExtendedGameplay/Systems/Placement/Profile/EGPlacementProfile.h"

#define LOCTEXT_NAMESPACE "EGPlacementRules"

namespace EGPlacementRequesterRulePrivate
{
	FVector ClosestPoint(const FEGPlacementOrientedBox& Box, const FVector& Point)
	{
		const FVector Local = Box.Rotation.UnrotateVector(Point - Box.Center);
		const FVector Clamped(
			FMath::Clamp(Local.X, -Box.Extent.X, Box.Extent.X),
			FMath::Clamp(Local.Y, -Box.Extent.Y, Box.Extent.Y),
			FMath::Clamp(Local.Z, -Box.Extent.Z, Box.Extent.Z));
		return Box.Center + Box.Rotation.RotateVector(Clamped);
	}
}

EEGPlacementRuleResult UEGPlacementRule_Reach::Check_Implementation(const UEGPlacementProfile* Profile, const FEGPlacementSpec& Spec, const FEGPlacementIntent& Intent,
	const FEGPlacementContext& Context, const FEGPlacementState& State, FEGPlacementRuleOutput& Output) const
{
	if (!Context.Requester && !Context.bHasViewLocation)
	{
		return EEGPlacementRuleResult::Pass;
	}
	const FVector View = Context.GetViewLocation();
	double Nearest = TNumericLimits<double>::Max();
	for (const FEGPlacementOrientedBox& Box : State.FootprintBoxes)
	{
		Nearest = FMath::Min(Nearest, FVector::Dist(View, EGPlacementRequesterRulePrivate::ClosestPoint(Box, View)));
	}
	if (State.FootprintBoxes.Num() == 0)
	{
		Nearest = FVector::Dist(View, State.Pose.GetLocation());
	}
	if (Nearest > MaxDistance)
	{
		return Fail(Output, TAG_EGPlacement_Reason_OutOfReach, LOCTEXT("TooFar", "Too far away."));
	}
	if (Nearest < MinDistance)
	{
		return Fail(Output, TAG_EGPlacement_Reason_OutOfReach, LOCTEXT("TooClose", "Too close."));
	}
	return EEGPlacementRuleResult::Pass;
}

EEGPlacementRuleResult UEGPlacementRule_LineOfSight::Check_Implementation(const UEGPlacementProfile* Profile, const FEGPlacementSpec& Spec, const FEGPlacementIntent& Intent,
	const FEGPlacementContext& Context, const FEGPlacementState& State, FEGPlacementRuleOutput& Output) const
{
	if (!Profile || !Context.World || (!Context.Requester && !Context.bHasViewLocation))
	{
		return EEGPlacementRuleResult::Pass;
	}
	const FVector View = Context.GetViewLocation();
	const FCollisionQueryParams Params = Profile->MakeQueryParams(Context, TEXT("EGPlacementSight"));
	TArray<FVector> Targets;
	for (const FEGPlacementOrientedBox& Box : State.FootprintBoxes)
	{
		Targets.Add(Box.Center);
	}
	if (Targets.Num() == 0)
	{
		Targets.Add(State.Pose.GetLocation());
	}
	for (const FVector& Target : Targets)
	{
		FHitResult Hit;
		const bool bHit = bUseProfileQuery
			? Profile->LineTrace(Context, View, Target, Hit, TEXT("EGPlacementSight"))
			: Context.World->LineTraceSingleByChannel(Hit, View, Target, Channel, Params);
		if (!bHit)
		{
			return EEGPlacementRuleResult::Pass;
		}
		for (const FEGPlacementOrientedBox& Box : State.FootprintBoxes)
		{
			if (EGPlacementMath::BoxContainsPoint(Box, Hit.ImpactPoint, Tolerance))
			{
				return EEGPlacementRuleResult::Pass;
			}
		}
	}
	return Fail(Output, TAG_EGPlacement_Reason_NoLineOfSight, LOCTEXT("Hidden", "The spot cannot be seen from here."));
}

#undef LOCTEXT_NAMESPACE
