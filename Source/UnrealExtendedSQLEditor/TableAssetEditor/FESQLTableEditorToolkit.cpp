// Copyright Kemal Erdem YILMAZ. All Rights Reserved.
// Adapted from Epic Games DataTableEditor.cpp — Slate layout preserved,
// all data access replaced with FESQLDatabase queries.

#include "FESQLTableEditorToolkit.h"
#include "TableAssetEditor/ESQLTableEditorDatabaseRegistry.h"
#include "SESQLTableListViewRow.h"
#include "SESQLRowEditor.h"
#include "TableAsset/ESQLTableAsset.h"
#include "Core/ESQLDatabase.h"
#include "Shared/ESQLSettings.h"
#include "UnrealExtendedSQLEditor.h"

#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Engine/UserDefinedStruct.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Notifications/NotificationManager.h"
#include "IDetailsView.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PropertyEditorModule.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Styling/AppStyle.h"
#include "Subsystem/ESQLSubsystem.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Fonts/FontMeasure.h"
#include "Rendering/SlateRenderer.h"
#include "DesktopPlatformModule.h"
#include "UObject/UnrealType.h"


#define LOCTEXT_NAMESPACE "ESQLTableEditor"

const FName FESQLTableEditorToolkit::TableTabId("ESQLTableEditor_Table");
const FName FESQLTableEditorToolkit::TableDetailsTabId("ESQLTableEditor_Details");
const FName FESQLTableEditorToolkit::RowEditorTabId("ESQLTableEditor_RowEditor");
const FName FESQLTableEditorToolkit::RowNameColumnId("RowName");
const FName FESQLTableEditorToolkit::RowNumberColumnId("RowNumber");


// ── Construction / Destruction ───────────────────────────────────────────────

FESQLTableEditorToolkit::FESQLTableEditorToolkit()
{
}

FESQLTableEditorToolkit::~FESQLTableEditorToolkit()
{
	// Unregister struct change notifications
	// Note: FStructureEditorUtils::RemoveListener was removed in UE 5.6
	// Struct change notifications are no longer supported via this pattern.

	FESQLTableEditorDatabaseRegistry::ClearIfMatches(TableAsset, Database);

	GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->NotifyAssetClosed(TableAsset, this);

	// Export `.usqlite/` on close so authored data stays text-first.
	if (TableAsset && Database && Database->IsOpen())
	{
		FString Error;
		TableAsset->ExportToUSqlite(TableAsset->GetUSqliteProjectPath(), Error, Database);
	}

	SaveLayoutData();
}


// ── Init ─────────────────────────────────────────────────────────────────────

void FESQLTableEditorToolkit::InitSQLTableEditor(
	const EToolkitMode::Type Mode,
	const TSharedPtr<class IToolkitHost>& InitToolkitHost,
	UESQLTableAsset* InTableAsset)
{
	TableAsset = InTableAsset;
	check(TableAsset);

	// Resolve table name from struct if empty
	if (TableAsset->TableName.IsEmpty() && TableAsset->RowStruct)
	{
		TableAsset->TableName = TableAsset->RowStruct->GetName();
	}

	UE_LOG(LogExtendedSQLEditor, Log, TEXT("InitSQLTableEditor: Asset='%s' DB='%s' Table='%s' Struct='%s'"),
		*TableAsset->GetName(),
		*TableAsset->DatabaseName,
		*TableAsset->TableName,
		TableAsset->RowStruct ? *TableAsset->RowStruct->GetName() : TEXT("(null)"));

	// Sync `.db` from `.usqlite/` on editor open, or create the initial `.usqlite/`
	// project from the live `.db` when no project exists yet.
	{
		FString SyncError;
		TableAsset->SyncDatabaseAndProject(SyncError);
		if (!SyncError.IsEmpty())
		{
			UE_LOG(LogExtendedSQLEditor, Warning, TEXT(".usqlite sync warning: %s"), *SyncError);
		}
	}

	// Open database connection for the editor
	const FString DbPath = UESQLSettings::ResolveDatabaseFilePath(TableAsset->DatabaseName);

	UE_LOG(LogExtendedSQLEditor, Log, TEXT("Opening database at: %s"), *DbPath);

	const FESQLDatabaseOpenResult OpenResult = FESQLDatabase::Open(DbPath);
	if (!OpenResult)
	{
		FESQLTableEditorDatabaseRegistry::Clear(TableAsset);
		UE_LOG(LogExtendedSQLEditor, Error, TEXT("Failed to open database for editor: %s"), *OpenResult.GetErrorMessage());
	}
	else
	{
		Database = OpenResult.GetValue();
		FESQLTableEditorDatabaseRegistry::Set(TableAsset, Database);
		UE_LOG(LogExtendedSQLEditor, Log, TEXT("Database opened successfully"));
	}

	// Register struct change notifications
	// Note: FStructureEditorUtils::AddListener was removed in UE 5.6
	// Struct change notifications are no longer supported via this pattern.

	// Build the editor layout — DataTable-style:
	//   Left (80%): Data grid (top 65%) + Row preview (bottom 35%)
	//   Right (20%): Table settings / asset details
	const TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout =
		FTabManager::NewLayout("Standalone_ESQLTableEditor_Layout_v2")
		->AddArea
		(
			FTabManager::NewPrimaryArea()->SetOrientation(Orient_Horizontal)
			->Split
			(
				// Left side: grid + row preview stacked vertically
				FTabManager::NewSplitter()->SetOrientation(Orient_Vertical)
				->SetSizeCoefficient(0.80f)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.65f)
					->AddTab(TableTabId, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.35f)
					->AddTab(RowEditorTabId, ETabState::OpenedTab)
				)
			)
			->Split
			(
				// Right side: table settings
				FTabManager::NewStack()
				->SetSizeCoefficient(0.20f)
				->AddTab(TableDetailsTabId, ETabState::OpenedTab)
			)
		);

	// Load layout and refresh data BEFORE InitAssetEditor.
	// InitAssetEditor triggers SpawnTab_Table() -> CreateContentBox() which
	// needs AvailableColumns already populated to build the header row.
	LoadLayoutData();
	RefreshCachedDataTable();

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	FAssetEditorToolkit::InitAssetEditor(
		Mode,
		InitToolkitHost,
		FName("ESQLTableEditorApp"),
		StandaloneDefaultLayout,
		bCreateDefaultStandaloneMenu,
		bCreateDefaultToolbar,
		TableAsset
	);

	// Extend toolbar with our SQL-specific buttons
	TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);
	ExtendToolbar(ToolbarExtender);
	AddToolbarExtender(ToolbarExtender);
	RegenerateMenusAndToolbars();
}


// ── Tab Management ──────────────────────────────────────────────────────────

void FESQLTableEditorToolkit::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);
	CreateAndRegisterTableTab(InTabManager);
	CreateAndRegisterRowEditorTab(InTabManager);
	CreateAndRegisterTableDetailsTab(InTabManager);
}

void FESQLTableEditorToolkit::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
	InTabManager->UnregisterTabSpawner(TableTabId);
	InTabManager->UnregisterTabSpawner(RowEditorTabId);
	InTabManager->UnregisterTabSpawner(TableDetailsTabId);
}

void FESQLTableEditorToolkit::CreateAndRegisterTableTab(const TSharedRef<class FTabManager>& InTabManager)
{
	InTabManager->RegisterTabSpawner(TableTabId, FOnSpawnTab::CreateSP(this, &FESQLTableEditorToolkit::SpawnTab_Table))
		.SetDisplayName(LOCTEXT("SQLTableTab", "SQL Table"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());
}

void FESQLTableEditorToolkit::CreateAndRegisterTableDetailsTab(const TSharedRef<class FTabManager>& InTabManager)
{
	InTabManager->RegisterTabSpawner(TableDetailsTabId, FOnSpawnTab::CreateSP(this, &FESQLTableEditorToolkit::SpawnTab_TableDetails))
		.SetDisplayName(LOCTEXT("SQLTableSettingsTab", "Table Settings"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());
}

void FESQLTableEditorToolkit::CreateAndRegisterRowEditorTab(const TSharedRef<class FTabManager>& InTabManager)
{
	InTabManager->RegisterTabSpawner(RowEditorTabId, FOnSpawnTab::CreateSP(this, &FESQLTableEditorToolkit::SpawnTab_RowEditor))
		.SetDisplayName(LOCTEXT("RowEditorTab", "Row Preview"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());
}


// ── Tab Spawning ────────────────────────────────────────────────────────────

TSharedRef<SDockTab> FESQLTableEditorToolkit::SpawnTab_Table(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId().TabType == TableTabId);

	TableTabWidget = CreateContentBox();

	return SNew(SDockTab)
		.Label(LOCTEXT("SQLTableTabLabel", "SQL Table"))
		[
			TableTabWidget.ToSharedRef()
		];
}

TSharedRef<SDockTab> FESQLTableEditorToolkit::SpawnTab_TableDetails(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId().TabType == TableDetailsTabId);

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DVArgs;
	DVArgs.bAllowSearch = false;
	DVArgs.bShowOptions = false;
	DVArgs.bAllowMultipleTopLevelObjects = false;
	DVArgs.bHideSelectionTip = true;
	DVArgs.NotifyHook = nullptr;

	PropertyView = PropertyModule.CreateDetailView(DVArgs);
	PropertyView->SetObject(TableAsset);

	return SNew(SDockTab)
		.Label(LOCTEXT("SQLTableSettingsLabel", "Table Settings"))
		[
			PropertyView.ToSharedRef()
		];
}

TSharedRef<SDockTab> FESQLTableEditorToolkit::SpawnTab_RowEditor(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId().TabType == RowEditorTabId);

	TSharedRef<SESQLRowEditor> RowEditor = SNew(SESQLRowEditor, TableAsset, Database)
		.SQLTableEditor(SharedThis(this));

	RowEditorTabWidget = RowEditor;

	return SNew(SDockTab)
		.Label(LOCTEXT("RowEditorTabLabel", "Row Preview"))
		[
			RowEditor
		];
}


// ── Toolkit Identity ────────────────────────────────────────────────────────

FName FESQLTableEditorToolkit::GetToolkitFName() const { return FName("ESQLTableEditor"); }
FText FESQLTableEditorToolkit::GetBaseToolkitName() const { return LOCTEXT("ToolkitName", "SQL Table Editor"); }
FString FESQLTableEditorToolkit::GetWorldCentricTabPrefix() const { return LOCTEXT("WorldCentricTabPrefix", "SQLTable ").ToString(); }
FLinearColor FESQLTableEditorToolkit::GetWorldCentricTabColorScale() const { return FLinearColor(0.22f, 0.70f, 0.67f, 0.5f); }
FString FESQLTableEditorToolkit::GetDocumentationLink() const { return FString(); }


// ── Data Refresh (CORE — replaces DataTable row map reads) ──────────────────

void FESQLTableEditorToolkit::RefreshCachedDataTable(const FName InCachedSelection, const bool bUpdateEvenIfValid)
{
	if (!Database || !Database->IsOpen() || !TableAsset)
	{
		return;
	}

	if (SortByColumn.IsNone())
	{
		SetDefaultSort();
	}

	AvailableColumns.Reset();
	AvailableRows.Reset();

	// Build columns from the RowStruct
	if (TableAsset->RowStruct)
	{
		// Primary key column first
		TSharedPtr<FESQLEditorColumnHeader> PKCol = MakeShareable(new FESQLEditorColumnHeader());
		PKCol->ColumnId = FName(*TableAsset->PrimaryKeyColumn);
		PKCol->DisplayName = FText::FromString(TableAsset->PrimaryKeyColumn);
		PKCol->Type = EESQLColumnType::Text;
		AvailableColumns.Add(PKCol);

		// Struct field columns — use GetAuthoredName() for display
		// (raw GetName() includes GUID suffixes for Blueprint UserDefinedStructs)
		TArray<FESQLColumn> ColDefs = TableAsset->GetColumnDefinitions();
		TMap<FString, FString> AuthoredNames;  // raw name → display name
		for (TFieldIterator<FProperty> It(TableAsset->RowStruct); It; ++It)
		{
			AuthoredNames.Add((*It)->GetName(), (*It)->GetAuthoredName());
		}

		for (int32 i = 1; i < ColDefs.Num(); ++i)  // Skip first (PK already added)
		{
			TSharedPtr<FESQLEditorColumnHeader> Col = MakeShareable(new FESQLEditorColumnHeader());
			Col->ColumnId = FName(*ColDefs[i].Name);  // raw name for data lookup
			const FString* DisplayName = AuthoredNames.Find(ColDefs[i].Name);
			Col->DisplayName = FText::FromString(DisplayName ? *DisplayName : ColDefs[i].Name);
			Col->Type = ColDefs[i].Type;
			AvailableColumns.Add(Col);
		}
	}

	// Query all rows from the database
	FString SQL = FString::Printf(TEXT("SELECT * FROM \"%s\""), *TableAsset->TableName);
	if (SortByColumn != NAME_None)
	{
		SQL += FString::Printf(TEXT(" ORDER BY \"%s\" %s"),
			*SortByColumn.ToString(),
			SortMode == EColumnSortMode::Ascending ? TEXT("ASC") : TEXT("DESC"));
	}

	FESQLQueryResult Result = Database->Execute(SQL);
	if (Result.bSuccess)
	{
		int32 RowNum = 1;
		for (const FESQLRow& SQLRow : Result.Rows)
		{
			TSharedPtr<FESQLEditorRow> EditorRow = MakeShareable(new FESQLEditorRow());
			EditorRow->RowNum = RowNum++;

			// Determine the row ID (primary key value)
			const FString* PKValue = SQLRow.Columns.Find(TableAsset->PrimaryKeyColumn);
			EditorRow->RowId = PKValue ? FName(**PKValue) : FName(*FString::FromInt(RowNum));

			// Copy all column values
			for (const auto& Pair : SQLRow.Columns)
			{
				EditorRow->CellData.Add(FName(*Pair.Key), Pair.Value);
			}

			AvailableRows.Add(EditorRow);
		}
	}

	// Ensure column widths array matches
	ColumnWidths.SetNum(AvailableColumns.Num());
	RefreshRowNameColumnWidth();
	RefreshRowNumberColumnWidth();

	UpdateVisibleRows(InCachedSelection, bUpdateEvenIfValid);
}

void FESQLTableEditorToolkit::UpdateVisibleRows(const FName InCachedSelection, const bool bUpdateEvenIfValid)
{
	VisibleRows.Reset();

	const FString FilterString = ActiveFilterText.ToString().ToLower();

	for (const FESQLEditorRowPtr& Row : AvailableRows)
	{
		if (FilterString.IsEmpty())
		{
			VisibleRows.Add(Row);
		}
		else
		{
			// Check if any cell value matches the filter
			bool bMatches = Row->RowId.ToString().ToLower().Contains(FilterString);
			if (!bMatches)
			{
				for (const auto& Pair : Row->CellData)
				{
					if (Pair.Value.ToLower().Contains(FilterString))
					{
						bMatches = true;
						break;
					}
				}
			}
			if (bMatches)
			{
				VisibleRows.Add(Row);
			}
		}
	}

	if (CellsListView.IsValid())
	{
		CellsListView->RequestListRefresh();
	}

	RestoreCachedSelection(InCachedSelection, bUpdateEvenIfValid);
}

void FESQLTableEditorToolkit::RestoreCachedSelection(const FName InCachedSelection, const bool bUpdateEvenIfValid)
{
	if (InCachedSelection != NAME_None)
	{
		SetHighlightedRow(InCachedSelection);
	}
}


// ── Content Box (main Slate layout — adapted from DataTableEditor) ──────────

TSharedRef<SVerticalBox> FESQLTableEditorToolkit::CreateContentBox()
{
	// Build header row
	ColumnNamesHeaderRow = SNew(SHeaderRow);

	// Row number column
	ColumnNamesHeaderRow->AddColumn(
		SHeaderRow::Column(RowNumberColumnId)
		.DefaultLabel(FText::GetEmpty())
		.ManualWidth(40)
		.SortMode(this, &FESQLTableEditorToolkit::GetColumnSortMode, RowNumberColumnId)
		.OnSort(this, &FESQLTableEditorToolkit::OnColumnSortModeChanged)
	);

	// Row name (PK) column
	ColumnNamesHeaderRow->AddColumn(
		SHeaderRow::Column(RowNameColumnId)
		.DefaultLabel(FText::FromString(TableAsset ? TableAsset->PrimaryKeyColumn : TEXT("RowName")))
		.ManualWidth(this, &FESQLTableEditorToolkit::GetRowNameColumnWidth)
		.OnWidthChanged(this, &FESQLTableEditorToolkit::OnRowNameColumnResized)
		.SortMode(this, &FESQLTableEditorToolkit::GetColumnSortMode, RowNameColumnId)
		.OnSort(this, &FESQLTableEditorToolkit::OnColumnNameSortModeChanged)
	);

	// Data columns
	for (int32 i = 0; i < AvailableColumns.Num(); ++i)
	{
		const FESQLEditorColumnHeaderPtr& Col = AvailableColumns[i];
		if (Col->ColumnId == FName(*TableAsset->PrimaryKeyColumn))
		{
			continue;  // Skip PK — already shown as RowName column
		}

		ColumnNamesHeaderRow->AddColumn(
			SHeaderRow::Column(Col->ColumnId)
			.DefaultLabel(Col->DisplayName)
			.ManualWidth(this, &FESQLTableEditorToolkit::GetColumnWidth, i)
			.OnWidthChanged(this, &FESQLTableEditorToolkit::OnColumnResized, i)
			.SortMode(this, &FESQLTableEditorToolkit::GetColumnSortMode, Col->ColumnId)
			.OnSort(this, &FESQLTableEditorToolkit::OnColumnSortModeChanged)
		);
	}

	// Search box
	SearchBoxWidget = SNew(SSearchBox)
		.OnTextChanged(this, &FESQLTableEditorToolkit::OnFilterTextChanged)
		.OnTextCommitted(this, &FESQLTableEditorToolkit::OnFilterTextCommitted);

	// List view
	CellsListView = SNew(SListView<FESQLEditorRowPtr>)
		.ListItemsSource(&VisibleRows)
		.HeaderRow(ColumnNamesHeaderRow)
		.OnGenerateRow(this, &FESQLTableEditorToolkit::MakeRowWidget)
		.OnSelectionChanged(this, &FESQLTableEditorToolkit::OnRowSelectionChanged)
		.SelectionMode(ESelectionMode::Single);

	// Status bar
	auto GetStatusText = [this]() -> FText
	{
		if (!TableAsset) return FText::GetEmpty();
		return FText::Format(
			LOCTEXT("StatusBar", "Database: {0}  |  Table: {1}  |  Rows: {2} ({3} visible)"),
			FText::FromString(TableAsset->DatabaseName),
			FText::FromString(TableAsset->TableName),
			FText::AsNumber(AvailableRows.Num()),
			FText::AsNumber(VisibleRows.Num())
		);
	};

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2)
		[
			SearchBoxWidget.ToSharedRef()
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SScrollBox)
			.Orientation(Orient_Horizontal)
			+ SScrollBox::Slot()
			[
				CellsListView.ToSharedRef()
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4, 2)
		[
			SNew(STextBlock)
			.Text_Lambda(GetStatusText)
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		];
}

TSharedRef<SWidget> FESQLTableEditorToolkit::CreateRowEditorBox()
{
	return SNew(SESQLRowEditor, TableAsset, Database)
		.SQLTableEditor(SharedThis(this));
}


// ── Row Widget Generation ───────────────────────────────────────────────────

TSharedRef<ITableRow> FESQLTableEditorToolkit::MakeRowWidget(
	FESQLEditorRowPtr InRowDataPtr,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SESQLTableListViewRow, OwnerTable)
		.SQLTableEditor(SharedThis(this))
		.RowDataPtr(InRowDataPtr);
}

TSharedRef<SWidget> FESQLTableEditorToolkit::MakeCellWidget(
	FESQLEditorRowPtr InRowDataPtr,
	const int32 InRowIndex,
	const FName& InColumnId)
{
	const FString* CellValue = InRowDataPtr->CellData.Find(InColumnId);
	const FString DisplayText = CellValue ? *CellValue : FString();

	return SNew(SBox)
		.Padding(FMargin(4, 2, 4, 2))
		[
			SNew(STextBlock)
			.Text(FText::FromString(DisplayText))
		];
}


// ── Row Selection ───────────────────────────────────────────────────────────

void FESQLTableEditorToolkit::OnRowSelectionChanged(
	FESQLEditorRowPtr InNewSelection,
	ESelectInfo::Type InSelectInfo)
{
	if (InNewSelection.IsValid())
	{
		SetHighlightedRow(InNewSelection->RowId);

		// Notify the row editor
		if (RowEditorTabWidget.IsValid())
		{
			TSharedPtr<SESQLRowEditor> RowEditor = StaticCastSharedPtr<SESQLRowEditor>(RowEditorTabWidget);
			if (RowEditor.IsValid())
			{
				RowEditor->SelectRow(InNewSelection->RowId);
			}
		}
	}
}

void FESQLTableEditorToolkit::SetHighlightedRow(FName Name)
{
	HighlightedRowName = Name;

	// Find the visible row index
	HighlightedVisibleRowIndex = INDEX_NONE;
	for (int32 i = 0; i < VisibleRows.Num(); ++i)
	{
		if (VisibleRows[i]->RowId == Name)
		{
			HighlightedVisibleRowIndex = i;
			break;
		}
	}

	CallbackOnRowHighlighted.ExecuteIfBound(Name);
}

FSlateColor FESQLTableEditorToolkit::GetRowTextColor(FName RowName) const
{
	if (RowName == HighlightedRowName)
	{
		return FSlateColor(FLinearColor(0.22f, 0.70f, 0.67f));  // Teal for selected
	}
	return FSlateColor::UseForeground();
}
// ── Toolbar ─────────────────────────────────────────────────────────────────

void FESQLTableEditorToolkit::ExtendToolbar(TSharedPtr<FExtender> Extender)
{
	Extender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateSP(this, &FESQLTableEditorToolkit::FillToolbar)
	);
}

void FESQLTableEditorToolkit::FillToolbar(FToolBarBuilder& ToolbarBuilder)
{
	ToolbarBuilder.BeginSection("SQLTableActions");

	ToolbarBuilder.AddToolBarButton(
		FUIAction(FExecuteAction::CreateSP(this, &FESQLTableEditorToolkit::OnBuildTestDatabaseClicked)),
		NAME_None,
		LOCTEXT("BuildTestDb", "Build Test DB"),
		LOCTEXT("BuildTestDbTooltip", "Build and open this table's test database through UESQLSubsystem."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Refresh")
	);

	ToolbarBuilder.AddToolBarButton(
		FUIAction(FExecuteAction::CreateSP(this, &FESQLTableEditorToolkit::OnRunQueryClicked)),
		NAME_None,
		LOCTEXT("RunQuery", "Run Test Query"),
		LOCTEXT("RunQueryTooltip", "Run a preview query against the subsystem-built test database and refresh the preview."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Search")
	);

	ToolbarBuilder.AddSeparator();

	ToolbarBuilder.AddToolBarButton(
		FUIAction(FExecuteAction::CreateSP(this, &FESQLTableEditorToolkit::OnRefreshClicked)),
		NAME_None,
		LOCTEXT("Refresh", "Refresh"),
		LOCTEXT("RefreshTooltip", "Reload data from the database"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Refresh")
	);

	ToolbarBuilder.AddSeparator();

	ToolbarBuilder.AddToolBarButton(
		FUIAction(FExecuteAction::CreateSP(this, &FESQLTableEditorToolkit::OnImportCSVClicked)),
		NAME_None,
		LOCTEXT("ImportCSV", "Import CSV"),
		LOCTEXT("ImportCSVTooltip", "Import rows from a CSV file (UPSERT)"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Import")
	);

	ToolbarBuilder.AddToolBarButton(
		FUIAction(FExecuteAction::CreateSP(this, &FESQLTableEditorToolkit::OnExportCSVClicked)),
		NAME_None,
		LOCTEXT("ExportCSV", "Export CSV"),
		LOCTEXT("ExportCSVTooltip", "Export all rows to a CSV file"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Save")
	);

	ToolbarBuilder.EndSection();
}


// ── SQL-specific toolbar actions ────────────────────────────────────────────

UESQLSubsystem* FESQLTableEditorToolkit::ResolvePreviewSubsystem(FString& OutError) const
{
	OutError.Reset();

	if (!GEditor)
	{
		OutError = TEXT("Editor instance is unavailable.");
		return nullptr;
	}

	UWorld* PreviewWorld = GEditor->PlayWorld;
	if (!PreviewWorld)
	{
		PreviewWorld = GEditor->GetEditorWorldContext().World();
	}

	UGameInstance* GameInstance = PreviewWorld ? PreviewWorld->GetGameInstance() : nullptr;
	if (!GameInstance)
	{
		OutError = TEXT("Build Test DB and Run Test Query require an active PIE or simulate world with a GameInstance.");
		return nullptr;
	}

	UESQLSubsystem* Subsystem = GameInstance->GetSubsystem<UESQLSubsystem>();
	if (!Subsystem)
	{
		OutError = TEXT("Failed to resolve UESQLSubsystem from the active GameInstance.");
	}

	return Subsystem;
}

bool FESQLTableEditorToolkit::ReopenPreviewDatabase(const FString& DatabasePath, FString& OutError)
{
	OutError.Reset();

	const FESQLDatabaseOpenResult OpenResult = FESQLDatabase::Open(DatabasePath);
	if (!OpenResult)
	{
		OutError = OpenResult.GetErrorMessage();
		return false;
	}

	Database = OpenResult.GetValue();
	FESQLTableEditorDatabaseRegistry::Set(TableAsset, Database);
	return true;
}

bool FESQLTableEditorToolkit::BuildSubsystemPreviewDatabase(FString& OutResolvedPath, FString& OutError)
{
	OutResolvedPath.Reset();
	OutError.Reset();

	if (!TableAsset)
	{
		OutError = TEXT("No table asset is loaded.");
		return false;
	}

	UESQLSubsystem* Subsystem = ResolvePreviewSubsystem(OutError);
	if (!Subsystem)
	{
		return false;
	}

	const FESQLQueryResult OpenResult = Subsystem->OpenDatabase(
		TableAsset->DatabaseName,
		TableAsset->Scope,
		TableAsset->Persistence);
	if (!OpenResult.bSuccess)
	{
		OutError = OpenResult.ErrorMessage;
		return false;
	}

	OutResolvedPath = Subsystem->GetDatabaseFilePath(TableAsset->DatabaseName);
	if (OutResolvedPath.IsEmpty())
	{
		OutError = TEXT("UESQLSubsystem did not report a resolved test database path.");
		return false;
	}

	return ReopenPreviewDatabase(OutResolvedPath, OutError);
}

void FESQLTableEditorToolkit::OnBuildTestDatabaseClicked()
{
	FString ResolvedPath;
	FString Error;
	if (!BuildSubsystemPreviewDatabase(ResolvedPath, Error))
	{
		FNotificationInfo Info(FText::Format(
			LOCTEXT("BuildTestDbFail", "Build Test DB failed: {0}"),
			FText::FromString(Error)));
		Info.ExpireDuration = 5.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
		return;
	}

	RefreshCachedDataTable(HighlightedRowName, true);

	FNotificationInfo Info(FText::Format(
		LOCTEXT("BuildTestDbSuccess", "Test DB ready: {0}"),
		FText::FromString(ResolvedPath)));
	Info.ExpireDuration = 4.0f;
	FSlateNotificationManager::Get().AddNotification(Info);
}

void FESQLTableEditorToolkit::OnRefreshClicked()
{
	RefreshCachedDataTable(HighlightedRowName);
}

void FESQLTableEditorToolkit::OnImportCSVClicked()
{
	if (!TableAsset) return;

	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform) return;

	TArray<FString> OutFiles;
	if (DesktopPlatform->OpenFileDialog(
		FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
		TEXT("Import CSV"),
		FPaths::ProjectDir(),
		TEXT(""),
		TEXT("CSV Files (*.csv)|*.csv"),
		EFileDialogFlags::None,
		OutFiles
	) && OutFiles.Num() > 0)
	{
		FString Error;
		if (TableAsset->ImportFromCSV(OutFiles[0], Error, Database))
		{
			RefreshCachedDataTable();
			FNotificationInfo Info(LOCTEXT("CSVImportSuccess", "CSV imported successfully"));
			Info.ExpireDuration = 3.0f;
			FSlateNotificationManager::Get().AddNotification(Info);
		}
		else
		{
			FNotificationInfo Info(FText::Format(LOCTEXT("CSVImportFail", "CSV import failed: {0}"), FText::FromString(Error)));
			Info.ExpireDuration = 5.0f;
			FSlateNotificationManager::Get().AddNotification(Info);
		}
	}
}

void FESQLTableEditorToolkit::OnExportCSVClicked()
{
	if (!TableAsset) return;

	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform) return;

	TArray<FString> OutFiles;
	if (DesktopPlatform->SaveFileDialog(
		FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
		TEXT("Export CSV"),
		FPaths::ProjectDir(),
		TableAsset->TableName + TEXT(".csv"),
		TEXT("CSV Files (*.csv)|*.csv"),
		EFileDialogFlags::None,
		OutFiles
	) && OutFiles.Num() > 0)
	{
		FString Error;
		if (TableAsset->ExportToCSV(OutFiles[0], Error, Database))
		{
			FNotificationInfo Info(LOCTEXT("CSVExportSuccess", "CSV exported successfully"));
			Info.ExpireDuration = 3.0f;
			FSlateNotificationManager::Get().AddNotification(Info);
		}
		else
		{
			FNotificationInfo Info(FText::Format(LOCTEXT("CSVExportFail", "CSV export failed: {0}"), FText::FromString(Error)));
			Info.ExpireDuration = 5.0f;
			FSlateNotificationManager::Get().AddNotification(Info);
		}
	}
}

void FESQLTableEditorToolkit::OnRunQueryClicked()
{
	if (!TableAsset)
	{
		return;
	}

	FString ResolvedPath;
	FString Error;
	if (!BuildSubsystemPreviewDatabase(ResolvedPath, Error))
	{
		FNotificationInfo Info(FText::Format(
			LOCTEXT("RunQueryBuildFail", "Run Test Query failed: {0}"),
			FText::FromString(Error)));
		Info.ExpireDuration = 5.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
		return;
	}

	UESQLSubsystem* Subsystem = ResolvePreviewSubsystem(Error);
	if (!Subsystem)
	{
		FNotificationInfo Info(FText::Format(
			LOCTEXT("RunQuerySubsystemFail", "Run Test Query failed: {0}"),
			FText::FromString(Error)));
		Info.ExpireDuration = 5.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
		return;
	}

	const FESQLQueryResult QueryResult = Subsystem->QueryTable(TableAsset, FESQLQuerySpec());
	if (!QueryResult.bSuccess)
	{
		FNotificationInfo Info(FText::Format(
			LOCTEXT("RunQueryFail", "Run Test Query failed: {0}"),
			FText::FromString(QueryResult.ErrorMessage)));
		Info.ExpireDuration = 5.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
		return;
	}

	RefreshCachedDataTable(HighlightedRowName, true);

	FNotificationInfo Info(FText::Format(
		LOCTEXT("RunQuerySuccess", "Queried {0} row(s) from test DB: {1}"),
		FText::AsNumber(QueryResult.Rows.Num()),
		FText::FromString(ResolvedPath)));
	Info.ExpireDuration = 4.0f;
	FSlateNotificationManager::Get().AddNotification(Info);
}

// ── Filter ──────────────────────────────────────────────────────────────────

void FESQLTableEditorToolkit::OnFilterTextChanged(const FText& InFilterText)
{
	ActiveFilterText = InFilterText;
	UpdateVisibleRows(HighlightedRowName);
}

void FESQLTableEditorToolkit::OnFilterTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo)
{
	OnFilterTextChanged(NewText);
}

FText FESQLTableEditorToolkit::GetFilterText() const
{
	return ActiveFilterText;
}


// ── Cell text accessors ─────────────────────────────────────────────────────

FText FESQLTableEditorToolkit::GetCellText(FESQLEditorRowPtr InRowDataPointer, int32 ColumnIndex) const
{
	if (!InRowDataPointer.IsValid() || ColumnIndex < 0 || ColumnIndex >= AvailableColumns.Num())
	{
		return FText::GetEmpty();
	}

	const FName ColId = AvailableColumns[ColumnIndex]->ColumnId;
	const FString* Value = InRowDataPointer->CellData.Find(ColId);
	return Value ? FText::FromString(*Value) : FText::GetEmpty();
}

FText FESQLTableEditorToolkit::GetCellToolTipText(FESQLEditorRowPtr InRowDataPointer, int32 ColumnIndex) const
{
	return GetCellText(InRowDataPointer, ColumnIndex);
}


// ── Column sizing ───────────────────────────────────────────────────────────

float FESQLTableEditorToolkit::GetRowNameColumnWidth() const { return RowNameColumnWidth > 0 ? RowNameColumnWidth : 150.0f; }
float FESQLTableEditorToolkit::GetRowNumberColumnWidth() const { return RowNumberColumnWidth > 0 ? RowNumberColumnWidth : 40.0f; }
float FESQLTableEditorToolkit::GetColumnWidth(const int32 ColumnIndex) const
{
	if (ColumnWidths.IsValidIndex(ColumnIndex) && !ColumnWidths[ColumnIndex].bIsAutoSized)
	{
		return ColumnWidths[ColumnIndex].CurrentWidth;
	}
	return 120.0f;
}

void FESQLTableEditorToolkit::OnColumnResized(const float NewWidth, const int32 ColumnIndex)
{
	if (ColumnWidths.IsValidIndex(ColumnIndex))
	{
		ColumnWidths[ColumnIndex].bIsAutoSized = false;
		ColumnWidths[ColumnIndex].CurrentWidth = NewWidth;
	}
}

void FESQLTableEditorToolkit::OnRowNameColumnResized(const float NewWidth) { RowNameColumnWidth = NewWidth; }
void FESQLTableEditorToolkit::OnRowNumberColumnResized(const float NewWidth) { RowNumberColumnWidth = NewWidth; }

void FESQLTableEditorToolkit::RefreshRowNameColumnWidth()
{
	if (RowNameColumnWidth <= 0)
	{
		RowNameColumnWidth = 150.0f;
	}
}

void FESQLTableEditorToolkit::RefreshRowNumberColumnWidth()
{
	if (RowNumberColumnWidth <= 0)
	{
		RowNumberColumnWidth = 40.0f;
	}
}


// ── Sorting ─────────────────────────────────────────────────────────────────

void FESQLTableEditorToolkit::SetDefaultSort()
{
	SortMode = EColumnSortMode::Ascending;
	SortByColumn = FName(*TableAsset->PrimaryKeyColumn);
}

EColumnSortMode::Type FESQLTableEditorToolkit::GetColumnSortMode(const FName ColumnId) const
{
	return (ColumnId == SortByColumn) ? SortMode : EColumnSortMode::None;
}

void FESQLTableEditorToolkit::OnColumnSortModeChanged(
	const EColumnSortPriority::Type SortPriority,
	const FName& ColumnId,
	const EColumnSortMode::Type InSortMode)
{
	SortByColumn = ColumnId;
	SortMode = InSortMode;
	RefreshCachedDataTable(HighlightedRowName);
}

void FESQLTableEditorToolkit::OnColumnNameSortModeChanged(
	const EColumnSortPriority::Type SortPriority,
	const FName& ColumnId,
	const EColumnSortMode::Type InSortMode)
{
	OnColumnSortModeChanged(SortPriority, FName(*TableAsset->PrimaryKeyColumn), InSortMode);
}


// ── Layout persistence (JSON) ───────────────────────────────────────────────

void FESQLTableEditorToolkit::LoadLayoutData()
{
	if (!TableAsset) return;

	const FString LayoutFile = FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("AssetData"),
		TEXT("ESQLTableEditorLayout"),
		TableAsset->GetName() + TEXT(".json")
	);

	FString JsonString;
	if (FFileHelper::LoadFileToString(JsonString, *LayoutFile))
	{
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
		FJsonSerializer::Deserialize(Reader, LayoutData);
	}
}

void FESQLTableEditorToolkit::SaveLayoutData()
{
	if (!TableAsset || !LayoutData.IsValid()) return;

	const FString LayoutFile = FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("AssetData"),
		TEXT("ESQLTableEditorLayout"),
		TableAsset->GetName() + TEXT(".json")
	);

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(LayoutData.ToSharedRef(), Writer);
	FFileHelper::SaveStringToFile(JsonString, *LayoutFile);
}


// ── Undo / Redo / Struct change ─────────────────────────────────────────────

void FESQLTableEditorToolkit::PostUndo(bool bSuccess) { HandleUndoRedo(); }
void FESQLTableEditorToolkit::PostRedo(bool bSuccess) { HandleUndoRedo(); }
void FESQLTableEditorToolkit::HandleUndoRedo() { RefreshCachedDataTable(HighlightedRowName); }

void FESQLTableEditorToolkit::PreChange(const UUserDefinedStruct* Struct, FStructureEditorUtils::EStructureEditorChangeInfo Info)
{
	const UUserDefinedStruct* EditedUserStruct = Cast<UUserDefinedStruct>(TableAsset ? TableAsset->RowStruct : nullptr);
	if (!Struct || Struct != EditedUserStruct)
	{
		return;
	}

	if (RowEditorTabWidget.IsValid())
	{
		TSharedPtr<SESQLRowEditor> RowEditor = StaticCastSharedPtr<SESQLRowEditor>(RowEditorTabWidget);
		if (RowEditor.IsValid())
		{
			RowEditor->HandleStructPreChange();
		}
	}
}

void FESQLTableEditorToolkit::PostChange(const UUserDefinedStruct* Struct, FStructureEditorUtils::EStructureEditorChangeInfo Info)
{
	const UUserDefinedStruct* EditedUserStruct = Cast<UUserDefinedStruct>(TableAsset ? TableAsset->RowStruct : nullptr);
	if (!Struct || Struct != EditedUserStruct)
	{
		return;
	}

	// Struct changed — run schema migration and refresh
	if (TableAsset && Database)
	{
		TableAsset->SyncSchema(Database);
		RefreshCachedDataTable(HighlightedRowName);
	}

	if (RowEditorTabWidget.IsValid())
	{
		TSharedPtr<SESQLRowEditor> RowEditor = StaticCastSharedPtr<SESQLRowEditor>(RowEditorTabWidget);
		if (RowEditor.IsValid())
		{
			RowEditor->HandleStructPostChange();
		}
	}
}


// ── Misc ─────────────────────────────────────────────────────────────────────

void FESQLTableEditorToolkit::PostRegenerateMenusAndToolbars() {}

#undef LOCTEXT_NAMESPACE
