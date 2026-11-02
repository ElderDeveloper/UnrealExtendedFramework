// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGPlacementRule.h"

EEGPlacementRuleResult UEGPlacementRule::Check_Implementation(const UEGPlacementProfile* Profile, const FEGPlacementSpec& Spec, const FEGPlacementIntent& Intent,
	const FEGPlacementContext& Context, const FEGPlacementState& State, FEGPlacementRuleOutput& Output) const
{
	return EEGPlacementRuleResult::Pass;
}

bool UEGPlacementRule::RunsIn(EEGPlacementScope InScope) const
{
	if (!bEnabled)
	{
		return false;
	}
	switch (Scope)
	{
	case EEGPlacementRuleScope::PreviewOnly: return InScope == EEGPlacementScope::Preview;
	case EEGPlacementRuleScope::AuthorityOnly: return InScope == EEGPlacementScope::Authority;
	default: return true;
	}
}

EEGPlacementRuleResult UEGPlacementRule::Fail(FEGPlacementRuleOutput& Output, const FGameplayTag& Tag, FText Text) const
{
	Output.Reasons.Emplace(Tag, MoveTemp(Text), this);
	return EEGPlacementRuleResult::Fail;
}

EEGPlacementRuleResult UEGPlacementRule::Wait(FEGPlacementRuleOutput& Output, const FGameplayTag& Tag, FText Text) const
{
	Output.Reasons.Emplace(Tag, MoveTemp(Text), this);
	return EEGPlacementRuleResult::Pending;
}

void UEGPlacementRule::K2_AddReason(FEGPlacementRuleOutput& Output, FGameplayTag Tag, FText Text) const
{
	Output.Reasons.Emplace(Tag, MoveTemp(Text), this);
}
