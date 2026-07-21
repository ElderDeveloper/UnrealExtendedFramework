// Copyright Moon Punch Games. All Rights Reserved.

#include "EELocPipeline.h"

#if WITH_EDITOR

#include "Internationalization/TextLocalizationManager.h"
#include "Localization/Context/EELocalizationContextProvider.h"
#include "Localization/Model/EELocalizationSession.h"
#include "Localization/Review/EELocReviewStore.h"
#include "LocalizationCommandletTasks.h"
#include "LocalizationSettings.h"
#include "LocalizationTargetTypes.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	FString CsvEscape(const FString& Value)
	{
		FString Escaped = Value;
		Escaped.ReplaceInline(TEXT("\""), TEXT("\"\""));
		Escaped.ReplaceInline(TEXT("\r\n"), TEXT("\n"));
		return FString::Printf(TEXT("\"%s\""), *Escaped);
	}
}

ULocalizationTarget* EELocPipeline::FindGameTarget(const FString& TargetName)
{
	if (const ULocalizationTargetSet* GameTargetSet = ULocalizationSettings::GetGameTargetSet())
	{
		for (const TObjectPtr<ULocalizationTarget>& Target : GameTargetSet->TargetObjects)
		{
			if (Target && Target->Settings.Name == TargetName)
			{
				return Target;
			}
		}
	}
	return nullptr;
}

bool EELocPipeline::Gather(const FString& TargetName, const TSharedRef<SWindow>& ParentWindow)
{
	ULocalizationTarget* Target = FindGameTarget(TargetName);
	return Target && LocalizationCommandletTasks::GatherTextForTarget(ParentWindow, Target);
}

bool EELocPipeline::Import(const FString& TargetName, const TSharedRef<SWindow>& ParentWindow)
{
	ULocalizationTarget* Target = FindGameTarget(TargetName);
	return Target && LocalizationCommandletTasks::ImportTextForTarget(ParentWindow, Target);
}

bool EELocPipeline::Export(const FString& TargetName, const TSharedRef<SWindow>& ParentWindow)
{
	ULocalizationTarget* Target = FindGameTarget(TargetName);
	return Target && LocalizationCommandletTasks::ExportTextForTarget(ParentWindow, Target);
}

bool EELocPipeline::Compile(const FString& TargetName, const TSharedRef<SWindow>& ParentWindow)
{
	ULocalizationTarget* Target = FindGameTarget(TargetName);
	return Target && LocalizationCommandletTasks::CompileTextForTarget(ParentWindow, Target);
}

void EELocPipeline::RefreshLiveResources()
{
	FTextLocalizationManager::Get().RefreshResources();
}

bool EELocPipeline::ExportContextPackage(const FEELocalizationSession& Session,
	const FEELocReviewStore& ReviewStore, const FString& Culture, FString& OutFilePath, FString& OutError)
{
	if (!Session.IsOpen() || Culture.IsEmpty())
	{
		OutError = TEXT("Open a target and select a culture first.");
		return false;
	}

	FString Csv = TEXT("Namespace,Key,Source,Translation,State,ReviewState,SourceLocation,Context\n");

	for (const TSharedPtr<FEELocEntry>& Entry : Session.GetEntries())
	{
		const FEELocTranslation* Translation = Entry->Translations.Find(Culture);

		FString StateText = TEXT("Missing");
		FString TranslationText;
		if (Translation)
		{
			TranslationText = Translation->Text;
			switch (Translation->State)
			{
			case EEELocEntryState::Stale: StateText = TEXT("Stale"); break;
			case EEELocEntryState::Identical: StateText = TEXT("Identical"); break;
			case EEELocEntryState::Translated: StateText = TEXT("Translated"); break;
			default: break;
			}
		}

		const FEELocReviewRecord Review = ReviewStore.GetRecord(Entry->Namespace, Entry->Key, Culture);

		// Aggregate provider context into one readable column.
		FEELocContext Context;
		FEELocContextProviderRegistry::Get().BuildContext(*Entry, Context);
		FString ContextText;
		for (const FEELocContextField& Field : Context.Fields)
		{
			ContextText += FString::Printf(TEXT("%s: %s; "), *Field.Label, *Field.Value);
		}

		Csv += FString::Printf(TEXT("%s,%s,%s,%s,%s,%s,%s,%s\n"),
			*CsvEscape(Entry->Namespace),
			*CsvEscape(Entry->Key),
			*CsvEscape(Entry->SourceText),
			*CsvEscape(TranslationText),
			*CsvEscape(StateText),
			*CsvEscape(FEELocReviewStore::ReviewStateToString(Review.State)),
			*CsvEscape(Entry->SourceLocation),
			*CsvEscape(ContextText.TrimEnd()));
	}

	OutFilePath = FPaths::ProjectSavedDir() / TEXT("Localization/ContextPackages") /
		FString::Printf(TEXT("%s_%s.csv"), *Session.GetTargetName(), *Culture);

	if (!FFileHelper::SaveStringToFile(Csv, *OutFilePath, FFileHelper::EEncodingOptions::ForceUTF8))
	{
		OutError = FString::Printf(TEXT("Failed to write %s"), *OutFilePath);
		return false;
	}

	return true;
}

#endif // WITH_EDITOR
