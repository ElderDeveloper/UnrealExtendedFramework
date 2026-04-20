// Copyright Kemal Erdem YILMAZ. All Rights Reserved.
// Adapted from Epic Games DataTableEditor — Slate layout and UX preserved,
// data model replaced with SQLite backend via UESQLTableAsset.

#pragma once

#include "CoreMinimal.h"
#include "EditorUndoClient.h"
#include "Kismet2/StructureEditorUtils.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/SListView.h"
#include "Shared/ESQLTypes.h"

class FExtender;
class FJsonObject;
class FSpawnTabArgs;
class FToolBarBuilder;
class ITableRow;
class SDockTab;
class SESQLRowEditor;
class SSearchBox;
class STableViewBase;
class SVerticalBox;
class SWidget;
class UESQLTableAsset;
class FESQLDatabase;

DECLARE_DELEGATE_OneParam(FOnRowHighlighted, FName /*Row name*/);


// ── Row/Column data structures (replaces FDataTableEditorRowListViewData) ────

/** Column header info for the editor grid. */
struct FESQLEditorColumnHeader
{
	FName ColumnId;
	FText DisplayName;
	EESQLColumnType Type = EESQLColumnType::Text;
	float DesiredWidth = 100.0f;
};
using FESQLEditorColumnHeaderPtr = TSharedPtr<FESQLEditorColumnHeader>;

/** One row in the editor grid — maps column names to string values. */
struct FESQLEditorRow
{
	/** Primary key value for this row. */
	FName RowId;

	/** Row number (1-based display index). */
	int32 RowNum = 0;

	/** Column name → display string value. */
	TMap<FName, FString> CellData;
};
using FESQLEditorRowPtr = TSharedPtr<FESQLEditorRow>;


// ── Main Editor Toolkit ─────────────────────────────────────────────────────

/**
 * Asset editor toolkit for UESQLTableAsset.
 * Adapted from UE's FDataTableEditor — Slate layout, column sorting, row
 * selection, header generation, and cell editing are preserved.
 * Data reads/writes go through FESQLDatabase instead of UDataTable's TMap.
 */
class FESQLTableEditorToolkit : public FAssetEditorToolkit
	, public FEditorUndoClient
	, public FStructureEditorUtils::INotifyOnStructChanged
{
	friend class SESQLTableListViewRow;

public:

	FESQLTableEditorToolkit();
	virtual ~FESQLTableEditorToolkit();

	// ── Init ─────────────────────────────────────────────────────────────

	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;

	/** Initialize the editor with a SQL Table Asset. */
	void InitSQLTableEditor(
		const EToolkitMode::Type Mode,
		const TSharedPtr<class IToolkitHost>& InitToolkitHost,
		UESQLTableAsset* InTableAsset
	);


	// ── FAssetEditorToolkit interface ─────────────────────────────────────

	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;
	virtual FString GetDocumentationLink() const override;


	// ── FEditorUndoClient ────────────────────────────────────────────────

	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;
	void HandleUndoRedo();


	// ── INotifyOnStructChanged ───────────────────────────────────────────

	virtual void PreChange(const class UUserDefinedStruct* Struct, FStructureEditorUtils::EStructureEditorChangeInfo Info) override;
	virtual void PostChange(const class UUserDefinedStruct* Struct, FStructureEditorUtils::EStructureEditorChangeInfo Info) override;


	// ── Accessors ────────────────────────────────────────────────────────

	/** Get the SQL Table Asset being edited. */
	UESQLTableAsset* GetTableAsset() const { return TableAsset; }

	/** Get the database connection. */
	TSharedPtr<FESQLDatabase> GetDatabase() const { return Database; }

	void SetHighlightedRow(FName Name);
	FText GetFilterText() const;
	FSlateColor GetRowTextColor(FName RowName) const;

protected:

	// ── Data refresh ─────────────────────────────────────────────────────

	/** Reload all rows from the database and rebuild AvailableRows/Columns. */
	void RefreshCachedDataTable(const FName InCachedSelection = NAME_None, const bool bUpdateEvenIfValid = false);

	/** Apply filter text to AvailableRows and populate VisibleRows. */
	void UpdateVisibleRows(const FName InCachedSelection = NAME_None, const bool bUpdateEvenIfValid = false);

	void RestoreCachedSelection(const FName InCachedSelection, const bool bUpdateEvenIfValid = false);

	void OnFilterTextChanged(const FText& InFilterText);
	void OnFilterTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo);


	// ── Slate builders ───────────────────────────────────────────────────

	virtual void PostRegenerateMenusAndToolbars() override;

	FText GetCellText(FESQLEditorRowPtr InRowDataPointer, int32 ColumnIndex) const;
	FText GetCellToolTipText(FESQLEditorRowPtr InRowDataPointer, int32 ColumnIndex) const;

	TSharedRef<SVerticalBox> CreateContentBox();
	TSharedRef<SWidget> CreateRowEditorBox();

	TSharedRef<SDockTab> SpawnTab_Table(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_TableDetails(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_RowEditor(const FSpawnTabArgs& Args);


	// ── Column sizing ────────────────────────────────────────────────────

	float GetRowNameColumnWidth() const;
	void RefreshRowNameColumnWidth();
	float GetRowNumberColumnWidth() const;
	void RefreshRowNumberColumnWidth();
	float GetColumnWidth(const int32 ColumnIndex) const;
	void OnColumnResized(const float NewWidth, const int32 ColumnIndex);
	void OnRowNameColumnResized(const float NewWidth);
	void OnRowNumberColumnResized(const float NewWidth);
	void LoadLayoutData();
	void SaveLayoutData();


	// ── Row widget builders ──────────────────────────────────────────────

	TSharedRef<ITableRow> MakeRowWidget(FESQLEditorRowPtr InRowDataPtr, const TSharedRef<STableViewBase>& OwnerTable);
	TSharedRef<SWidget> MakeCellWidget(FESQLEditorRowPtr InRowDataPtr, const int32 InRowIndex, const FName& InColumnId);
	void OnRowSelectionChanged(FESQLEditorRowPtr InNewSelection, ESelectInfo::Type InSelectInfo);

	virtual void CreateAndRegisterTableTab(const TSharedRef<class FTabManager>& InTabManager);
	virtual void CreateAndRegisterTableDetailsTab(const TSharedRef<class FTabManager>& InTabManager);
	virtual void CreateAndRegisterRowEditorTab(const TSharedRef<class FTabManager>& InTabManager);


	// ── Toolbar / Sorting ────────────────────────────────────────────────

	void SetDefaultSort();
	EColumnSortMode::Type GetColumnSortMode(const FName ColumnId) const;
	void OnColumnSortModeChanged(const EColumnSortPriority::Type SortPriority, const FName& ColumnId, const EColumnSortMode::Type InSortMode);
	void OnColumnNameSortModeChanged(const EColumnSortPriority::Type SortPriority, const FName& ColumnId, const EColumnSortMode::Type InSortMode);

	void ExtendToolbar(TSharedPtr<FExtender> Extender);
	void FillToolbar(FToolBarBuilder& ToolbarBuilder);


	// ── SQL-specific toolbar actions ─────────────────────────────────────

	void OnBuildTestDatabaseClicked();
	void OnImportCSVClicked();
	void OnExportCSVClicked();
	void OnRefreshClicked();
	void OnRunQueryClicked();
	class UESQLSubsystem* ResolvePreviewSubsystem(FString& OutError) const;
	bool ReopenPreviewDatabase(const FString& DatabasePath, FString& OutError);
	bool BuildSubsystemPreviewDatabase(FString& OutResolvedPath, FString& OutError);


protected:

	/** Struct holding information about the current column widths */
	struct FColumnWidth
	{
		FColumnWidth() : bIsAutoSized(true), CurrentWidth(0.0f) {}
		bool bIsAutoSized;
		float CurrentWidth;
	};

	// ── Core state ───────────────────────────────────────────────────────

	/** The SQL Table Asset being edited. */
	UESQLTableAsset* TableAsset = nullptr;

	/** Database connection (owned by the editor, closed on destroy). */
	TSharedPtr<FESQLDatabase> Database;


	// ── UI state ─────────────────────────────────────────────────────────

	TSharedPtr<SWidget> TableTabWidget;
	TSharedPtr<class IDetailsView> PropertyView;
	TSharedPtr<SSearchBox> SearchBoxWidget;
	TSharedPtr<SWidget> RowEditorTabWidget;

	TArray<FESQLEditorColumnHeaderPtr> AvailableColumns;
	TArray<FESQLEditorRowPtr> AvailableRows;
	TArray<FESQLEditorRowPtr> VisibleRows;

	TSharedPtr<SHeaderRow> ColumnNamesHeaderRow;
	TSharedPtr<SListView<FESQLEditorRowPtr>> CellsListView;

	float RowNameColumnWidth = 0.0f;
	float RowNumberColumnWidth = 0.0f;
	TArray<FColumnWidth> ColumnWidths;
	TSharedPtr<FJsonObject> LayoutData;

	FName HighlightedRowName;
	int32 HighlightedVisibleRowIndex = INDEX_NONE;
	FText ActiveFilterText;

	EColumnSortMode::Type SortMode = EColumnSortMode::Ascending;
	FName SortByColumn;

	FOnRowHighlighted CallbackOnRowHighlighted;

	static const FName TableTabId;
	static const FName TableDetailsTabId;
	static const FName RowEditorTabId;
	static const FName RowNameColumnId;
	static const FName RowNumberColumnId;
};
