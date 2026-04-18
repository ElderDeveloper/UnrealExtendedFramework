// Copyright Kemal Erdem YILMAZ. All Rights Reserved.
// Adapted from Epic Games SDataTableListViewRow — cell rendering preserved,
// data model replaced with FESQLEditorRow.

#include "SESQLTableListViewRow.h"
#include "FESQLTableEditorToolkit.h"
#include "TableAsset/ESQLTableAsset.h"
#include "Core/ESQLDatabase.h"
#include "UnrealExtendedSQLEditor.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ScopedTransaction.h"
#include "Styling/AppStyle.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"


#define LOCTEXT_NAMESPACE "SESQLTableListViewRow"


// ── SESQLTableRowHandle ─────────────────────────────────────────────────────

void SESQLTableRowHandle::Construct(const FArguments& InArgs)
{
	ParentRow = InArgs._ParentRow;

	ChildSlot
	[
		InArgs._Content.Widget
	];
}

FReply SESQLTableRowHandle::OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (ParentRow.IsValid())
	{
		TSharedPtr<FESQLRowDragDropOp> DragOp = CreateDragDropOperation(ParentRow.Pin());
		if (DragOp.IsValid())
		{
			return FReply::Handled().BeginDragDrop(DragOp.ToSharedRef());
		}
	}
	return FReply::Unhandled();
}

TSharedPtr<FESQLRowDragDropOp> SESQLTableRowHandle::CreateDragDropOperation(TSharedPtr<SESQLTableListViewRow> InRow)
{
	TSharedPtr<FESQLRowDragDropOp> Op = MakeShareable(new FESQLRowDragDropOp(InRow));
	return Op;
}


// ── SESQLTableListViewRow ───────────────────────────────────────────────────

void SESQLTableListViewRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
{
	RowDataPtr = InArgs._RowDataPtr;
	SQLTableEditor = InArgs._SQLTableEditor;
	IsEditable = InArgs._IsEditable;
	bIsDragDropObject = false;
	bIsHoveredDragTarget = false;

	// Cache current name
	if (RowDataPtr.IsValid())
	{
		CurrentName = MakeShareable(new FName(RowDataPtr->RowId));

		// Cache cell values for GetCellValues()
		CellValues.Reset();
		if (SQLTableEditor.IsValid())
		{
			TSharedPtr<FESQLTableEditorToolkit> Editor = SQLTableEditor.Pin();
			// Already have CellData in the row
			for (const auto& Pair : RowDataPtr->CellData)
			{
				CellValues.Add(FText::FromString(Pair.Value));
			}
		}
	}

	SMultiColumnTableRow<FESQLEditorRowPtr>::Construct(
		FSuperRowType::FArguments()
			.Style(FAppStyle::Get(), "DataTableEditor.CellListViewRow"),
		InOwnerTableView
	);
}

TSharedRef<SWidget> SESQLTableListViewRow::GenerateWidgetForColumn(const FName& ColumnName)
{
	if (!RowDataPtr.IsValid())
	{
		return SNullWidget::NullWidget;
	}

	// Row number column
	if (ColumnName == FESQLTableEditorToolkit::RowNumberColumnId)
	{
		return SNew(SBox)
			.Padding(FMargin(4, 2, 4, 2))
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::AsNumber(RowDataPtr->RowNum))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			];
	}

	// Row name (PK) column — editable
	if (ColumnName == FESQLTableEditorToolkit::RowNameColumnId)
	{
		return SNew(SBox)
			.Padding(FMargin(4, 2, 4, 2))
			[
				SAssignNew(InlineEditableText, SInlineEditableTextBlock)
				.Text(this, &SESQLTableListViewRow::GetCurrentNameAsText)
				.OnTextCommitted(this, &SESQLTableListViewRow::OnRowRenamed)
				.IsSelected(this, &SESQLTableListViewRow::IsSelectedExclusively)
				.ColorAndOpacity_Lambda([this]() -> FSlateColor
				{
					return SQLTableEditor.IsValid()
						? SQLTableEditor.Pin()->GetRowTextColor(GetCurrentName())
						: FSlateColor::UseForeground();
				})
			];
	}

	// Data cell column
	return MakeCellWidget(RowDataPtr->RowNum - 1, ColumnName);
}

TSharedRef<SWidget> SESQLTableListViewRow::MakeCellWidget(const int32 InRowIndex, const FName& InColumnId)
{
	if (!SQLTableEditor.IsValid())
	{
		return SNullWidget::NullWidget;
	}

	return SQLTableEditor.Pin()->MakeCellWidget(RowDataPtr, InRowIndex, InColumnId);
}


// ── Row actions ─────────────────────────────────────────────────────────────

FReply SESQLTableListViewRow::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (IsEditable && MouseEvent.GetEffectingButton() == EKeys::RightMouseButton && RowDataPtr.IsValid())
	{
		TSharedRef<SWidget> MenuWidget = MakeRowActionsMenu();

		FWidgetPath WidgetPath = MouseEvent.GetEventPath() ? *MouseEvent.GetEventPath() : FWidgetPath();
		FSlateApplication::Get().PushMenu(
			SharedThis(this),
			WidgetPath,
			MenuWidget,
			MouseEvent.GetScreenSpacePosition(),
			FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu)
		);
		return FReply::Handled();
	}

	return SMultiColumnTableRow<FESQLEditorRowPtr>::OnMouseButtonUp(MyGeometry, MouseEvent);
}

TSharedRef<SWidget> SESQLTableListViewRow::MakeRowActionsMenu()
{
	FMenuBuilder MenuBuilder(true, nullptr);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("CopyRow", "Copy Row"),
		LOCTEXT("CopyRowTooltip", "Copy the selected row to the clipboard"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this]()
		{
			if (SQLTableEditor.IsValid())
			{
				SQLTableEditor.Pin()->CopySelectedRow();
			}
		}))
	);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("DuplicateRow", "Duplicate Row"),
		LOCTEXT("DuplicateRowTooltip", "Create a copy of the selected row"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this]()
		{
			if (SQLTableEditor.IsValid())
			{
				SQLTableEditor.Pin()->DuplicateSelectedRow();
			}
		}))
	);

	MenuBuilder.AddMenuSeparator();

	MenuBuilder.AddMenuEntry(
		LOCTEXT("DeleteRow", "Delete Row"),
		LOCTEXT("DeleteRowTooltip", "Delete the selected row from the table"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this]()
		{
			if (SQLTableEditor.IsValid())
			{
				SQLTableEditor.Pin()->DeleteSelectedRow();
			}
		}))
	);

	MenuBuilder.AddMenuSeparator();

	MenuBuilder.AddMenuEntry(
		LOCTEXT("RenameRow", "Rename Row"),
		LOCTEXT("RenameRowTooltip", "Rename this row's primary key"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this]()
		{
			SetRowForRename();
		}))
	);

	return MenuBuilder.MakeWidget();
}

void SESQLTableListViewRow::OnRowRenamed(const FText& Text, ETextCommit::Type CommitType)
{
	if (!RowDataPtr.IsValid() || !SQLTableEditor.IsValid()) return;
	if (CommitType == ETextCommit::OnCleared) return;

	const FString OldName = RowDataPtr->RowId.ToString();
	const FString NewName = Text.ToString();

	if (OldName == NewName) return;

	TSharedPtr<FESQLTableEditorToolkit> Editor = SQLTableEditor.Pin();
	UESQLTableAsset* Asset = Editor->GetTableAsset();
	TSharedPtr<FESQLDatabase> DB = Editor->GetDatabase();

	if (!Asset || !DB || !DB->IsOpen()) return;

	// UPDATE PK
	FString SQL = FString::Printf(TEXT("UPDATE \"%s\" SET \"%s\" = ?1 WHERE \"%s\" = ?2"),
		*Asset->TableName, *Asset->PrimaryKeyColumn, *Asset->PrimaryKeyColumn);
	TArray<FString> Bindings = { NewName, OldName };

	FESQLQueryResult Result = DB->Execute(SQL, Bindings);
	if (Result.bSuccess)
	{
		RowDataPtr->RowId = FName(*NewName);
		CurrentName = MakeShareable(new FName(*NewName));
		Editor->RefreshCachedDataTable(FName(*NewName));
	}
	else
	{
		UE_LOG(LogExtendedSQLEditor, Warning, TEXT("Failed to rename row: %s"), *Result.ErrorMessage);
	}
}

FReply SESQLTableListViewRow::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::F2)
	{
		SetRowForRename();
		return FReply::Handled();
	}
	if (InKeyEvent.GetKey() == EKeys::Delete)
	{
		if (SQLTableEditor.IsValid())
		{
			SQLTableEditor.Pin()->DeleteSelectedRow();
		}
		return FReply::Handled();
	}
	return SMultiColumnTableRow<FESQLEditorRowPtr>::OnKeyDown(MyGeometry, InKeyEvent);
}

FReply SESQLTableListViewRow::OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && RowDataPtr.IsValid() && SQLTableEditor.IsValid())
	{
		SQLTableEditor.Pin()->SetHighlightedRow(RowDataPtr->RowId);
		SetRowForRename();
		return FReply::Handled();
	}

	return SMultiColumnTableRow<FESQLEditorRowPtr>::OnMouseButtonDoubleClick(InMyGeometry, InMouseEvent);
}


// ── Accessors ───────────────────────────────────────────────────────────────

FText SESQLTableListViewRow::GetCurrentNameAsText() const
{
	return CurrentName.IsValid() ? FText::FromName(*CurrentName) : FText::GetEmpty();
}

FName SESQLTableListViewRow::GetCurrentName() const
{
	return CurrentName.IsValid() ? *CurrentName : NAME_None;
}

uint32 SESQLTableListViewRow::GetCurrentIndex() const
{
	return RowDataPtr.IsValid() ? RowDataPtr->RowNum : 0;
}

const TArray<FText>& SESQLTableListViewRow::GetCellValues() const
{
	return CellValues;
}

void SESQLTableListViewRow::SetRowForRename()
{
	if (InlineEditableText.IsValid())
	{
		InlineEditableText->EnterEditingMode();
	}
}

void SESQLTableListViewRow::SetIsDragDrop(bool bInIsDragDrop)
{
	bIsDragDropObject = bInIsDragDrop;
}


// ── Drag/Drop visual feedback ───────────────────────────────────────────────

void SESQLTableListViewRow::OnRowDragEnter(const FDragDropEvent& DragDropEvent)
{
	bIsHoveredDragTarget = true;
}

void SESQLTableListViewRow::OnRowDragLeave(const FDragDropEvent& DragDropEvent)
{
	bIsHoveredDragTarget = false;
}

FReply SESQLTableListViewRow::OnRowDrop(const FDragDropEvent& DragDropEvent)
{
	bIsHoveredDragTarget = false;
	return FReply::Handled();
}

const FSlateBrush* SESQLTableListViewRow::GetBorder() const
{
	return SMultiColumnTableRow<FESQLEditorRowPtr>::GetBorder();
}

void SESQLTableListViewRow::OnSearchForReferences()
{
	// No-op for SQL tables (no asset references)
}


// ── FESQLRowDragDropOp ──────────────────────────────────────────────────────

FESQLRowDragDropOp::FESQLRowDragDropOp(TSharedPtr<SESQLTableListViewRow> InRow)
{
	Row = InRow;

	if (InRow.IsValid())
	{
		InRow->SetIsDragDrop(true);

		DecoratorWidget = SNew(SBorder)
			.Padding(4)
			.BorderImage(FAppStyle::GetBrush("Graph.ConnectorFeedback.Border"))
			[
				SNew(STextBlock)
				.Text(InRow->GetCurrentNameAsText())
			];
	}
}

void FESQLRowDragDropOp::OnDrop(bool bDropWasHandled, const FPointerEvent& MouseEvent)
{
	if (Row.IsValid())
	{
		Row.Pin()->SetIsDragDrop(false);
	}
	FDecoratedDragDropOp::OnDrop(bDropWasHandled, MouseEvent);
}

#undef LOCTEXT_NAMESPACE
