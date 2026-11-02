// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealExtendedGameplay/Systems/Placement/Rules/EGPlacementRule.h"

#include "EGPlacementRules_Authority.generated.h"

/**
 * Authority scope: the requester's claims match what was measured here. A client's pose solved
 * again on the host must land where the client's ghost was, on the same kind of surface, on the
 * same support and anchor; otherwise ClaimMismatch with the mismatching parts.
 */
UCLASS(meta = (DisplayName = "Claim match"))
class UNREALEXTENDEDGAMEPLAY_API UEGPlacementRule_ClaimMatch : public UEGPlacementRule
{
	GENERATED_BODY()

public:
	UEGPlacementRule_ClaimMatch();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	bool bCheckOrientation = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	bool bCheckSupport = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	bool bCheckAnchor = true;

	/** Compare the proposed pose with the pose solved here. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	bool bCheckPose = true;

	/** In cm. Must exceed the proposal's quantization (0.01 cm) and the solvers' tolerance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement", meta = (EditCondition = "bCheckPose", ClampMin = "0.01"))
	float PoseTolerance = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement", meta = (EditCondition = "bCheckPose", ClampMin = "0.01"))
	float AngleToleranceDegrees = 1.0f;

	virtual EEGPlacementRuleScope GetMeaningfulScope() const override { return EEGPlacementRuleScope::AuthorityOnly; }

	virtual EEGPlacementRuleResult Check_Implementation(const UEGPlacementProfile* Profile, const FEGPlacementSpec& Spec, const FEGPlacementIntent& Intent,
		const FEGPlacementContext& Context, const FEGPlacementState& State, FEGPlacementRuleOutput& Output) const override;
};

/** Authority scope: every revision the requester's answer depended on is still current; otherwise Stale. */
UCLASS(meta = (DisplayName = "Revision"))
class UNREALEXTENDEDGAMEPLAY_API UEGPlacementRule_Revision : public UEGPlacementRule
{
	GENERATED_BODY()

public:
	UEGPlacementRule_Revision();

	virtual EEGPlacementRuleScope GetMeaningfulScope() const override { return EEGPlacementRuleScope::AuthorityOnly; }

	virtual EEGPlacementRuleResult Check_Implementation(const UEGPlacementProfile* Profile, const FEGPlacementSpec& Spec, const FEGPlacementIntent& Intent,
		const FEGPlacementContext& Context, const FEGPlacementState& State, FEGPlacementRuleOutput& Output) const override;
};
