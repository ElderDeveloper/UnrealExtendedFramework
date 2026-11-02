// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealExtendedGameplay/Systems/Placement/Rules/EGPlacementRule.h"

#include "EGPlacementRules_Geometry.generated.h"

class AActor;

/**
 * The clearance boxes overlap nothing, tested as oriented boxes by the physics scene. The support
 * may touch them only within the profile's ContactTolerance of the contact face; every other part
 * of the supporting actor, and the support itself above that band, still blocks.
 */
UCLASS(meta = (DisplayName = "Clearance"))
class UNREALEXTENDEDGAMEPLAY_API UEGPlacementRule_Clearance : public UEGPlacementRule
{
	GENERATED_BODY()

public:
	/** Use the profile's channel or collision profile. Off uses Channel. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	bool bUseProfileQuery = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement", meta = (EditCondition = "!bUseProfileQuery"))
	TEnumAsByte<ECollisionChannel> Channel = ECC_WorldDynamic;

	/** Pawns standing in the spot do not block. Off: a pawn in the way refuses, as for any other blocker. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	bool bIgnorePawns = false;

	/** Shrinks every clearance box so neighbours that only touch it do not block, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement", meta = (ClampMin = "0.0"))
	float Skin = 0.1f;

	virtual EEGPlacementRuleResult Check_Implementation(const UEGPlacementProfile* Profile, const FEGPlacementSpec& Spec, const FEGPlacementIntent& Intent,
		const FEGPlacementContext& Context, const FEGPlacementState& State, FEGPlacementRuleOutput& Output) const override;
};

/**
 * Enough of the footprint rests on support, so objects do not hang off edges. Probes a grid on the
 * bottom face of each footprint box, Spacing apart, down to the contact plane.
 */
UCLASS(meta = (DisplayName = "Support coverage"))
class UNREALEXTENDEDGAMEPLAY_API UEGPlacementRule_SupportCoverage : public UEGPlacementRule
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement", meta = (ClampMin = "0.5"))
	float Spacing = 10.0f;

	/** Caps the probes per footprint box; spacing widens to stay under it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement", meta = (ClampMin = "4"))
	int32 MaxProbesPerBox = 64;

	/** Fraction of probes that must find support within the contact tolerance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinFraction = 0.6f;

	/** Only count probes that hit the same component as the main support. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	bool bRequireSameSupport = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	bool bPassAnchored = true;

	virtual EEGPlacementRuleResult Check_Implementation(const UEGPlacementProfile* Profile, const FEGPlacementSpec& Spec, const FEGPlacementIntent& Intent,
		const FEGPlacementContext& Context, const FEGPlacementState& State, FEGPlacementRuleOutput& Output) const override;
};

/**
 * Accepts or refuses the support by class, tag or interface: no placing on pawns, on moving
 * things, or on a shelf that is stocked rather than decorated (IEGPlacementSupportInterface).
 */
UCLASS(meta = (DisplayName = "Support filter"))
class UNREALEXTENDEDGAMEPLAY_API UEGPlacementRule_SupportFilter : public UEGPlacementRule
{
	GENERATED_BODY()

public:
	/** Refuse results that rest on nothing (anchored results always have their anchor's support). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	bool bRequireSupport = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	bool bRefusePawns = true;

	/** Refuse supports whose mobility is Movable. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	bool bRefuseMovable = false;

	/** Empty allows every class. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	TArray<TSubclassOf<AActor>> AllowedClasses;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	TArray<TSubclassOf<AActor>> RefusedClasses;

	/** Actor or component tags that must be present (any of them). Empty requires none. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	TArray<FName> RequiredTags;

	/** Actor or component tags that refuse. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	TArray<FName> RefusedTags;

	/** Ask IEGPlacementSupportInterface::CanSupportPlacement when the support's actor implements it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	bool bAskSupport = true;

	virtual EEGPlacementRuleResult Check_Implementation(const UEGPlacementProfile* Profile, const FEGPlacementSpec& Spec, const FEGPlacementIntent& Intent,
		const FEGPlacementContext& Context, const FEGPlacementState& State, FEGPlacementRuleOutput& Output) const override;
};
