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

const FEGAnimationSetEntry* UEGAnimationSet::FindEntry(FGameplayTag Tag) const
{
	if (!Tag.IsValid())
	{
		return nullptr;
	}

	const UEGAnimationSet* Set = this;
	for (int32 Depth = 0; Set && Depth < MaxParentDepth; ++Depth, Set = Set->Parent)
	{
		for (FGameplayTag Current = Tag; Current.IsValid(); Current = Current.RequestDirectParent())
		{
			if (const FEGAnimationSetEntry* Entry = Set->Entries.Find(Current))
			{
				return Entry;
			}
		}
	}
	return nullptr;
}

bool UEGAnimationSet::PickVariant(FGameplayTag Tag, int32 VariantIndex, TSoftObjectPtr<UAnimMontage>& OutMontage, FEGAnimationSetEntry& OutEntry) const
{
	const FEGAnimationSetEntry* Entry = FindEntry(Tag);
	if (!Entry || Entry->Variants.Num() == 0)
	{
		return false;
	}

	const int32 Count = Entry->Variants.Num();
	int32 Index = 0;
	if (VariantIndex >= 0)
	{
		Index = VariantIndex % Count;
	}
	else
	{
		switch (Entry->Pick)
		{
		case EEGAnimationVariantPick::Random:
			Index = FMath::RandRange(0, Count - 1);
			break;
		case EEGAnimationVariantPick::Sequential:
		{
			int32& Cursor = SequentialCursor.FindOrAdd(Tag);
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

	OutMontage = Entry->Variants[Index];
	OutEntry = *Entry;
	return !OutMontage.IsNull();
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
