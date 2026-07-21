// Copyright Moon Punch Games. All Rights Reserved.

#include "EELocSetup.h"

#if WITH_EDITOR

#include "LocalizationSettings.h"
#include "LocalizationTargetTypes.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "EELocSetup"

namespace
{
	ULocalizationTarget* FindTarget(const FString& TargetName)
	{
		ULocalizationTargetSet* GameTargetSet = ULocalizationSettings::GetGameTargetSet();
		if (!GameTargetSet)
		{
			return nullptr;
		}

		for (const TObjectPtr<ULocalizationTarget>& Target : GameTargetSet->TargetObjects)
		{
			if (Target && (TargetName.IsEmpty() || Target->Settings.Name == TargetName))
			{
				return Target;
			}
		}
		return nullptr;
	}

	/** Syncs mutated target settings back into ULocalizationSettings' config and writes DefaultEditor.ini. */
	void PersistTargetSettings()
	{
		if (ULocalizationSettings* Settings = GetMutableDefault<ULocalizationSettings>())
		{
			FPropertyChangedEvent EmptyEvent(nullptr);
			Settings->PostEditChangeProperty(EmptyEvent);
		}
	}
}

EELocSetup::EEESetupState EELocSetup::DetectState(const FString& TargetName)
{
	const ULocalizationTarget* Target = FindTarget(TargetName);
	if (!Target)
	{
		return EEESetupState::NoTargets;
	}

	const FLocalizationTargetSettings& Settings = Target->Settings;
	if (!Settings.SupportedCulturesStatistics.IsValidIndex(Settings.NativeCultureIndex))
	{
		return EEESetupState::NoCultures;
	}

	const bool bGathersText = Settings.GatherFromTextFiles.IsEnabled && Settings.GatherFromTextFiles.SearchDirectories.Num() > 0;
	const bool bGathersPackages = Settings.GatherFromPackages.IsEnabled && Settings.GatherFromPackages.IncludePathWildcards.Num() > 0;
	if (!bGathersText && !bGathersPackages)
	{
		return EEESetupState::NoGatherPaths;
	}

	const FString ManifestPath = FPaths::ProjectContentDir() / TEXT("Localization") / Settings.Name / (Settings.Name + TEXT(".manifest"));
	if (!FPaths::FileExists(ManifestPath))
	{
		return EEESetupState::NeverGathered;
	}

	return EEESetupState::Ready;
}

ULocalizationTarget* EELocSetup::CreateGameTarget(const FString& TargetName, FString& OutError)
{
	if (TargetName.TrimStartAndEnd().IsEmpty())
	{
		OutError = TEXT("Target name cannot be empty.");
		return nullptr;
	}

	if (FindTarget(TargetName))
	{
		OutError = FString::Printf(TEXT("Target '%s' already exists."), *TargetName);
		return nullptr;
	}

	ULocalizationTargetSet* GameTargetSet = ULocalizationSettings::GetGameTargetSet();
	if (!GameTargetSet)
	{
		OutError = TEXT("No game localization target set available.");
		return nullptr;
	}

	ULocalizationTarget* NewTarget = NewObject<ULocalizationTarget>(GameTargetSet);
	NewTarget->Settings.Name = TargetName;
	GameTargetSet->TargetObjects.Add(NewTarget);

	PersistTargetSettings();
	return NewTarget;
}

bool EELocSetup::ConfigureCultures(const FString& TargetName,
	const FString& NativeCulture, const TArray<FString>& ForeignCultures, FString& OutError)
{
	ULocalizationTarget* Target = FindTarget(TargetName);
	if (!Target)
	{
		OutError = FString::Printf(TEXT("Target '%s' not found."), *TargetName);
		return false;
	}

	const FString Native = NativeCulture.TrimStartAndEnd();
	if (Native.IsEmpty())
	{
		OutError = TEXT("Native culture cannot be empty.");
		return false;
	}

	TArray<FCultureStatistics> Cultures;
	Cultures.Add(FCultureStatistics(Native));
	for (const FString& Culture : ForeignCultures)
	{
		const FString Trimmed = Culture.TrimStartAndEnd();
		if (!Trimmed.IsEmpty() && Trimmed != Native)
		{
			Cultures.Add(FCultureStatistics(Trimmed));
		}
	}

	Target->Settings.SupportedCulturesStatistics = MoveTemp(Cultures);
	Target->Settings.NativeCultureIndex = 0;

	PersistTargetSettings();
	return true;
}

bool EELocSetup::AddDefaultGatherPaths(const FString& TargetName,
	const bool bGatherSource, const bool bGatherContent, FString& OutError)
{
	ULocalizationTarget* Target = FindTarget(TargetName);
	if (!Target)
	{
		OutError = FString::Printf(TEXT("Target '%s' not found."), *TargetName);
		return false;
	}

	if (!bGatherSource && !bGatherContent)
	{
		OutError = TEXT("Pick at least one gather source.");
		return false;
	}

	FLocalizationTargetSettings& Settings = Target->Settings;

	if (bGatherSource)
	{
		Settings.GatherFromTextFiles.IsEnabled = true;
		const bool bAlreadyPresent = Settings.GatherFromTextFiles.SearchDirectories.ContainsByPredicate(
			[](const FGatherTextSearchDirectory& Directory) { return Directory.Path == TEXT("Source"); });
		if (!bAlreadyPresent)
		{
			FGatherTextSearchDirectory SourceDirectory;
			SourceDirectory.Path = TEXT("Source");
			Settings.GatherFromTextFiles.SearchDirectories.Add(MoveTemp(SourceDirectory));
		}
	}

	if (bGatherContent)
	{
		Settings.GatherFromPackages.IsEnabled = true;
		const bool bAlreadyPresent = Settings.GatherFromPackages.IncludePathWildcards.ContainsByPredicate(
			[](const FGatherTextIncludePath& Include) { return Include.Pattern == TEXT("Content/*"); });
		if (!bAlreadyPresent)
		{
			FGatherTextIncludePath ContentInclude;
			ContentInclude.Pattern = TEXT("Content/*");
			Settings.GatherFromPackages.IncludePathWildcards.Add(MoveTemp(ContentInclude));

			// Never regather already-localized assets.
			FGatherTextExcludePath LocalizationExclude;
			LocalizationExclude.Pattern = TEXT("Content/Localization/*");
			Settings.GatherFromPackages.ExcludePathWildcards.Add(MoveTemp(LocalizationExclude));
		}
	}

	PersistTargetSettings();
	return true;
}

FText EELocSetup::DescribeState(const EEESetupState State)
{
	switch (State)
	{
	case EEESetupState::NoTargets:
		return LOCTEXT("NoTargets", "This project has no localization target yet. A target owns the manifest, per-culture archives, and compiled resources for one body of text (UI, dialogue, ...). Create one to begin — \"Game\" is the standard name and is already wired into the engine's default localization paths.");
	case EEESetupState::NoCultures:
		return LOCTEXT("NoCultures", "The target has no cultures configured. The native culture is the language your source text is written in; foreign cultures are the languages you will translate into. This writes the same settings the Localization Dashboard edits.");
	case EEESetupState::NoGatherPaths:
		return LOCTEXT("NoGatherPaths", "Nothing is configured to be gathered. Gathering scans your code and assets for localizable text and records it in the target's manifest. The defaults below cover C++ LOCTEXT/NSLOCTEXT and all Content packages (widgets, DataTables, StringTables).");
	case EEESetupState::NeverGathered:
		return LOCTEXT("NeverGathered", "The target is configured but text has never been gathered. Running Gather + Compile scans the project, builds the manifest and archives, and compiles localization resources. This runs the standard Unreal commandlets and can take a few minutes on first run.");
	default:
		return FText::GetEmpty();
	}
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR
