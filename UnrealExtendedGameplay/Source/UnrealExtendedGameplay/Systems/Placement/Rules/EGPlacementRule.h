// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UnrealExtendedGameplay/Systems/Placement/Core/EGPlacementTypes.h"

#include "EGPlacementRule.generated.h"

class UEGPlacementProfile;

/** What a rule reports besides its verdict. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGPlacementRuleOutput
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Placement")
	TArray<FEGPlacementReason> Reasons;

	/** The rule answered by sampling rather than exactly. */
	UPROPERTY(BlueprintReadWrite, Category = "Placement")
	bool bApproximate = false;

	/** EEGPlacementClaimMismatch bits. */
	UPROPERTY(BlueprintReadWrite, Category = "Placement")
	int32 ClaimMismatches = 0;
};

/**
 * UEGPlacementRule
 * One check on a solved pose: Pass, Fail (with reasons) or Pending (data not available). Instanced
 * into a profile and shared, so a rule keeps no per-call state.
 *
 * Rules must not change the world. Native rules get const inputs; Blueprint rules must keep to it
 * too, which the compiler cannot enforce.
 *
 * Project rules (ownership, inventory, combat lock, shop open) subclass this and read the spec's
 * ProjectData and the context's ContextData.
 */
UCLASS(Abstract, EditInlineNew, DefaultToInstanced, CollapseCategories, Blueprintable)
class UNREALEXTENDEDGAMEPLAY_API UEGPlacementRule : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	bool bEnabled = true;

	/** Where the rule runs. Authority-only rules (claims, revisions, project permissions) never run for a preview. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	EEGPlacementRuleScope Scope = EEGPlacementRuleScope::Both;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Extended|Placement")
	EEGPlacementRuleResult Check(const UEGPlacementProfile* Profile, const FEGPlacementSpec& Spec, const FEGPlacementIntent& Intent,
		const FEGPlacementContext& Context, const FEGPlacementState& State, FEGPlacementRuleOutput& Output) const;
	virtual EEGPlacementRuleResult Check_Implementation(const UEGPlacementProfile* Profile, const FEGPlacementSpec& Spec, const FEGPlacementIntent& Intent,
		const FEGPlacementContext& Context, const FEGPlacementState& State, FEGPlacementRuleOutput& Output) const;

	/** The scopes in which the rule means anything. Asset validation flags a Scope outside them. */
	virtual EEGPlacementRuleScope GetMeaningfulScope() const { return EEGPlacementRuleScope::Both; }

	bool RunsIn(EEGPlacementScope InScope) const;

	/** Appends a reason raised by this rule and returns Fail. */
	EEGPlacementRuleResult Fail(FEGPlacementRuleOutput& Output, const FGameplayTag& Tag, FText Text) const;

	/** Appends a reason raised by this rule and returns Pending. */
	EEGPlacementRuleResult Wait(FEGPlacementRuleOutput& Output, const FGameplayTag& Tag, FText Text) const;

protected:
	UFUNCTION(BlueprintCallable, Category = "Extended|Placement", meta = (DisplayName = "Add Reason"))
	void K2_AddReason(UPARAM(ref) FEGPlacementRuleOutput& Output, FGameplayTag Tag, FText Text) const;
};
