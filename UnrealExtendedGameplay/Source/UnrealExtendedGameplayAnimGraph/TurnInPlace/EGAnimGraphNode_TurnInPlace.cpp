// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGAnimGraphNode_TurnInPlace.h"

#define LOCTEXT_NAMESPACE "EGAnimGraphNode_TurnInPlace"

FText UAnimGraphNode_EGTurnInPlace::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Title", "Turn In Place");
}

FText UAnimGraphNode_EGTurnInPlace::GetTooltipText() const
{
	return LOCTEXT("Tooltip", "Plays the owner's turn in place (UEGTurnInPlaceComponent) on top of the Source pose: picks and crossfades the turn clips, turns procedurally where a clip is missing, and writes the turn curves the component consumes. Put it after the locomotion or inside its Idle state; nothing after it may scale its curves.");
}

FString UAnimGraphNode_EGTurnInPlace::GetNodeCategory() const
{
	return TEXT("Extended|Turn In Place");
}

FLinearColor UAnimGraphNode_EGTurnInPlace::GetNodeTitleColor() const
{
	return FLinearColor(0.15f, 0.55f, 0.35f);
}

#undef LOCTEXT_NAMESPACE
