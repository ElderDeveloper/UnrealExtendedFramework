// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "SExtendedAtlassianDocumentEditor.h"

#include "ExtendedAtlassianMarkdown.h"
#include "SExtendedAtlassianDocumentView.h"

#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ExtendedAtlassianDocumentEditor"

namespace ExtendedAtlassianDocumentEditorPrivate
{
	/**
	 * Preview rebuild interval.
	 *
	 * Re-parsing and rebuilding every widget on each keystroke makes typing lag badly on a long
	 * document, so the preview refreshes on a timer instead.
	 */
	constexpr float PreviewRefreshSeconds = 0.35f;
}

void SExtendedAtlassianDocumentEditor::Construct(const FArguments& InArgs)
{
	using namespace ExtendedAtlassianDocumentEditorPrivate;

	OnMarkdownChanged = InArgs._OnMarkdownChanged;

	ChildSlot
	[
		SNew(SVerticalBox)

		// Read-only banner, shown only when saving is blocked.
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.DarkGroupBorder")))
			.Padding(FMargin(8.0f, 6.0f))
			.Visibility_Lambda([this]() { return bReadOnly ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				SNew(STextBlock)
				.Text_Lambda([this]() { return ReadOnlyReason; })
				.ColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.72f, 0.30f)))
				.AutoWrapText(true)
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 4.0f)
		[
			BuildToolbar()
		]

		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SSplitter)
			.Orientation(Orient_Horizontal)

			+ SSplitter::Slot()
			.Value(0.5f)
			[
				SAssignNew(SourceBox, SMultiLineEditableTextBox)
				.AllowMultiLine(true)
				.AutoWrapText(false)
				.IsReadOnly_Lambda([this]() { return bReadOnly; })
				.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Mono"), 9))
				.HintText(LOCTEXT("EditorHint", "Markdown"))
				.OnTextChanged_Lambda([this](const FText& NewText)
				{
					Markdown = NewText.ToString();
					bDirty = true;
					bPreviewStale = true;
				})
			]

			+ SSplitter::Slot()
			.Value(0.5f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
				[
					SAssignNew(Preview, SExtendedAtlassianDocumentView)
				]
			]
		]
	];

	RegisterActiveTimer(
		PreviewRefreshSeconds,
		FWidgetActiveTimerDelegate::CreateSP(this, &SExtendedAtlassianDocumentEditor::HandlePreviewTimer));
}

TSharedRef<SWidget> SExtendedAtlassianDocumentEditor::BuildToolbar()
{
	// Compact labelled buttons rather than icons: no icon artwork ships with the plugin, and a row
	// of unlabelled placeholder glyphs would be worse than text.
	auto MakeButton = [this](const FText& Label, const FText& Tip, TFunction<FReply()> Action)
	{
		return SNew(SButton)
			.Text(Label)
			.ToolTipText(Tip)
			.ContentPadding(FMargin(6.0f, 2.0f))
			.IsEnabled_Lambda([this]() { return !bReadOnly; })
			.OnClicked_Lambda([Action]() { return Action(); });
	};

	TSharedRef<SHorizontalBox> Bar = SNew(SHorizontalBox);

	struct FToolbarEntry
	{
		FText Label;
		FText Tip;
		TFunction<FReply()> Action;
	};

	TArray<FToolbarEntry> Entries;

	Entries.Add({ LOCTEXT("Bold", "B"), LOCTEXT("BoldTip", "Bold"),
		[this]() { return WrapSelection(TEXT("**"), TEXT("**")); } });

	Entries.Add({ LOCTEXT("Italic", "I"), LOCTEXT("ItalicTip", "Italic"),
		[this]() { return WrapSelection(TEXT("*"), TEXT("*")); } });

	Entries.Add({ LOCTEXT("InlineCode", "Code"), LOCTEXT("CodeTip", "Inline code"),
		[this]() { return WrapSelection(TEXT("`"), TEXT("`")); } });

	Entries.Add({ LOCTEXT("H1", "H1"), LOCTEXT("H1Tip", "Heading 1"),
		[this]() { return PrefixLines(TEXT("# ")); } });

	Entries.Add({ LOCTEXT("H2", "H2"), LOCTEXT("H2Tip", "Heading 2"),
		[this]() { return PrefixLines(TEXT("## ")); } });

	Entries.Add({ LOCTEXT("H3", "H3"), LOCTEXT("H3Tip", "Heading 3"),
		[this]() { return PrefixLines(TEXT("### ")); } });

	Entries.Add({ LOCTEXT("Bullet", "List"), LOCTEXT("BulletTip", "Bulleted list"),
		[this]() { return PrefixLines(TEXT("- ")); } });

	Entries.Add({ LOCTEXT("Numbered", "1."), LOCTEXT("NumberedTip", "Numbered list"),
		[this]() { return PrefixLines(TEXT("1. ")); } });

	Entries.Add({ LOCTEXT("Task", "Task"), LOCTEXT("TaskTip", "Task list item"),
		[this]() { return PrefixLines(TEXT("- [ ] ")); } });

	Entries.Add({ LOCTEXT("Quote", "Quote"), LOCTEXT("QuoteTip", "Block quote"),
		[this]() { return PrefixLines(TEXT("> ")); } });

	Entries.Add({ LOCTEXT("Link", "Link"), LOCTEXT("LinkTip", "Insert a link"),
		[this]() { return WrapSelection(TEXT("["), TEXT("](https://)")); } });

	Entries.Add({ LOCTEXT("CodeBlock", "Block"), LOCTEXT("CodeBlockTip", "Fenced code block"),
		[this]() { return InsertBlock(TEXT("\n```\n\n```\n")); } });

	Entries.Add({ LOCTEXT("Table", "Table"), LOCTEXT("TableTip", "Insert a table"),
		[this]() { return InsertBlock(TEXT("\n| Column | Column |\n| --- | --- |\n|  |  |\n")); } });

	Entries.Add({ LOCTEXT("Rule", "---"), LOCTEXT("RuleTip", "Horizontal rule"),
		[this]() { return InsertBlock(TEXT("\n---\n")); } });

	for (const FToolbarEntry& Entry : Entries)
	{
		Bar->AddSlot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 3.0f, 0.0f)
			[
				MakeButton(Entry.Label, Entry.Tip, Entry.Action)
			];
	}

	return Bar;
}

void SExtendedAtlassianDocumentEditor::SetMarkdown(const FString& InMarkdown)
{
	Markdown = InMarkdown;
	bDirty = false;
	bPreviewStale = false;

	if (SourceBox.IsValid())
	{
		SourceBox->SetText(FText::FromString(Markdown));
	}

	if (Preview.IsValid())
	{
		Preview->SetBlocks(FExtendedAtlassianMarkdown::ToBlocks(Markdown));
	}
}

void SExtendedAtlassianDocumentEditor::SetReadOnly(bool bInReadOnly, const FText& Reason)
{
	bReadOnly = bInReadOnly;
	ReadOnlyReason = Reason;
}

FReply SExtendedAtlassianDocumentEditor::WrapSelection(FString Prefix, FString Suffix)
{
	if (!SourceBox.IsValid() || bReadOnly)
	{
		return FReply::Handled();
	}

	const FString Selected = SourceBox->GetSelectedText().ToString();
	SourceBox->InsertTextAtCursor(FText::FromString(Prefix + Selected + Suffix));

	return FReply::Handled();
}

FReply SExtendedAtlassianDocumentEditor::PrefixLines(FString Prefix)
{
	if (!SourceBox.IsValid() || bReadOnly)
	{
		return FReply::Handled();
	}

	const FString Selected = SourceBox->GetSelectedText().ToString();

	if (Selected.IsEmpty())
	{
		SourceBox->InsertTextAtCursor(FText::FromString(Prefix));
		return FReply::Handled();
	}

	TArray<FString> Lines;
	Selected.ParseIntoArray(Lines, TEXT("\n"), false);

	for (FString& Line : Lines)
	{
		Line = Prefix + Line;
	}

	SourceBox->InsertTextAtCursor(FText::FromString(FString::Join(Lines, TEXT("\n"))));
	return FReply::Handled();
}

FReply SExtendedAtlassianDocumentEditor::InsertBlock(FString Text)
{
	if (!SourceBox.IsValid() || bReadOnly)
	{
		return FReply::Handled();
	}

	SourceBox->InsertTextAtCursor(FText::FromString(Text));
	return FReply::Handled();
}

EActiveTimerReturnType SExtendedAtlassianDocumentEditor::HandlePreviewTimer(double InCurrentTime, float InDeltaTime)
{
	if (bPreviewStale)
	{
		bPreviewStale = false;

		if (Preview.IsValid())
		{
			Preview->SetBlocks(FExtendedAtlassianMarkdown::ToBlocks(Markdown));
		}

		OnMarkdownChanged.ExecuteIfBound(Markdown);
	}

	return EActiveTimerReturnType::Continue;
}

#undef LOCTEXT_NAMESPACE
