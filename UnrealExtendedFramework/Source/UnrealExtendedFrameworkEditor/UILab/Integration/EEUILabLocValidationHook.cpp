// Copyright Moon Punch Games. All Rights Reserved.

#include "EEUILabLocValidationHook.h"

#if WITH_EDITOR

#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Localization/Model/EELocalizationSession.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"

void FEEUILabLocValidationHook::ValidateEntry(const TSharedPtr<FEELocEntry>& Entry, const FString& Culture, TArray<FEELocIssue>& OutIssues)
{
	if (!Entry.IsValid() || !FSlateApplication::IsInitialized())
	{
		return;
	}

	const FEELocTranslation* Translation = Entry->Translations.Find(Culture);
	if (!Translation || Translation->Text.IsEmpty() || Entry->SourceText.IsEmpty())
	{
		return;
	}

	const TSharedRef<FSlateFontMeasure> FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", 10);

	const float SourceWidth = FontMeasure->Measure(Entry->SourceText, Font).X;
	const float TranslationWidth = FontMeasure->Measure(Translation->Text, Font).X;

	// Unsafe expansion: rendered width grew well past the source's — the classic clipping risk.
	if (SourceWidth > 1.0f && TranslationWidth > SourceWidth * 1.6f && TranslationWidth - SourceWidth > 40.0f)
	{
		OutIssues.Add({Entry, Culture, TEXT("UIExpansion"), EEELocIssueSeverity::Warning,
			FString::Printf(TEXT("Unsafe expansion: renders at %.0fpx vs source %.0fpx (%.0f%%). Check layouts that fit the source text."),
				TranslationWidth, SourceWidth, TranslationWidth / SourceWidth * 100.0f)});
	}
}

#endif // WITH_EDITOR
