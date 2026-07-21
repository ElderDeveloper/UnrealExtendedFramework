// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

class FLocTextHelper;
class FLocMetadataObject;

/** Textual translation state derived from manifest/archive comparison (LW-1). */
enum class EEELocEntryState : uint8
{
	/** No translation in the culture's archive. */
	Missing,
	/** Translation exists but its recorded source no longer matches the manifest source. */
	Stale,
	/** Translation exists and is identical to the source text (suspicious for foreign cultures). */
	Identical,
	/** Translation exists and is current. */
	Translated
};

struct FEELocTranslation
{
	FString Text;
	EEELocEntryState State = EEELocEntryState::Missing;
	/** When Stale: the source text this translation was made against. */
	FString StaleSourceText;
};

/** One namespace/key with its source text and per-culture translations. */
struct FEELocEntry
{
	FString Namespace;
	FString Key;
	FString SourceText;
	FString SourceLocation;
	/** Coarse origin bucket derived from the source location: "C++", "Asset", or "Other". */
	FString SourceType;
	TSharedPtr<FLocMetadataObject> KeyMetadata;
	/** Culture code -> translation. */
	TMap<FString, FEELocTranslation> Translations;
};

/**
 * LW-1 localization session: opens one Unreal localization target's manifest and archives
 * through the engine's own FLocTextHelper (so writes round-trip through the same serialization
 * the pipeline uses), derives Missing/Stale/Identical/Translated states, and stages edits in
 * memory until Save().
 *
 * The session never replaces the manifest/archive/LocRes model — it is a view over it.
 */
class UNREALEXTENDEDFRAMEWORKEDITOR_API FEELocalizationSession
{
public:
	/** Names of game localization targets configured in the Localization Dashboard. */
	static void GetAvailableTargets(TArray<FString>& OutTargetNames);

	/** Opens a target by name; loads manifest + all culture archives. */
	bool Open(const FString& TargetName, FString& OutError);
	void Close();

	bool IsOpen() const { return LocTextHelper.IsValid(); }
	const FString& GetTargetName() const { return TargetName; }
	const FString& GetNativeCulture() const { return NativeCulture; }
	const TArray<FString>& GetForeignCultures() const { return ForeignCultures; }
	const TArray<TSharedPtr<FEELocEntry>>& GetEntries() const { return Entries; }
	bool HasUnsavedChanges() const { return bDirty; }

	/** Per-culture counts for the given state. */
	int32 CountState(const FString& Culture, EEELocEntryState State) const;

	/** Stages a translation edit (updates archive in memory and the entry's state). */
	bool SetTranslation(const TSharedPtr<FEELocEntry>& Entry, const FString& Culture, const FString& NewText, FString& OutError);

	/** Writes manifest + archives back to disk through the engine serialization. */
	bool Save(FString& OutError);

private:
	void RefreshEntryState(FEELocEntry& Entry, const FString& Culture);

	TSharedPtr<FLocTextHelper> LocTextHelper;
	TArray<TSharedPtr<FEELocEntry>> Entries;
	FString TargetName;
	FString NativeCulture;
	TArray<FString> ForeignCultures;
	bool bDirty = false;
};

#endif // WITH_EDITOR
