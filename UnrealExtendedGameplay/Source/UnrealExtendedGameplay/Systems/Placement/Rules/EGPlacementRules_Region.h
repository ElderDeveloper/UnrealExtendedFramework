// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UnrealExtendedGameplay/Systems/Placement/Rules/EGPlacementRule.h"

#include "EGPlacementRules_Region.generated.h"

/**
 * The footprint is inside the union of the matching include regions.
 *
 * Exact in the common cases: a box inside one region box, and a box inside several regions that
 * share its plane (floor zones), where the footprint's shadow is subtracted from the union of the
 * region shadows. Regions that do not share a plane are sampled, with the spacing and cap below,
 * and the answer is marked approximate. Where regions may be unloaded, a failure is Pending.
 */
UCLASS(meta = (DisplayName = "Region containment"))
class UNREALEXTENDEDGAMEPLAY_API UEGPlacementRule_RegionContainment : public UEGPlacementRule
{
	GENERATED_BODY()

public:
	/** Which include regions count. Empty: every include region. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	FGameplayTagQuery RegionFilter;

	/** Test the clearance boxes instead of the footprint. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	bool bUseClearance = false;

	/** Sample spacing for regions that do not share a plane, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement", meta = (ClampMin = "0.5"))
	float SampleSpacing = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement", meta = (ClampMin = "8"))
	int32 MaxSamples = 1000;

	virtual EEGPlacementRuleResult Check_Implementation(const UEGPlacementProfile* Profile, const FEGPlacementSpec& Spec, const FEGPlacementIntent& Intent,
		const FEGPlacementContext& Context, const FEGPlacementState& State, FEGPlacementRuleOutput& Output) const override;
};

/**
 * The footprint touches no matching exclusion region, by exact oriented-box intersection; thin
 * boxes included. Where regions may be unloaded the answer is Pending, since a missing region may
 * be an exclusion.
 */
UCLASS(meta = (DisplayName = "Region exclusion"))
class UNREALEXTENDEDGAMEPLAY_API UEGPlacementRule_RegionExclusion : public UEGPlacementRule
{
	GENERATED_BODY()

public:
	/** Which exclusion regions count. Empty: every exclusion region. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	FGameplayTagQuery RegionFilter;

	/** Test the clearance boxes instead of the footprint. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	bool bUseClearance = true;

	virtual EEGPlacementRuleResult Check_Implementation(const UEGPlacementProfile* Profile, const FEGPlacementSpec& Spec, const FEGPlacementIntent& Intent,
		const FEGPlacementContext& Context, const FEGPlacementState& State, FEGPlacementRuleOutput& Output) const override;
};
