// Copyright Moon Punch Games. All Rights Reserved.

#include "EELocalizationSession.h"

#if WITH_EDITOR

#include "Internationalization/InternationalizationManifest.h"
#include "Internationalization/InternationalizationArchive.h"
#include "LocalizationSettings.h"
#include "LocalizationTargetTypes.h"
#include "LocTextHelper.h"
#include "Misc/Paths.h"

void FEELocalizationSession::GetAvailableTargets(TArray<FString>& OutTargetNames)
{
	if (const ULocalizationTargetSet* GameTargetSet = ULocalizationSettings::GetGameTargetSet())
	{
		for (const TObjectPtr<ULocalizationTarget>& Target : GameTargetSet->TargetObjects)
		{
			if (Target)
			{
				OutTargetNames.Add(Target->Settings.Name);
			}
		}
	}
}

bool FEELocalizationSession::Open(const FString& InTargetName, FString& OutError)
{
	Close();

	// Resolve the target's cultures from the Localization Dashboard configuration.
	const ULocalizationTargetSet* GameTargetSet = ULocalizationSettings::GetGameTargetSet();
	const ULocalizationTarget* FoundTarget = nullptr;
	if (GameTargetSet)
	{
		for (const TObjectPtr<ULocalizationTarget>& Target : GameTargetSet->TargetObjects)
		{
			if (Target && Target->Settings.Name == InTargetName)
			{
				FoundTarget = Target;
				break;
			}
		}
	}

	if (!FoundTarget)
	{
		OutError = FString::Printf(TEXT("Localization target '%s' not found in the game target set."), *InTargetName);
		return false;
	}

	const FLocalizationTargetSettings& Settings = FoundTarget->Settings;
	if (!Settings.SupportedCulturesStatistics.IsValidIndex(Settings.NativeCultureIndex))
	{
		OutError = FString::Printf(TEXT("Target '%s' has no valid native culture."), *InTargetName);
		return false;
	}

	NativeCulture = Settings.SupportedCulturesStatistics[Settings.NativeCultureIndex].CultureName;
	ForeignCultures.Reset();
	for (int32 i = 0; i < Settings.SupportedCulturesStatistics.Num(); ++i)
	{
		if (i != Settings.NativeCultureIndex)
		{
			ForeignCultures.Add(Settings.SupportedCulturesStatistics[i].CultureName);
		}
	}

	const FString TargetPath = FPaths::ProjectContentDir() / TEXT("Localization") / InTargetName;
	LocTextHelper = MakeShared<FLocTextHelper>(
		TargetPath,
		InTargetName + TEXT(".manifest"),
		InTargetName + TEXT(".archive"),
		NativeCulture,
		ForeignCultures,
		nullptr);

	FText LoadError;
	if (!LocTextHelper->LoadAll(ELocTextHelperLoadFlags::Load, &LoadError))
	{
		OutError = FString::Printf(TEXT("Failed to load '%s': %s"), *InTargetName, *LoadError.ToString());
		LocTextHelper.Reset();
		return false;
	}

	TargetName = InTargetName;

	// Flatten manifest entries (one per context/key) with per-culture translation states.
	TArray<FString> AllCultures = ForeignCultures;

	LocTextHelper->EnumerateSourceTexts([this, &AllCultures](TSharedRef<FManifestEntry> ManifestEntry) -> bool
	{
		for (const FManifestContext& Context : ManifestEntry->Contexts)
		{
			TSharedPtr<FEELocEntry> Entry = MakeShared<FEELocEntry>();
			Entry->Namespace = ManifestEntry->Namespace.GetString();
			Entry->Key = Context.Key.GetString();
			Entry->SourceText = ManifestEntry->Source.Text;
			Entry->SourceLocation = Context.SourceLocation;
			Entry->KeyMetadata = Context.KeyMetadataObj;

			if (Entry->SourceLocation.StartsWith(TEXT("/")))
			{
				Entry->SourceType = TEXT("Asset");
			}
			else if (Entry->SourceLocation.Contains(TEXT(".cpp")) || Entry->SourceLocation.Contains(TEXT(".h")) || Entry->SourceLocation.Contains(TEXT(" - line ")))
			{
				Entry->SourceType = TEXT("C++");
			}
			else
			{
				Entry->SourceType = TEXT("Other");
			}

			for (const FString& Culture : AllCultures)
			{
				RefreshEntryState(*Entry, Culture);
			}

			Entries.Add(MoveTemp(Entry));
		}
		return true;
	}, /*bCheckDependencies*/ true);

	bDirty = false;
	return true;
}

void FEELocalizationSession::Close()
{
	LocTextHelper.Reset();
	Entries.Reset();
	TargetName.Reset();
	NativeCulture.Reset();
	ForeignCultures.Reset();
	bDirty = false;
}

void FEELocalizationSession::RefreshEntryState(FEELocEntry& Entry, const FString& Culture)
{
	FEELocTranslation Translation;

	const TSharedPtr<FArchiveEntry> ArchiveEntry = LocTextHelper->FindTranslation(
		Culture, FLocKey(Entry.Namespace), FLocKey(Entry.Key), Entry.KeyMetadata);

	if (!ArchiveEntry.IsValid() || ArchiveEntry->Translation.Text.IsEmpty())
	{
		Translation.State = EEELocEntryState::Missing;
	}
	else
	{
		Translation.Text = ArchiveEntry->Translation.Text;
		if (ArchiveEntry->Source.Text != Entry.SourceText)
		{
			Translation.State = EEELocEntryState::Stale;
			Translation.StaleSourceText = ArchiveEntry->Source.Text;
		}
		else if (Translation.Text == Entry.SourceText)
		{
			Translation.State = EEELocEntryState::Identical;
		}
		else
		{
			Translation.State = EEELocEntryState::Translated;
		}
	}

	Entry.Translations.Add(Culture, MoveTemp(Translation));
}

int32 FEELocalizationSession::CountState(const FString& Culture, const EEELocEntryState State) const
{
	int32 Count = 0;
	for (const TSharedPtr<FEELocEntry>& Entry : Entries)
	{
		if (const FEELocTranslation* Translation = Entry->Translations.Find(Culture))
		{
			if (Translation->State == State)
			{
				++Count;
			}
		}
	}
	return Count;
}

bool FEELocalizationSession::SetTranslation(const TSharedPtr<FEELocEntry>& Entry, const FString& Culture, const FString& NewText, FString& OutError)
{
	if (!IsOpen() || !Entry.IsValid())
	{
		OutError = TEXT("No open session.");
		return false;
	}

	if (Culture == NativeCulture)
	{
		OutError = TEXT("Native source text is edited at its source (asset/code), not in archives.");
		return false;
	}

	const FLocKey NamespaceKey(Entry->Namespace);
	const FLocKey KeyKey(Entry->Key);
	const FLocItem Source(Entry->SourceText);
	const FLocItem Translation(NewText);

	// Update refreshes an existing archive entry (also un-staling it by rewriting its source);
	// Add covers entries that never had a translation.
	if (!LocTextHelper->UpdateTranslation(Culture, NamespaceKey, KeyKey, Entry->KeyMetadata, Source, Translation))
	{
		if (!LocTextHelper->AddTranslation(Culture, NamespaceKey, KeyKey, Entry->KeyMetadata, Source, Translation, /*bOptional*/ false))
		{
			OutError = FString::Printf(TEXT("Failed to write translation for %s,%s (%s)."),
				*Entry->Namespace, *Entry->Key, *Culture);
			return false;
		}
	}

	RefreshEntryState(*Entry, Culture);
	bDirty = true;
	return true;
}

bool FEELocalizationSession::Save(FString& OutError)
{
	if (!IsOpen())
	{
		OutError = TEXT("No open session.");
		return false;
	}

	// Archives only: the manifest belongs to the gather step, and rewriting it here would
	// create noisy diffs for edits that only touched translations.
	FText SaveError;
	if (!LocTextHelper->SaveAllArchives(&SaveError))
	{
		OutError = SaveError.ToString();
		return false;
	}

	bDirty = false;
	return true;
}

#endif // WITH_EDITOR
