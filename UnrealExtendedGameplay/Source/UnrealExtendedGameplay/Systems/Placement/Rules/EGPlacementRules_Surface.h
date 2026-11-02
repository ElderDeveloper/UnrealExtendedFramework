// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UnrealExtendedGameplay/Systems/Placement/Rules/EGPlacementRule.h"

#include "EGPlacementRules_Surface.generated.h"

/** The measured surface orientation is one the spec allows. */
UCLASS(meta = (DisplayName = "Surface orientation"))
class UNREALEXTENDEDGAMEPLAY_API UEGPlacementRule_SurfaceOrientation : public UEGPlacementRule
{
	GENERATED_BODY()

public:
	/** Anchored results take the anchor's pose and have no surface to classify. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	bool bPassAnchored = true;

	virtual EEGPlacementRuleResult Check_Implementation(const UEGPlacementProfile* Profile, const FEGPlacementSpec& Spec, const FEGPlacementIntent& Intent,
		const FEGPlacementContext& Context, const FEGPlacementState& State, FEGPlacementRuleOutput& Output) const override;
};

/** The slope is within the spec's limit, or the profile's default. */
UCLASS(meta = (DisplayName = "Slope"))
class UNREALEXTENDEDGAMEPLAY_API UEGPlacementRule_Slope : public UEGPlacementRule
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	bool bPassAnchored = true;

	virtual EEGPlacementRuleResult Check_Implementation(const UEGPlacementProfile* Profile, const FEGPlacementSpec& Spec, const FEGPlacementIntent& Intent,
		const FEGPlacementContext& Context, const FEGPlacementState& State, FEGPlacementRuleOutput& Output) const override;
};

/**
 * Surface tags (component and actor tags, the profile's physical material table, the support
 * interface, include regions) meet the requirements. Where regions may be unloaded a failure is
 * Pending, since a missing region may be the one lending the tag.
 */
UCLASS(meta = (DisplayName = "Surface tags"))
class UNREALEXTENDEDGAMEPLAY_API UEGPlacementRule_SurfaceTags : public UEGPlacementRule
{
	GENERATED_BODY()

public:
	/** Every surface must match. Empty: no requirement. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	FGameplayTagQuery Required;

	/** When the spec has the key tag, the surface must match the query ("needs a timber support"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	TMap<FGameplayTag, FGameplayTagQuery> SpecRequirements;

	/** Surfaces carrying any of these refuse everything. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	FGameplayTagContainer RefusedSurfaceTags;

	virtual EEGPlacementRuleResult Check_Implementation(const UEGPlacementProfile* Profile, const FEGPlacementSpec& Spec, const FEGPlacementIntent& Intent,
		const FEGPlacementContext& Context, const FEGPlacementState& State, FEGPlacementRuleOutput& Output) const override;
};
