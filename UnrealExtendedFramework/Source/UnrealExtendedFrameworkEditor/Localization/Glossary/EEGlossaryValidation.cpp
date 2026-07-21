// Copyright Moon Punch Games. All Rights Reserved.

#include "EEGlossaryValidation.h"

#if WITH_EDITOR

#include "AssetRegistry/AssetRegistryModule.h"
#include "Localization/Glossary/EFLocalizationGlossary.h"
#include "Localization/Model/EELocalizationSession.h"

namespace
{
	void CollectGlossaries(TArray<const UEFLocalizationGlossary*>& OutGlossaries)
	{
		const FAssetRegistryModule& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

		FARFilter Filter;
		Filter.ClassPaths.Add(UEFLocalizationGlossary::StaticClass()->GetClassPathName());
		Filter.bRecursiveClasses = true;
		Filter.bRecursivePaths = true;

		TArray<FAssetData> Assets;
		AssetRegistry.Get().GetAssets(Filter, Assets);

		for (const FAssetData& Asset : Assets)
		{
			if (const UEFLocalizationGlossary* Glossary = Cast<UEFLocalizationGlossary>(Asset.GetAsset()))
			{
				OutGlossaries.Add(Glossary);
			}
		}
	}
}

void FEEGlossaryValidationRule::ValidateTranslation(const TSharedPtr<FEELocEntry>& Entry, const FString& Culture,
	const FString& SourceText, const FString& TranslationText, TArray<FEELocIssue>& OutIssues) const
{
	if (TranslationText.IsEmpty())
	{
		return;
	}

	TArray<const UEFLocalizationGlossary*> Glossaries;
	CollectGlossaries(Glossaries);

	for (const UEFLocalizationGlossary* Glossary : Glossaries)
	{
		for (const FEFGlossaryTerm& Term : Glossary->Terms)
		{
			if (Term.Term.IsEmpty() || !SourceText.Contains(Term.Term))
			{
				continue;
			}

			// Forbidden alternatives are an error in any culture.
			for (const FString& Forbidden : Term.ForbiddenAlternatives)
			{
				if (!Forbidden.IsEmpty() && TranslationText.Contains(Forbidden))
				{
					OutIssues.Add({Entry, Culture, GetRuleId(), EEELocIssueSeverity::Error,
						FString::Printf(TEXT("Forbidden term '%s' used (glossary term '%s')."), *Forbidden, *Term.Term)});
				}
			}

			if (Term.bDoNotTranslate)
			{
				if (!TranslationText.Contains(Term.Term))
				{
					OutIssues.Add({Entry, Culture, GetRuleId(), EEELocIssueSeverity::Warning,
						FString::Printf(TEXT("Do-not-translate term '%s' is missing verbatim."), *Term.Term)});
				}
			}
			else if (const FString* Approved = Term.ApprovedTranslations.Find(Culture))
			{
				if (!Approved->IsEmpty() && !TranslationText.Contains(*Approved))
				{
					OutIssues.Add({Entry, Culture, GetRuleId(), EEELocIssueSeverity::Warning,
						FString::Printf(TEXT("Glossary term '%s' should be translated as '%s'."), *Term.Term, **Approved)});
				}
			}
		}
	}
}

#endif // WITH_EDITOR
