// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGAnimGraphNode_FootLock.h"

#define LOCTEXT_NAMESPACE "EGAnimGraphNode_FootLock"

FText UAnimGraphNode_EGFootLock::GetControllerDescription() const
{
	return LOCTEXT("Description", "Foot Lock");
}

FText UAnimGraphNode_EGFootLock::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return GetControllerDescription();
}

FText UAnimGraphNode_EGFootLock::GetTooltipText() const
{
	return LOCTEXT("Tooltip", "Keeps planted feet fixed in the world while the standing character's capsule rotates under them, and steps a foot when it drifts or twists too far. Feet the animation moves follow the animation. Defaults to the UE5 mannequin's foot_l and foot_r.");
}

#undef LOCTEXT_NAMESPACE
