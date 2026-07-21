// Copyright Moon Punch Games. All Rights Reserved.

#include "EELocalizationValidator.h"

#if WITH_EDITOR

#include "Localization/Model/EELocalizationSession.h"

namespace
{
	/** Collects "{Name}" format arguments and printf-style "%x" tokens. */
	void ExtractPlaceholders(const FString& Text, TArray<FString>& OutPlaceholders)
	{
		// {Named} tokens (FText::Format).
		int32 Index = 0;
		while (true)
		{
			const int32 Open = Text.Find(TEXT("{"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Index);
			if (Open == INDEX_NONE)
			{
				break;
			}
			const int32 Close = Text.Find(TEXT("}"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Open + 1);
			if (Close == INDEX_NONE)
			{
				break;
			}
			// "{{" escapes a literal brace in FText::Format.
			if (Close > Open + 1 && !Text.Mid(Open + 1, 1).Equals(TEXT("{")))
			{
				OutPlaceholders.Add(Text.Mid(Open, Close - Open + 1));
			}
			Index = Close + 1;
		}

		// printf-style tokens.
		for (int32 i = 0; i < Text.Len() - 1; ++i)
		{
			if (Text[i] == TEXT('%'))
			{
				const TCHAR Next = Text[i + 1];
				if (FChar::IsAlpha(Next))
				{
					OutPlaceholders.Add(FString::Printf(TEXT("%%%c"), Next));
					++i;
				}
			}
		}

		OutPlaceholders.Sort();
	}

	class FEEPlaceholderRule final : public IEELocValidationRule
	{
	public:
		virtual FName GetRuleId() const override { return TEXT("Placeholders"); }

		virtual void ValidateTranslation(const TSharedPtr<FEELocEntry>& Entry, const FString& Culture,
			const FString& SourceText, const FString& TranslationText, TArray<FEELocIssue>& OutIssues) const override
		{
			if (TranslationText.IsEmpty())
			{
				return;
			}

			TArray<FString> SourcePlaceholders, TranslationPlaceholders;
			ExtractPlaceholders(SourceText, SourcePlaceholders);
			ExtractPlaceholders(TranslationText, TranslationPlaceholders);

			if (SourcePlaceholders != TranslationPlaceholders)
			{
				OutIssues.Add({Entry, Culture, GetRuleId(), EEELocIssueSeverity::Error,
					FString::Printf(TEXT("Placeholder mismatch: source has [%s], translation has [%s]."),
						*FString::Join(SourcePlaceholders, TEXT(" ")),
						*FString::Join(TranslationPlaceholders, TEXT(" ")))});
			}
		}
	};

	class FEERichTextTagRule final : public IEELocValidationRule
	{
	public:
		virtual FName GetRuleId() const override { return TEXT("RichTextTags"); }

		static void CountTags(const FString& Text, int32& OutOpen, int32& OutClose)
		{
			OutOpen = 0;
			OutClose = 0;
			for (int32 i = 0; i < Text.Len(); ++i)
			{
				if (Text[i] != TEXT('<'))
				{
					continue;
				}
				const int32 End = Text.Find(TEXT(">"), ESearchCase::CaseSensitive, ESearchDir::FromStart, i + 1);
				if (End == INDEX_NONE)
				{
					break;
				}
				const FString Tag = Text.Mid(i + 1, End - i - 1);
				if (Tag == TEXT("/"))
				{
					++OutClose;
				}
				else if (!Tag.IsEmpty() && !Tag.EndsWith(TEXT("/")))
				{
					// "<style>" or "<img id=\"x\">" opens; "<br/>"-style self-closing tags count neither.
					++OutOpen;
				}
				i = End;
			}
		}

		virtual void ValidateTranslation(const TSharedPtr<FEELocEntry>& Entry, const FString& Culture,
			const FString& SourceText, const FString& TranslationText, TArray<FEELocIssue>& OutIssues) const override
		{
			if (TranslationText.IsEmpty())
			{
				return;
			}

			int32 SourceOpen, SourceClose, TranslationOpen, TranslationClose;
			CountTags(SourceText, SourceOpen, SourceClose);
			CountTags(TranslationText, TranslationOpen, TranslationClose);

			if (TranslationOpen != TranslationClose)
			{
				OutIssues.Add({Entry, Culture, GetRuleId(), EEELocIssueSeverity::Error,
					FString::Printf(TEXT("Unbalanced rich-text tags (%d open, %d close)."), TranslationOpen, TranslationClose)});
			}
			else if (SourceOpen != TranslationOpen)
			{
				OutIssues.Add({Entry, Culture, GetRuleId(), EEELocIssueSeverity::Warning,
					FString::Printf(TEXT("Rich-text tag count differs from source (%d vs %d)."), TranslationOpen, SourceOpen)});
			}
		}
	};

	class FEEWhitespaceRule final : public IEELocValidationRule
	{
	public:
		virtual FName GetRuleId() const override { return TEXT("Whitespace"); }

		virtual void ValidateTranslation(const TSharedPtr<FEELocEntry>& Entry, const FString& Culture,
			const FString& SourceText, const FString& TranslationText, TArray<FEELocIssue>& OutIssues) const override
		{
			if (TranslationText.IsEmpty())
			{
				return;
			}

			auto HasLeading = [](const FString& Text) { return Text.Len() > 0 && FChar::IsWhitespace(Text[0]); };
			auto HasTrailing = [](const FString& Text) { return Text.Len() > 0 && FChar::IsWhitespace(Text[Text.Len() - 1]); };

			if (HasLeading(TranslationText) != HasLeading(SourceText) || HasTrailing(TranslationText) != HasTrailing(SourceText))
			{
				OutIssues.Add({Entry, Culture, GetRuleId(), EEELocIssueSeverity::Warning,
					TEXT("Leading/trailing whitespace differs from source.")});
			}

			int32 SourceBreaks = 0, TranslationBreaks = 0;
			for (const TCHAR Char : SourceText) { if (Char == TEXT('\n')) { ++SourceBreaks; } }
			for (const TCHAR Char : TranslationText) { if (Char == TEXT('\n')) { ++TranslationBreaks; } }
			if (SourceBreaks != TranslationBreaks)
			{
				OutIssues.Add({Entry, Culture, GetRuleId(), EEELocIssueSeverity::Warning,
					FString::Printf(TEXT("Line-break count differs from source (%d vs %d)."), TranslationBreaks, SourceBreaks)});
			}
		}
	};

	class FEEStateRule final : public IEELocValidationRule
	{
	public:
		virtual FName GetRuleId() const override { return TEXT("TranslationState"); }

		virtual void ValidateSession(const FEELocalizationSession& Session, TArray<FEELocIssue>& OutIssues) const override
		{
			for (const TSharedPtr<FEELocEntry>& Entry : Session.GetEntries())
			{
				for (const TPair<FString, FEELocTranslation>& Pair : Entry->Translations)
				{
					switch (Pair.Value.State)
					{
					case EEELocEntryState::Missing:
						OutIssues.Add({Entry, Pair.Key, GetRuleId(), EEELocIssueSeverity::Warning, TEXT("Missing translation.")});
						break;
					case EEELocEntryState::Stale:
						OutIssues.Add({Entry, Pair.Key, GetRuleId(), EEELocIssueSeverity::Warning, TEXT("Stale: source changed since this was translated.")});
						break;
					case EEELocEntryState::Identical:
						OutIssues.Add({Entry, Pair.Key, GetRuleId(), EEELocIssueSeverity::Warning, TEXT("Translation is identical to the source text.")});
						break;
					default:
						break;
					}
				}
			}
		}
	};

	class FEEIdentityRule final : public IEELocValidationRule
	{
	public:
		virtual FName GetRuleId() const override { return TEXT("Identity"); }

		virtual void ValidateSession(const FEELocalizationSession& Session, TArray<FEELocIssue>& OutIssues) const override
		{
			TMap<FString, TSharedPtr<FEELocEntry>> SeenIdentities;
			for (const TSharedPtr<FEELocEntry>& Entry : Session.GetEntries())
			{
				if (Entry->Key.TrimStartAndEnd().IsEmpty())
				{
					OutIssues.Add({Entry, FString(), GetRuleId(), EEELocIssueSeverity::Error, TEXT("Empty key.")});
				}

				const FString Identity = Entry->Namespace + TEXT(",") + Entry->Key;
				if (const TSharedPtr<FEELocEntry>* Existing = SeenIdentities.Find(Identity))
				{
					if ((*Existing)->SourceText != Entry->SourceText)
					{
						OutIssues.Add({Entry, FString(), GetRuleId(), EEELocIssueSeverity::Error,
							TEXT("Identity conflict: same namespace/key with different source text.")});
					}
				}
				else
				{
					SeenIdentities.Add(Identity, Entry);
				}
			}
		}
	};
}

FEELocValidator::FEELocValidator()
{
	Rules.Add(MakeShared<FEEStateRule>());
	Rules.Add(MakeShared<FEEIdentityRule>());
	Rules.Add(MakeShared<FEEPlaceholderRule>());
	Rules.Add(MakeShared<FEERichTextTagRule>());
	Rules.Add(MakeShared<FEEWhitespaceRule>());
}

FEELocValidator& FEELocValidator::Get()
{
	static FEELocValidator Instance;
	return Instance;
}

void FEELocValidator::RegisterRule(const TSharedRef<IEELocValidationRule>& Rule)
{
	Rules.AddUnique(Rule);
}

void FEELocValidator::UnregisterRule(const TSharedRef<IEELocValidationRule>& Rule)
{
	Rules.Remove(Rule);
}

void FEELocValidator::ValidateAll(const FEELocalizationSession& Session, TArray<FEELocIssue>& OutIssues) const
{
	for (const TSharedRef<IEELocValidationRule>& Rule : Rules)
	{
		Rule->ValidateSession(Session, OutIssues);
	}

	for (const TSharedPtr<FEELocEntry>& Entry : Session.GetEntries())
	{
		for (const TPair<FString, FEELocTranslation>& Pair : Entry->Translations)
		{
			for (const TSharedRef<IEELocValidationRule>& Rule : Rules)
			{
				Rule->ValidateTranslation(Entry, Pair.Key, Entry->SourceText, Pair.Value.Text, OutIssues);
			}

			if (UIHook.IsValid())
			{
				UIHook->ValidateEntry(Entry, Pair.Key, OutIssues);
			}
		}
	}
}

#endif // WITH_EDITOR
