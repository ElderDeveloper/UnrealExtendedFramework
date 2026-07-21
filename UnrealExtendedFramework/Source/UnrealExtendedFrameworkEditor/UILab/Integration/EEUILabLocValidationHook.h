// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

#include "Localization/Validation/EELocalizationValidator.h"

/**
 * Integration hook (plan section 7): UI Lab fulfils the Localization Workbench's optional
 * UI-aware validation slot. Translations are measured with the real Slate font measure
 * service and flagged when their rendered width expands past safe layout bounds.
 * The workbench functions identically when this hook is not registered.
 */
class FEEUILabLocValidationHook final : public IEELocalizationUIValidationHook
{
public:
	virtual void ValidateEntry(const TSharedPtr<FEELocEntry>& Entry, const FString& Culture, TArray<FEELocIssue>& OutIssues) override;
};

#endif // WITH_EDITOR
