// Copyright Moonpunch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimGraphNode_SkeletalControlBase.h"
#include "Animation/EFAnimNode_WeaponPhysics.h"
#include "EEAnimGraphNode_WeaponPhysics.generated.h"

/**
 * AnimGraph editor node for Weapon Physics (Spring + Tilt).
 *
 * Exposes FAnimNode_WeaponPhysics in the AnimGraph editor as a drag-and-drop
 * node with single-bone, one-arm, and two-arm weapon physics controls.
 */
UCLASS()
class UNREALEXTENDEDEDITOR_API UEEAnimGraphNode_WeaponPhysics : public UAnimGraphNode_SkeletalControlBase
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, Category = "Settings")
	FAnimNode_WeaponPhysics Node;

	// UEdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FText GetMenuCategory() const override;
	// End of UEdGraphNode interface

protected:

	// UAnimGraphNode_SkeletalControlBase interface
	virtual const FAnimNode_SkeletalControlBase* GetNode() const override { return &Node; }
	// End of UAnimGraphNode_SkeletalControlBase interface
};
