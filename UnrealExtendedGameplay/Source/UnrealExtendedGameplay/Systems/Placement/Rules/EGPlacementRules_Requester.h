// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealExtendedGameplay/Systems/Placement/Rules/EGPlacementRule.h"

#include "EGPlacementRules_Requester.generated.h"

/**
 * Distance from the requester's view (or eyes) to the nearest point of the footprint. Calls
 * without a requester and without a view location (scripts, AI without a body) pass.
 */
UCLASS(meta = (DisplayName = "Reach"))
class UNREALEXTENDEDGAMEPLAY_API UEGPlacementRule_Reach : public UEGPlacementRule
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement", meta = (ClampMin = "0.0"))
	float MaxDistance = 400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement", meta = (ClampMin = "0.0"))
	float MinDistance = 0.0f;

	virtual EEGPlacementRuleResult Check_Implementation(const UEGPlacementProfile* Profile, const FEGPlacementSpec& Spec, const FEGPlacementIntent& Intent,
		const FEGPlacementContext& Context, const FEGPlacementState& State, FEGPlacementRuleOutput& Output) const override;
};

/** The requester's view can see the pose: one unblocked line to any footprint box centre. */
UCLASS(meta = (DisplayName = "Line of sight"))
class UNREALEXTENDEDGAMEPLAY_API UEGPlacementRule_LineOfSight : public UEGPlacementRule
{
	GENERATED_BODY()

public:
	/** Use the profile's channel or collision profile. Off uses Channel. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	bool bUseProfileQuery = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement", meta = (EditCondition = "!bUseProfileQuery"))
	TEnumAsByte<ECollisionChannel> Channel = ECC_Visibility;

	/** A hit this close to a footprint box (on its support, for one) still counts as seeing it, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement", meta = (ClampMin = "0.0"))
	float Tolerance = 5.0f;

	virtual EEGPlacementRuleResult Check_Implementation(const UEGPlacementProfile* Profile, const FEGPlacementSpec& Spec, const FEGPlacementIntent& Intent,
		const FEGPlacementContext& Context, const FEGPlacementState& State, FEGPlacementRuleOutput& Output) const override;
};
