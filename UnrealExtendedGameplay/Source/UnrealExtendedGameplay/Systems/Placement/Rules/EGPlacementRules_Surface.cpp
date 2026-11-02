// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGPlacementRules_Surface.h"

#include "UnrealExtendedGameplay/Systems/Placement/Core/EGPlacementEvaluator.h"
#include "UnrealExtendedGameplay/Systems/Placement/Core/EGPlacementTags.h"
#include "UnrealExtendedGameplay/Systems/Placement/Profile/EGPlacementProfile.h"

#define LOCTEXT_NAMESPACE "EGPlacementRules"

EEGPlacementRuleResult UEGPlacementRule_SurfaceOrientation::Check_Implementation(const UEGPlacementProfile* Profile, const FEGPlacementSpec& Spec, const FEGPlacementIntent& Intent,
	const FEGPlacementContext& Context, const FEGPlacementState& State, FEGPlacementRuleOutput& Output) const
{
	if (bPassAnchored && State.Anchor.IsValid())
	{
		return EEGPlacementRuleResult::Pass;
	}
	if (Spec.AllowsOrientation(State.Orientation))
	{
		return EEGPlacementRuleResult::Pass;
	}
	return Fail(Output, TAG_EGPlacement_Reason_SurfaceOrientation, LOCTEXT("Orientation", "The object does not go on this kind of surface."));
}

EEGPlacementRuleResult UEGPlacementRule_Slope::Check_Implementation(const UEGPlacementProfile* Profile, const FEGPlacementSpec& Spec, const FEGPlacementIntent& Intent,
	const FEGPlacementContext& Context, const FEGPlacementState& State, FEGPlacementRuleOutput& Output) const
{
	if (bPassAnchored && State.Anchor.IsValid())
	{
		return EEGPlacementRuleResult::Pass;
	}
	const float MaxSlope = Profile ? Profile->GetMaxSlope(Spec) : 90.0f;
	if (State.SlopeDegrees <= MaxSlope + 0.01f)
	{
		return EEGPlacementRuleResult::Pass;
	}
	return Fail(Output, TAG_EGPlacement_Reason_TooSteep,
		FText::Format(LOCTEXT("Steep", "The surface is {0} degrees steep; the limit is {1}."), FText::AsNumber(FMath::RoundToInt(State.SlopeDegrees)), FText::AsNumber(FMath::RoundToInt(MaxSlope))));
}

EEGPlacementRuleResult UEGPlacementRule_SurfaceTags::Check_Implementation(const UEGPlacementProfile* Profile, const FEGPlacementSpec& Spec, const FEGPlacementIntent& Intent,
	const FEGPlacementContext& Context, const FEGPlacementState& State, FEGPlacementRuleOutput& Output) const
{
	FGameplayTagContainer SurfaceTags;
	EGPlacement::GatherSurfaceTags(Profile, Spec, State, SurfaceTags);

	FText Problem;
	if (SurfaceTags.HasAny(RefusedSurfaceTags))
	{
		// A refused tag is present: missing regions cannot change that.
		return Fail(Output, TAG_EGPlacement_Reason_SurfaceTag, LOCTEXT("RefusedTag", "The surface refuses placement."));
	}
	if (!Required.IsEmpty() && !Required.Matches(SurfaceTags))
	{
		Problem = LOCTEXT("Required", "The surface lacks a required tag.");
	}
	else
	{
		for (const TPair<FGameplayTag, FGameplayTagQuery>& Pair : SpecRequirements)
		{
			if (Spec.Tags.HasTag(Pair.Key) && !Pair.Value.IsEmpty() && !Pair.Value.Matches(SurfaceTags))
			{
				Problem = FText::Format(LOCTEXT("SpecRequirement", "Objects tagged {0} need a different surface."), FText::FromName(Pair.Key.GetTagName()));
				break;
			}
		}
	}
	if (Problem.IsEmpty())
	{
		return EEGPlacementRuleResult::Pass;
	}
	if (!State.bAreaAvailable)
	{
		return Wait(Output, TAG_EGPlacement_Reason_Pending_Region, LOCTEXT("TagsPending", "The area's regions are still loading."));
	}
	return Fail(Output, TAG_EGPlacement_Reason_SurfaceTag, Problem);
}

#undef LOCTEXT_NAMESPACE
