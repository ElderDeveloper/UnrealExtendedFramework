// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

class FEELocalizationSession;
struct FEELocEntry;

enum class EEELocIssueSeverity : uint8
{
	Warning,
	Error
};

/** One validation finding. */
struct FEELocIssue
{
	TSharedPtr<FEELocEntry> Entry;
	/** Culture the issue applies to (empty = source/identity issue). */
	FString Culture;
	FName RuleId;
	EEELocIssueSeverity Severity = EEELocIssueSeverity::Warning;
	FString Message;
};

/** A validation rule; runs per entry/culture or session-wide. */
class IEELocValidationRule
{
public:
	virtual ~IEELocValidationRule() = default;
	virtual FName GetRuleId() const = 0;

	/** Per-translation validation (called for every entry × foreign culture). */
	virtual void ValidateTranslation(const TSharedPtr<FEELocEntry>& Entry, const FString& Culture,
		const FString& SourceText, const FString& TranslationText, TArray<FEELocIssue>& OutIssues) const {}

	/** Session-wide validation (identity conflicts, duplicates, ...). */
	virtual void ValidateSession(const FEELocalizationSession& Session, TArray<FEELocIssue>& OutIssues) const {}
};

/**
 * Integration hook (plan section 7): UI Lab can register UI-aware validation (clipping,
 * expansion, glyph coverage in real widgets). The workbench functions fully without it.
 */
class IEELocalizationUIValidationHook
{
public:
	virtual ~IEELocalizationUIValidationHook() = default;
	virtual void ValidateEntry(const TSharedPtr<FEELocEntry>& Entry, const FString& Culture, TArray<FEELocIssue>& OutIssues) = 0;
};

/**
 * LW-4 validation engine: runs registered rules over an open localization session.
 * Built-in rules cover translation integrity (missing/stale/identical, placeholder and
 * rich-text-tag mismatches, whitespace and line-break policy) and identity conflicts.
 * Additional rules and the optional UI hook register through this class.
 */
class UNREALEXTENDEDFRAMEWORKEDITOR_API FEELocValidator
{
public:
	static FEELocValidator& Get();

	void RegisterRule(const TSharedRef<IEELocValidationRule>& Rule);
	void UnregisterRule(const TSharedRef<IEELocValidationRule>& Rule);

	void SetUIValidationHook(const TSharedPtr<IEELocalizationUIValidationHook>& Hook) { UIHook = Hook; }
	TSharedPtr<IEELocalizationUIValidationHook> GetUIValidationHook() const { return UIHook; }

	/** Runs every rule over the whole session. */
	void ValidateAll(const FEELocalizationSession& Session, TArray<FEELocIssue>& OutIssues) const;

private:
	FEELocValidator();

	TArray<TSharedRef<IEELocValidationRule>> Rules;
	TSharedPtr<IEELocalizationUIValidationHook> UIHook;
};

#endif // WITH_EDITOR
