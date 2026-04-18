// Copyright Kemal Erdem YILMAZ. All Rights Reserved.
// Adapted from Epic Games SDataTableListViewRow — Slate row widget preserved,
// data model replaced with FESQLEditorRow.

#pragma once

#include "CoreMinimal.h"
#include "FESQLTableEditorToolkit.h"
#include "DragAndDrop/DecoratedDragDropOp.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

class FESQLTableEditorToolkit;
class SESQLTableListViewRow;
class SInlineEditableTextBlock;
class STableViewBase;
class SWidget;
struct FGeometry;
struct FKeyEvent;
struct FPointerEvent;
struct FSlateBrush;


// ── Drag handle widget ──────────────────────────────────────────────────────

class SESQLTableRowHandle : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SESQLTableRowHandle) {}
		SLATE_DEFAULT_SLOT(FArguments, Content)
		SLATE_ARGUMENT(TSharedPtr<SESQLTableListViewRow>, ParentRow)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		return FReply::Handled().DetectDrag(SharedThis(this), EKeys::LeftMouseButton);
	}

	FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	TSharedPtr<class FESQLRowDragDropOp> CreateDragDropOperation(TSharedPtr<SESQLTableListViewRow> InRow);

private:
	TWeakPtr<SESQLTableListViewRow> ParentRow;
};


// ── List view row widget ────────────────────────────────────────────────────

/**
 * Represents one row in the SQL Table Editor grid.
 * Adapted from SDataTableListViewRow — cell rendering, right-click context
 * menu, drag-drop, and inline renaming are preserved.
 * Data source is FESQLEditorRow instead of UDataTable row map.
 */
class SESQLTableListViewRow : public SMultiColumnTableRow<FESQLEditorRowPtr>
{
public:

	SLATE_BEGIN_ARGS(SESQLTableListViewRow)
		: _IsEditable(true)
	{}
		SLATE_ARGUMENT(TSharedPtr<FESQLTableEditorToolkit>, SQLTableEditor)
		SLATE_ARGUMENT(FESQLEditorRowPtr, RowDataPtr)
		SLATE_ARGUMENT(bool, IsEditable)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView);

	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	void OnRowRenamed(const FText& Text, ETextCommit::Type CommitType);
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;

	FText GetCurrentNameAsText() const;
	FName GetCurrentName() const;
	uint32 GetCurrentIndex() const;
	const TArray<FText>& GetCellValues() const;

	virtual FReply OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent);

	void SetRowForRename();
	void SetIsDragDrop(bool bInIsDragDrop);

	const FESQLEditorRowPtr& GetRowDataPtr() const { return RowDataPtr; }

private:

	void OnSearchForReferences();
	FReply OnRowDrop(const FDragDropEvent& DragDropEvent);

	TSharedRef<SWidget> MakeCellWidget(const int32 InRowIndex, const FName& InColumnId);

	void OnRowDragEnter(const FDragDropEvent& DragDropEvent);
	void OnRowDragLeave(const FDragDropEvent& DragDropEvent);

	virtual const FSlateBrush* GetBorder() const;

	TSharedRef<SWidget> MakeRowActionsMenu();

	TSharedPtr<SInlineEditableTextBlock> InlineEditableText;
	TSharedPtr<FName> CurrentName;

	FESQLEditorRowPtr RowDataPtr;
	TWeakPtr<FESQLTableEditorToolkit> SQLTableEditor;
	TArray<FText> CellValues;

	bool IsEditable = true;
	bool bIsDragDropObject = false;
	bool bIsHoveredDragTarget = false;
};


// ── Drag-drop operation ─────────────────────────────────────────────────────

class FESQLRowDragDropOp : public FDecoratedDragDropOp
{
public:
	DRAG_DROP_OPERATOR_TYPE(FESQLRowDragDropOp, FDecoratedDragDropOp)

	FESQLRowDragDropOp(TSharedPtr<SESQLTableListViewRow> InRow);
	void OnDrop(bool bDropWasHandled, const FPointerEvent& MouseEvent);

	TSharedPtr<SWidget> DecoratorWidget;

	virtual TSharedPtr<SWidget> GetDefaultDecorator() const override
	{
		return DecoratorWidget;
	}

	TWeakPtr<SESQLTableListViewRow> Row;
};
