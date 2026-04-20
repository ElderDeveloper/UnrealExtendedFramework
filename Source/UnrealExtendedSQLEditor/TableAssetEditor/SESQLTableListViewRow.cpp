// Copyright Kemal Erdem YILMAZ. All Rights Reserved.
// Adapted from Epic Games SDataTableListViewRow — preview row rendering preserved,
// data model replaced with FESQLEditorRow.

#include "SESQLTableListViewRow.h"
#include "FESQLTableEditorToolkit.h"

#include "Styling/AppStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"


#define LOCTEXT_NAMESPACE "SESQLTableListViewRow"

void SESQLTableListViewRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
{
	RowDataPtr = InArgs._RowDataPtr;
	SQLTableEditor = InArgs._SQLTableEditor;

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
				SNew(STextBlock)
				.Text(FText::FromName(RowDataPtr->RowId))
				.ColorAndOpacity_Lambda([this]() -> FSlateColor
				{
					return SQLTableEditor.IsValid()
						? SQLTableEditor.Pin()->GetRowTextColor(RowDataPtr->RowId)
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

#undef LOCTEXT_NAMESPACE
