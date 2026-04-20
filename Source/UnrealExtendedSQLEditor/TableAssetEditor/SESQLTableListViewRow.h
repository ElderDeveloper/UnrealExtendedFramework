// Copyright Kemal Erdem YILMAZ. All Rights Reserved.
// Adapted from Epic Games SDataTableListViewRow — preview row rendering preserved,
// data model replaced with FESQLEditorRow.

#pragma once

#include "CoreMinimal.h"
#include "FESQLTableEditorToolkit.h"
#include "Widgets/Views/STableRow.h"

class FESQLTableEditorToolkit;
class STableViewBase;
class SWidget;

/**
 * Represents one row in the SQL Table Editor grid.
 * Adapted from SDataTableListViewRow — cell rendering and selection are preserved.
 * Data source is FESQLEditorRow instead of UDataTable row map.
 */
class SESQLTableListViewRow : public SMultiColumnTableRow<FESQLEditorRowPtr>
{
public:

	SLATE_BEGIN_ARGS(SESQLTableListViewRow) {}
		SLATE_ARGUMENT(TSharedPtr<FESQLTableEditorToolkit>, SQLTableEditor)
		SLATE_ARGUMENT(FESQLEditorRowPtr, RowDataPtr)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView);
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;

private:

	TSharedRef<SWidget> MakeCellWidget(const int32 InRowIndex, const FName& InColumnId);

	FESQLEditorRowPtr RowDataPtr;
	TWeakPtr<FESQLTableEditorToolkit> SQLTableEditor;
};
