// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

#include "Localization/Validation/EELocalizationValidator.h"

/**
 * LW-5 glossary rule: for every glossary asset in the project, checks that source terms keep
 * their approved (or untranslated) form in translations and that forbidden alternatives never
 * appear. Registered by the Localization Workbench feature at startup.
 */
class FEEGlossaryValidationRule final : public IEELocValidationRule
{
public:
	virtual FName GetRuleId() const override { return TEXT("Glossary"); }

	virtual void ValidateTranslation(const TSharedPtr<FEELocEntry>& Entry, const FString& Culture,
		const FString& SourceText, const FString& TranslationText, TArray<FEELocIssue>& OutIssues) const override;
};

#endif // WITH_EDITOR
