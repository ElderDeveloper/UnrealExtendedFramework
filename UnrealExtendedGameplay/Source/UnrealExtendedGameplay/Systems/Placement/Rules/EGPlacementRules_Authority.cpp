// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGPlacementRules_Authority.h"

#include "UnrealExtendedGameplay/Systems/Placement/Core/EGPlacementEvaluator.h"
#include "UnrealExtendedGameplay/Systems/Placement/Core/EGPlacementTags.h"

#define LOCTEXT_NAMESPACE "EGPlacementRules"

UEGPlacementRule_ClaimMatch::UEGPlacementRule_ClaimMatch()
{
	Scope = EEGPlacementRuleScope::AuthorityOnly;
}

EEGPlacementRuleResult UEGPlacementRule_ClaimMatch::Check_Implementation(const UEGPlacementProfile* Profile, const FEGPlacementSpec& Spec, const FEGPlacementIntent& Intent,
	const FEGPlacementContext& Context, const FEGPlacementState& State, FEGPlacementRuleOutput& Output) const
{
	const FEGPlacementClaims& Claims = Intent.Claims;
	if (!Claims.bHasClaims)
	{
		return EEGPlacementRuleResult::Pass;
	}
	EEGPlacementClaimMismatch Mismatch = EEGPlacementClaimMismatch::None;
	if (bCheckOrientation && Claims.Orientation != EEGPlacementOrientation::None && Claims.Orientation != State.Orientation)
	{
		Mismatch |= EEGPlacementClaimMismatch::Orientation;
	}
	if (bCheckSupport && Claims.Support != State.Support.Handle)
	{
		Mismatch |= EEGPlacementClaimMismatch::Support;
	}
	if (bCheckAnchor && Claims.Anchor != State.Anchor)
	{
		Mismatch |= EEGPlacementClaimMismatch::Anchor;
	}
	if (bCheckPose)
	{
		const double Distance = FVector::Dist(Intent.Pose.GetLocation(), State.Pose.GetLocation());
		const double Angle = FMath::RadiansToDegrees(Intent.Pose.GetRotation().AngularDistance(State.Pose.GetRotation()));
		if (Distance > PoseTolerance || Angle > AngleToleranceDegrees)
		{
			Mismatch |= EEGPlacementClaimMismatch::Pose;
		}
	}
	if (Mismatch == EEGPlacementClaimMismatch::None)
	{
		return EEGPlacementRuleResult::Pass;
	}
	Output.ClaimMismatches |= static_cast<int32>(Mismatch);
	return Fail(Output, TAG_EGPlacement_Reason_ClaimMismatch, LOCTEXT("Mismatch", "The spot is not what the requester saw."));
}

UEGPlacementRule_Revision::UEGPlacementRule_Revision()
{
	Scope = EEGPlacementRuleScope::AuthorityOnly;
}

EEGPlacementRuleResult UEGPlacementRule_Revision::Check_Implementation(const UEGPlacementProfile* Profile, const FEGPlacementSpec& Spec, const FEGPlacementIntent& Intent,
	const FEGPlacementContext& Context, const FEGPlacementState& State, FEGPlacementRuleOutput& Output) const
{
	for (const FEGPlacementRevision& Seen : Intent.Claims.Revisions)
	{
		const UObject* Source = Seen.Source.Get();
		if (!Source || EGPlacement::GetRevision(Source) != Seen.Revision)
		{
			return Fail(Output, TAG_EGPlacement_Reason_Stale, LOCTEXT("Stale", "Something changed since the spot was chosen."));
		}
	}
	return EEGPlacementRuleResult::Pass;
}

#undef LOCTEXT_NAMESPACE
