// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SExtendedAtlassianDocumentView;
class SMultiLineEditableTextBox;

/**
 * Split Markdown editor: editable source on one side, live rendered preview on the other.
 *
 * Not WYSIWYG by choice. The file in Saved/Documents is the shared source of truth for people, this
 * plugin and any LLM reading the repo, so it has to stay clean idiomatic Markdown. A WYSIWYG editor
 * operates on a text layout and serialises back whatever its marshaller emits, which would churn
 * the file on every round trip and turn diffs into noise. Toolbar buttons wrap the selection so
 * syntax rarely has to be typed by hand.
 */
class SExtendedAtlassianDocumentEditor : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_OneParam(FOnMarkdownChanged, const FString& /*Markdown*/);

	SLATE_BEGIN_ARGS(SExtendedAtlassianDocumentEditor) {}
		/** Fires after the debounce, not on every keystroke. */
		SLATE_EVENT(FOnMarkdownChanged, OnMarkdownChanged)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Replaces the document. Clears the dirty flag: this is a load, not an edit. */
	void SetMarkdown(const FString& InMarkdown);

	FString GetMarkdown() const { return Markdown; }
	bool IsDirty() const { return bDirty; }
	void ClearDirty() { bDirty = false; }

	/**
	 * Blocks editing, with a reason shown in a banner.
	 *
	 * Used for pages containing Confluence constructs this plugin cannot rebuild, where saving
	 * would silently delete them.
	 */
	void SetReadOnly(bool bInReadOnly, const FText& Reason);

private:
	TSharedRef<SWidget> BuildToolbar();

	/** Wraps the selection, or inserts the delimiters at the caret when nothing is selected. */
	FReply WrapSelection(FString Prefix, FString Suffix);

	/** Prefixes every selected line, for headings, list items and quotes. */
	FReply PrefixLines(FString Prefix);

	FReply InsertBlock(FString Text);

	EActiveTimerReturnType HandlePreviewTimer(double InCurrentTime, float InDeltaTime);

	TSharedPtr<SMultiLineEditableTextBox> SourceBox;
	TSharedPtr<SExtendedAtlassianDocumentView> Preview;

	FString Markdown;
	bool bDirty = false;
	bool bReadOnly = false;
	FText ReadOnlyReason;

	/** Set on every keystroke; the timer rebuilds the preview at most a few times a second. */
	bool bPreviewStale = false;

	FOnMarkdownChanged OnMarkdownChanged;
};
