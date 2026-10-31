// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EGTurnInPlaceTypes.h"
#include "Engine/DataAsset.h"

#include "EGTurnInPlaceSet.generated.h"

class UAnimSequenceBase;

/**
 * UEGTurnInPlaceSet
 *
 * A character's turn animations and settings, per stance. The stance is picked once per frame on
 * the game thread from the owner's anim state tags (IEGAnimStateProvider): the first key of
 * Stances found in them wins, otherwise Default. Movement and animation read the same choice.
 *
 * Read-only at runtime: the anim worker thread reads entries straight from the asset.
 */
UCLASS(BlueprintType)
class UNREALEXTENDEDGAMEPLAY_API UEGTurnInPlaceSet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Turn In Place")
	FEGTurnInPlaceEntry Default;

	/** Checked in order; the first key present in the owner's anim state tags is used. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Turn In Place", meta = (ForceInlineRow))
	TMap<FGameplayTag, FEGTurnInPlaceEntry> Stances;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Turn In Place")
	FEGTurnInPlaceCurveNames Curves;

	/** The entry for a stance key from FindStanceKey; Default for an empty or unknown key. */
	const FEGTurnInPlaceEntry& GetEntry(const FGameplayTag& StanceKey) const;

	/** The first Stances key contained in StateTags, or an empty tag for Default. */
	FGameplayTag FindStanceKey(const FGameplayTagContainer& StateTags) const;

	/** An entry with defaults and no animations, for code paths without a set. */
	static const FEGTurnInPlaceEntry& GetEmptyEntry();

	/** The clip has the RemainingTurnYaw and TurnYawWeight curves (from the curve modifier). */
	static bool HasTurnCurves(const UAnimSequenceBase* Sequence, const FEGTurnInPlaceCurveNames& Names);

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
