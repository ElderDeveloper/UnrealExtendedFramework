// Copyright Moonpunch Games. All Rights Reserved.

#include "EEAnimGraphNode_WeaponPhysics.h"

#define LOCTEXT_NAMESPACE "AnimNode_WeaponPhysics"

FText UEEAnimGraphNode_WeaponPhysics::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("NodeTitle", "Weapon Physics (Spring + Tilt)");
}

FText UEEAnimGraphNode_WeaponPhysics::GetTooltipText() const
{
	return LOCTEXT("Tooltip",
		"Applies spring lag, velocity tilt, and locomotion sway to a weapon bone, one arm, or both arms.\n"
		"Wire CharacterVelocity and CharacterAcceleration from the AnimInstance to drive the effect.\n\n"
		"Use Single Bone for legacy weapon/grip setups, or Right Arm, Left Arm, and Both Arms for arm-driven FPS weapon physics.");
}

FText UEEAnimGraphNode_WeaponPhysics::GetMenuCategory() const
{
	return LOCTEXT("MenuCategory", "UEF | Animation");
}

#undef LOCTEXT_NAMESPACE
