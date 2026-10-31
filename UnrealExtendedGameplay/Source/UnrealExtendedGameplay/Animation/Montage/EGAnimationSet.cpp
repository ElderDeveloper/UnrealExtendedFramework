// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGAnimationSet.h"

#include "Animation/AnimMontage.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "EGAnimationSet"

namespace
{
	/** Parent chains longer than this are treated as a cycle. */
	constexpr int32 MaxParentDepth = 16;
}

const FEGAnimationSetEntry* UEGAnimationSet::FindExactEntryWithOwner(FGameplayTag Tag, const UEGAnimationSet*& OutOwner) const
{
	OutOwner = nullptr;
	if (!Tag.IsValid())
	{
		return nullptr;
	}

	const UEGAnimationSet* Set = this;
	for (int32 Depth = 0; Set && Depth < MaxParentDepth; ++Depth, Set = Set->Parent)
	{
		if (const FEGAnimationSetEntry* Entry = Set->Entries.Find(Tag))
		{
			OutOwner = Set;
			return Entry;
		}
	}
	return nullptr;
}

const FEGAnimationSetEntry* UEGAnimationSet::FindExactEntry(FGameplayTag Tag) const
{
	const UEGAnimationSet* Owner = nullptr;
	return FindExactEntryWithOwner(Tag, Owner);
}

const FEGAnimationSetEntry* UEGAnimationSet::FindEntry(FGameplayTag Tag) const
{
	const UEGAnimationSet* const Self = this;
	return FindEntryInSets(MakeArrayView(&Self, 1), Tag, false);
}

const FEGAnimationSetEntry* UEGAnimationSet::FindEntryInSets(TArrayView<const UEGAnimationSet* const> Sets, FGameplayTag Tag, bool bExactTag, const UEGAnimationSet** OutOwner)
{
	// Tag-major: the most specific tag in any set beats a less specific one in an earlier set.
	for (FGameplayTag Current = Tag; Current.IsValid(); Current = Current.RequestDirectParent())
	{
		for (const UEGAnimationSet* Set : Sets)
		{
			const UEGAnimationSet* Owner = nullptr;
			if (const FEGAnimationSetEntry* Entry = Set ? Set->FindExactEntryWithOwner(Current, Owner) : nullptr)
			{
				if (OutOwner)
				{
					*OutOwner = Owner;
				}
				return Entry;
			}
		}
		if (bExactTag)
		{
			break;
		}
	}
	return nullptr;
}

bool UEGAnimationSet::PickFromEntry(const FEGAnimationSetEntry& Entry, FGameplayTag CursorKey, int32 VariantIndex, TSoftObjectPtr<UAnimMontage>& OutMontage) const
{
	const int32 Count = Entry.Variants.Num();
	if (Count == 0)
	{
		return false;
	}

	int32 Index = 0;
	if (VariantIndex >= 0)
	{
		Index = VariantIndex % Count;
	}
	else
	{
		switch (Entry.Pick)
		{
		case EEGAnimationVariantPick::Random:
			Index = FMath::RandRange(0, Count - 1);
			break;
		case EEGAnimationVariantPick::Sequential:
		{
			int32& Cursor = SequentialCursor.FindOrAdd(CursorKey);
			Index = Cursor % Count;
			Cursor = (Cursor + 1) % Count;
			break;
		}
		case EEGAnimationVariantPick::First:
		default:
			Index = 0;
			break;
		}
	}

	OutMontage = Entry.Variants[Index];
	return !OutMontage.IsNull();
}

bool UEGAnimationSet::PickVariantInSets(TArrayView<const UEGAnimationSet* const> Sets, FGameplayTag Tag, bool bExactTag, int32 VariantIndex, TSoftObjectPtr<UAnimMontage>& OutMontage, FEGAnimationSetEntry& OutEntry)
{
	const UEGAnimationSet* Owner = nullptr;
	const FEGAnimationSetEntry* Entry = FindEntryInSets(Sets, Tag, bExactTag, &Owner);
	if (!Entry || !Owner)
	{
		return false;
	}

	OutEntry = *Entry;
	// The sequential cursor lives on the set that owns the entry, keyed by the tag asked for.
	return Owner->PickFromEntry(*Entry, Tag, VariantIndex, OutMontage);
}

bool UEGAnimationSet::PickVariant(FGameplayTag Tag, int32 VariantIndex, TSoftObjectPtr<UAnimMontage>& OutMontage, FEGAnimationSetEntry& OutEntry) const
{
	const UEGAnimationSet* const Self = this;
	return PickVariantInSets(MakeArrayView(&Self, 1), Tag, false, VariantIndex, OutMontage, OutEntry);
}

FEGResolvedAnimation UEGAnimationSet::ResolveAnimation(FGameplayTag Tag, int32 VariantIndex) const
{
	FEGResolvedAnimation Resolved;
	TSoftObjectPtr<UAnimMontage> Montage;
	FEGAnimationSetEntry Entry;
	if (PickVariant(Tag, VariantIndex, Montage, Entry))
	{
		Resolved.Montage = Montage.LoadSynchronous();
		Resolved.StartSection = Entry.StartSection;
		Resolved.PlayRate = Entry.PlayRate;
		Resolved.StartTime = Entry.StartTime;
	}
	return Resolved;
}

void UEGAnimationSet::GatherSoftPaths(TArray<FSoftObjectPath>& OutPaths) const
{
	const UEGAnimationSet* Set = this;
	for (int32 Depth = 0; Set && Depth < MaxParentDepth; ++Depth, Set = Set->Parent)
	{
		for (const TPair<FGameplayTag, FEGAnimationSetEntry>& Pair : Set->Entries)
		{
			for (const TSoftObjectPtr<UAnimMontage>& Variant : Pair.Value.Variants)
			{
				if (!Variant.IsNull())
				{
					OutPaths.AddUnique(Variant.ToSoftObjectPath());
				}
			}
		}
	}
}

#if WITH_EDITOR
EDataValidationResult UEGAnimationSet::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	const UEGAnimationSet* Set = Parent;
	for (int32 Depth = 0; Set; ++Depth, Set = Set->Parent)
	{
		if (Set == this || Depth >= MaxParentDepth)
		{
			Context.AddError(LOCTEXT("ParentCycle", "Parent chain loops back to this set (or is deeper than 16)."));
			Result = EDataValidationResult::Invalid;
			break;
		}
	}

	for (const TPair<FGameplayTag, FEGAnimationSetEntry>& Pair : Entries)
	{
		if (!Pair.Key.IsValid())
		{
			Context.AddError(LOCTEXT("EmptyTag", "An entry has no tag."));
			Result = EDataValidationResult::Invalid;
		}
		if (Pair.Value.Variants.Num() == 0 || Pair.Value.Variants.ContainsByPredicate([](const TSoftObjectPtr<UAnimMontage>& Variant) { return Variant.IsNull(); }))
		{
			Context.AddWarning(FText::Format(LOCTEXT("EmptyVariant", "{0} has no montage, or an empty variant slot."), FText::FromString(Pair.Key.ToString())));
		}
	}
	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
