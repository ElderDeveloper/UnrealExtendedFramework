// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/CurveSequence.h"
#include "ExtendedAtlassianDocBlock.h"
#include "Widgets/SCompoundWidget.h"

class SVerticalBox;
class SEditableTextBox;
class SBox;

/**
 * Block-native Backlot document editor.
 *
 * Markdown remains the storage/working-copy boundary, but the editor exposes the authored block
 * model rather than a source/preview split that does not exist in the HTML reference.
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
	virtual void Tick(
		const FGeometry& AllottedGeometry,
		double InCurrentTime,
		float InDeltaTime) override;

	/** Replaces the document. Clears the dirty flag: this is a load, not an edit. */
	void SetMarkdown(const FString& InMarkdown);

	FString GetMarkdown() const { return Markdown; }
	const TArray<FExtendedAtlassianDocBlock>& GetBlocks() const { return Blocks; }
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
	void RebuildBlocks();
	TSharedRef<SWidget> BuildBlockEditor(int32 BlockIndex);
	TSharedRef<SWidget> BuildBlockPalette();
	TSharedRef<SWidget> BuildInsertMenu(int32 AfterBlockIndex);
	void AddBlock(EExtendedAtlassianBlockKind Kind);
	void InsertBlock(EExtendedAtlassianBlockKind Kind, int32 AfterBlockIndex);
	void MoveBlock(int32 BlockIndex, int32 Direction);
	void ToggleInsertMenu(int32 BlockIndex);
	void RemoveBlock(int32 BlockIndex);
	void NormalizeBlocks();
	void CommitBlocks();
	void SetMarkupText(const FText& Text, int32 BlockIndex);
	void SetRawText(const FText& Text, int32 BlockIndex);
	void SetCodeLanguage(const FText& Text, int32 BlockIndex);
	void SetCodeFile(const FText& Text, int32 BlockIndex);
	void SetTableCell(const FText& Text, int32 BlockIndex, int32 CellIndex);
	void ToggleTask(int32 BlockIndex);
	void OnSlashChanged(const FText& Text);
	FReply OnSlashKeyDown(
		const FGeometry& Geometry,
		const FKeyEvent& KeyEvent);
	bool SlashMatches(EExtendedAtlassianBlockKind Kind) const;

	EActiveTimerReturnType HandlePreviewTimer(double InCurrentTime, float InDeltaTime);

	TSharedPtr<SVerticalBox> ContentBox;
	TSharedPtr<SEditableTextBox> SlashInput;
	TSharedPtr<SBox> SlashMenuBox;
	FCurveSequence SlashMenuSequence;
	FCurveHandle SlashMenuCurve;

	FString Markdown;
	FString SlashText;
	TArray<FExtendedAtlassianDocBlock> Blocks;
	int32 InsertMenuBlockIndex = INDEX_NONE;
	bool bDirty = false;
	bool bReadOnly = false;
	FText ReadOnlyReason;

	/** Coalesces model notifications while a user types in a block. */
	bool bChangeNotificationPending = false;

	FOnMarkdownChanged OnMarkdownChanged;
};
