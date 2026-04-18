// Copyright Kemal Erdem YILMAZ. All Rights Reserved.
// Adapted from Epic Games DataTableEditor.cpp — Slate layout preserved,
// all data access replaced with FESQLDatabase queries.

#include "FESQLTableEditorToolkit.h"
#include "SESQLTableListViewRow.h"
#include "SESQLRowEditor.h"
#include "TableAsset/ESQLTableAsset.h"
#include "Core/ESQLDatabase.h"
#include "Shared/ESQLPropertySerializer.h"
#include "Shared/ESQLSettings.h"
#include "VCSPipeline/ESQLDumpPipeline.h"
#include "UnrealExtendedSQLEditor.h"

#include "Dom/JsonObject.h"
#include "Editor.h"
#include "StructUtils/UserDefinedStruct.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "IDetailsView.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PropertyEditorModule.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Styling/AppStyle.h"
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

	if (TableAsset && TableAsset->GetCachedDatabase() == Database)
	{
		TableAsset->SetCachedDatabase(nullptr);
	}

	GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->NotifyAssetClosed(TableAsset, this);

	// Export .sqldump on close (VCS safe save order)
	if (TableAsset && Database && Database->IsOpen())
	{
		FString Error;
		TableAsset->ExportToSQLDump(TableAsset->GetSQLDumpPath(), Error, Database);
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

	// Sync .db and .sqldump on editor open (Phase 7 bidirectional sync)
	{
		FString SyncError;
		TableAsset->SyncDatabaseAndDump(SyncError);
		if (!SyncError.IsEmpty())
		{
			UE_LOG(LogExtendedSQLEditor, Warning, TEXT("VCS sync warning: %s"), *SyncError);
		}
	}

	// Open database connection for the editor
	const FString DbPath = UESQLSettings::ResolveDatabaseFilePath(TableAsset->DatabaseName);

	UE_LOG(LogExtendedSQLEditor, Log, TEXT("Opening database at: %s"), *DbPath);

	FString DbError;
	Database = FESQLDatabase::Open(DbPath, DbError);
	if (!Database)
	{
		TableAsset->SetCachedDatabase(nullptr);
		UE_LOG(LogExtendedSQLEditor, Error, TEXT("Failed to open database for editor: %s"), *DbError);
	}
	else
	{
		TableAsset->SetCachedDatabase(Database);
		UE_LOG(LogExtendedSQLEditor, Log, TEXT("Database opened successfully"));

		// Ensure the table exists (CREATE TABLE IF NOT EXISTS + SyncSchema)
		// This is critical — without it, Add Row would fail on first open.
		FString TableError;
		if (!TableAsset->EnsureTableExists(Database, TableError))
		{
			UE_LOG(LogExtendedSQLEditor, Error, TEXT("EnsureTableExists FAILED: %s"), *TableError);
		}
		else
		{
			UE_LOG(LogExtendedSQLEditor, Log, TEXT("Table '%s' ensured/verified in database"), *TableAsset->TableName);
		}
	}

	// Register struct change notifications
	// Note: FStructureEditorUtils::AddListener was removed in UE 5.6
	// Struct change notifications are no longer supported via this pattern.

	// Build the editor layout — DataTable-style:
	//   Left (80%): Data grid (top 65%) + Row editor (bottom 35%)
	//   Right (20%): Table settings / asset details
	const TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout =
		FTabManager::NewLayout("Standalone_ESQLTableEditor_Layout_v2")
		->AddArea
		(
			FTabManager::NewPrimaryArea()->SetOrientation(Orient_Horizontal)
			->Split
			(
				// Left side: grid + row editor stacked vertically
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
		.SetDisplayName(LOCTEXT("RowEditorTab", "Row Editor"))
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
		.Label(LOCTEXT("RowEditorTabLabel", "Row Editor"))
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
bool FESQLTableEditorToolkit::CanEditRows() const { return true; }


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
		.RowDataPtr(InRowDataPtr)
		.IsEditable(CanEditRows());
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


// ── Row Operations (INSERT, DELETE, UPDATE via SQL) ─────────────────────────

void FESQLTableEditorToolkit::OnAddClicked()
{
	if (!Database)
	{
		FNotificationInfo Info(LOCTEXT("AddRowNoDb", "Add Row failed: No database connection"));
		Info.ExpireDuration = 5.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
		return;
	}

	if (!Database->IsOpen())
	{
		FNotificationInfo Info(LOCTEXT("AddRowDbClosed", "Add Row failed: Database is not open"));
		Info.ExpireDuration = 5.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
		return;
	}

	if (!TableAsset)
	{
		FNotificationInfo Info(LOCTEXT("AddRowNoAsset", "Add Row failed: No table asset"));
		Info.ExpireDuration = 5.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
		return;
	}

	// Resolve table name from struct if empty
	FString EffectiveTableName = TableAsset->TableName;
	if (EffectiveTableName.IsEmpty() && TableAsset->RowStruct)
	{
		EffectiveTableName = TableAsset->RowStruct->GetName();
		TableAsset->TableName = EffectiveTableName;
	}

	if (EffectiveTableName.IsEmpty())
	{
		FNotificationInfo Info(LOCTEXT("AddRowNoTable", "Add Row failed: TableName is empty"));
		Info.ExpireDuration = 5.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
		return;
	}

	// Ensure the table exists first
	FString EnsureError;
	if (!TableAsset->EnsureTableExists(Database, EnsureError))
	{
		FNotificationInfo Info(FText::Format(
			LOCTEXT("AddRowEnsureFail", "Add Row: Could not create table: {0}"),
			FText::FromString(EnsureError)));
		Info.ExpireDuration = 5.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
		return;
	}

	// Generate a unique row name
	FString NewRowName = FString::Printf(TEXT("NewRow_%d"), FMath::Rand());

	FString ColumnList = FString::Printf(TEXT("\"%s\""), *TableAsset->PrimaryKeyColumn);
	FString PlaceholderList = TEXT("?1");
	TArray<FESQLBindingValue> Bindings = { FESQLBindingValue::FromText(NewRowName) };
	int32 BindIndex = 2;

	if (TableAsset->RowStruct)
	{
		FStructOnScope DefaultRowScope(TableAsset->RowStruct);
		const void* DefaultRowData = DefaultRowScope.GetStructMemory();

		for (TFieldIterator<FProperty> It(TableAsset->RowStruct); It; ++It)
		{
			FProperty* Property = *It;
			FESQLBindingValue BindingValue;

			if (!FESQLPropertySerializer::SerializePropertyToBindingValue(Property, DefaultRowData, BindingValue))
			{
				continue;
			}

			ColumnList += FString::Printf(TEXT(", \"%s\""), *Property->GetName());
			PlaceholderList += FString::Printf(TEXT(", ?%d"), BindIndex);
			Bindings.Add(BindingValue);
			++BindIndex;
		}
	}

	FString SQL = FString::Printf(TEXT("INSERT INTO \"%s\" (%s) VALUES (%s)"),
		*EffectiveTableName, *ColumnList, *PlaceholderList);

	UE_LOG(LogExtendedSQLEditor, Log, TEXT("Add Row SQL: %s  Binding: %s"), *SQL, *NewRowName);

	FESQLQueryResult Result = Database->Execute(SQL, Bindings);
	if (Result.bSuccess)
	{
		RefreshCachedDataTable(FName(*NewRowName));
		UE_LOG(LogExtendedSQLEditor, Log, TEXT("Added row: %s"), *NewRowName);
	}
	else
	{
		UE_LOG(LogExtendedSQLEditor, Error, TEXT("Failed to add row: %s"), *Result.ErrorMessage);
		FNotificationInfo Info(FText::Format(
			LOCTEXT("AddRowSQLFail", "Add Row SQL failed: {0}"),
			FText::FromString(Result.ErrorMessage)));
		Info.ExpireDuration = 5.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
	}
}

void FESQLTableEditorToolkit::OnRemoveClicked()
{
	DeleteSelectedRow();
}

void FESQLTableEditorToolkit::DeleteSelectedRow()
{
	if (!Database || !Database->IsOpen() || !TableAsset) return;
	if (HighlightedRowName == NAME_None) return;

	FString SQL = FString::Printf(TEXT("DELETE FROM \"%s\" WHERE \"%s\" = ?1"),
		*TableAsset->TableName, *TableAsset->PrimaryKeyColumn);
	TArray<FString> Bindings = { HighlightedRowName.ToString() };

	FESQLQueryResult Result = Database->Execute(SQL, Bindings);
	if (Result.bSuccess)
	{
		UE_LOG(LogExtendedSQLEditor, Log, TEXT("Deleted row: %s"), *HighlightedRowName.ToString());
		HighlightedRowName = NAME_None;
		RefreshCachedDataTable();
	}
	else
	{
		UE_LOG(LogExtendedSQLEditor, Warning, TEXT("Failed to delete row: %s"), *Result.ErrorMessage);
	}
}

void FESQLTableEditorToolkit::CopySelectedRow()
{
	if (HighlightedRowName == NAME_None) return;

	// Find the row data
	for (const FESQLEditorRowPtr& Row : AvailableRows)
	{
		if (Row->RowId == HighlightedRowName)
		{
			// Serialize row to clipboard as TSV
			FString ClipboardText;
			for (const auto& Pair : Row->CellData)
			{
				if (!ClipboardText.IsEmpty()) ClipboardText += TEXT("\t");
				ClipboardText += Pair.Value;
			}
			FPlatformApplicationMisc::ClipboardCopy(*ClipboardText);
			break;
		}
	}
}

void FESQLTableEditorToolkit::DuplicateSelectedRow()
{
	if (!Database || !Database->IsOpen() || !TableAsset) return;
	if (HighlightedRowName == NAME_None) return;

	// Find the row and insert a copy with new PK
	for (const FESQLEditorRowPtr& Row : AvailableRows)
	{
		if (Row->RowId == HighlightedRowName)
		{
			FString NewPK = HighlightedRowName.ToString() + TEXT("_Copy");

			FString ColList, PlaceholderList;
			TArray<FString> Bindings;
			int32 Idx = 1;

			for (const auto& Pair : Row->CellData)
			{
				if (Idx > 1) { ColList += TEXT(", "); PlaceholderList += TEXT(", "); }
				ColList += FString::Printf(TEXT("\"%s\""), *Pair.Key.ToString());
				PlaceholderList += FString::Printf(TEXT("?%d"), Idx);

				// Replace PK with new name
				if (Pair.Key.ToString() == TableAsset->PrimaryKeyColumn)
				{
					Bindings.Add(NewPK);
				}
				else
				{
					Bindings.Add(Pair.Value);
				}
				++Idx;
			}

			FString SQL = FString::Printf(TEXT("INSERT INTO \"%s\" (%s) VALUES (%s)"),
				*TableAsset->TableName, *ColList, *PlaceholderList);

			FESQLQueryResult Result = Database->Execute(SQL, Bindings);
			if (Result.bSuccess)
			{
				RefreshCachedDataTable(FName(*NewPK));
			}
			break;
		}
	}
}

void FESQLTableEditorToolkit::PasteOnSelectedRow() { /* TODO: Parse clipboard TSV and UPDATE */ }
void FESQLTableEditorToolkit::RenameSelectedRowCommand() { /* TODO: Inline rename */ }


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
		FUIAction(FExecuteAction::CreateSP(this, &FESQLTableEditorToolkit::OnAddClicked)),
		NAME_None,
		LOCTEXT("AddRow", "+ Add Row"),
		LOCTEXT("AddRowTooltip", "Insert a new row into the table"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Plus")
	);

	ToolbarBuilder.AddToolBarButton(
		FUIAction(FExecuteAction::CreateSP(this, &FESQLTableEditorToolkit::OnRemoveClicked)),
		NAME_None,
		LOCTEXT("DeleteRow", "- Delete"),
		LOCTEXT("DeleteRowTooltip", "Delete the selected row"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Delete")
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
	// TODO: Open a raw SQL input dialog
}

void FESQLTableEditorToolkit::OnEditDataTableStructClicked()
{
	if (TableAsset && TableAsset->RowStruct)
	{
		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(const_cast<UScriptStruct*>(TableAsset->RowStruct));
	}
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
void FESQLTableEditorToolkit::HandlePostChange() { RefreshCachedDataTable(HighlightedRowName); }

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

FReply FESQLTableEditorToolkit::OnFindRowInContentBrowserClicked()
{
	return FReply::Handled();
}

bool FESQLTableEditorToolkit::CanEditTable() const
{
	return TableAsset != nullptr && Database != nullptr && Database->IsOpen();
}

void FESQLTableEditorToolkit::OnCopyClicked() { CopySelectedRow(); }
void FESQLTableEditorToolkit::OnPasteClicked() { PasteOnSelectedRow(); }
void FESQLTableEditorToolkit::OnDuplicateClicked() { DuplicateSelectedRow(); }

#undef LOCTEXT_NAMESPACE
