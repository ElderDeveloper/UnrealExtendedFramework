// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/StreamableManager.h"
#include "GameplayTagContainer.h"

#include "EGAnimationSet.generated.h"

class UAnimMontage;

UENUM(BlueprintType)
enum class EEGAnimationVariantPick : uint8
{
	Random,
	/** In order, wrapping. The counter is shared by everyone using this set. */
	Sequential,
	First
};

/** The montages for one tag, and how to play them. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGAnimationSetEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TArray<TSoftObjectPtr<UAnimMontage>> Variants;

	/**
	 * How a variant is chosen when the caller does not name one. Random picks differ between
	 * machines; pass a variant index for anything that has to match in multiplayer.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	EEGAnimationVariantPick Pick = EEGAnimationVariantPick::Random;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	FName StartSection;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation", meta = (ClampMin = "0.01"))
	float PlayRate = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation", meta = (ClampMin = "0", Units = "s"))
	float StartTime = 0.0f;
};

/** A montage picked from a set, with the entry's playback settings. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGResolvedAnimation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> Montage = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Animation")
	FName StartSection;

	UPROPERTY(BlueprintReadOnly, Category = "Animation")
	float PlayRate = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Animation")
	float StartTime = 0.0f;

	bool IsValid() const { return Montage != nullptr; }
};

/**
 * UEGAnimationSet
 *
 * A character's montages by gameplay tag, so systems ask for "ExtendedGameplay.Anim.Cover.GetIn"
 * instead of holding montage pointers of their own. A character supplies its set through
 * IEGMontagePlayerInterface::GetAnimationSet; a system may also carry a default set.
 *
 * The most specific tag wins: Cover.GetIn.Crouched is looked for in this set and every Parent set
 * before falling back to Cover.GetIn, so a crouched clip in a parent beats a standing one here.
 * For the same tag the nearest set wins, so a variant set only lists what differs. Montages are
 * soft references: they load when first used, or ahead of time through GatherSoftPaths.
 */
UCLASS(BlueprintType)
class UNREALEXTENDEDGAMEPLAY_API UEGAnimationSet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Consulted for any tag this set has no entry for. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UEGAnimationSet> Parent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation", meta = (ForceInlineRow))
	TMap<FGameplayTag, FEGAnimationSetEntry> Entries;

	/** The entry for exactly this tag, in this set or its Parent chain (nearest first). */
	const FEGAnimationSetEntry* FindExactEntry(FGameplayTag Tag) const;

	/** The entry for this tag or its most specific parent tag, across this set and its Parent chain. */
	const FEGAnimationSetEntry* FindEntry(FGameplayTag Tag) const;

	/**
	 * The same lookup across several sets (a character's set, then a system's fallback): the most
	 * specific tag found in any of them wins; for one tag, earlier sets win. bExactTag skips parent tags.
	 */
	static const FEGAnimationSetEntry* FindEntryInSets(TArrayView<const UEGAnimationSet* const> Sets, FGameplayTag Tag, bool bExactTag, const UEGAnimationSet** OutOwner = nullptr);

	/** PickVariant over several sets, with FindEntryInSets' order. */
	static bool PickVariantInSets(TArrayView<const UEGAnimationSet* const> Sets, FGameplayTag Tag, bool bExactTag, int32 VariantIndex, TSoftObjectPtr<UAnimMontage>& OutMontage, FEGAnimationSetEntry& OutEntry);

	UFUNCTION(BlueprintPure, Category = "Extended|Animation Set")
	bool HasAnimation(FGameplayTag Tag) const { return FindEntry(Tag) != nullptr; }

	/**
	 * Which variant would play, without loading it. VariantIndex >= 0 picks that one (wrapped).
	 * False when there is no entry or it has no variants.
	 */
	bool PickVariant(FGameplayTag Tag, int32 VariantIndex, TSoftObjectPtr<UAnimMontage>& OutMontage, FEGAnimationSetEntry& OutEntry) const;

	/**
	 * Picks a variant. A set loaded from disk starts loading its montages in PostLoad, so the
	 * montage is normally in memory; otherwise it loads synchronously (reported in development builds).
	 */
	UFUNCTION(BlueprintCallable, Category = "Extended|Animation Set")
	FEGResolvedAnimation ResolveAnimation(FGameplayTag Tag, int32 VariantIndex = -1) const;

	/** Every montage this set (and its parents) can play, for preloading. */
	void GatherSoftPaths(TArray<FSoftObjectPath>& OutPaths) const;

	/** Starts loading this set's own montages (each Parent does the same for its own). */
	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

private:
	/** Keeps the montages PostLoad started loading in memory while this set is. */
	TSharedPtr<FStreamableHandle> MontagePreloadHandle;

	const FEGAnimationSetEntry* FindExactEntryWithOwner(FGameplayTag Tag, const UEGAnimationSet*& OutOwner) const;
	bool PickFromEntry(const FEGAnimationSetEntry& Entry, FGameplayTag CursorKey, int32 VariantIndex, TSoftObjectPtr<UAnimMontage>& OutMontage) const;

	/** Sequential pick positions. Runtime only. */
	mutable TMap<FGameplayTag, int32> SequentialCursor;
};
