// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "K2Node/SGraphPinESQLId.h"

#include "Core/ESQLDatabase.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "HAL/PlatformFileManager.h"
#include "K2Node/ESQLK2FieldFilterUtils.h"
#include "Misc/OutputDeviceNull.h"
#include "Misc/Paths.h"
#include "ScopedTransaction.h"
#include "Shared/ESQLId.h"
#include "Shared/ESQLSettings.h"
#include "Shared/ESQLTypes.h"
#include "Styling/AppStyle.h"
#include "TableAsset/ESQLTableAsset.h"
#include "UObject/PropertyPortFlags.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "SGraphPinESQLId"

TMap<FString, SGraphPinESQLId::FCachedQuery> SGraphPinESQLId::QueryCache;

void SGraphPinESQLId::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	RefreshPickerConfig();
	SGraphPin::Construct(SGraphPin::FArguments(), InGraphPinObj);
}

EVisibility SGraphPinESQLId::GetDefaultValueVisibility() const
{
	if (bOnlyShowDefaultValue)
	{
		return EVisibility::Visible;
	}

	UEdGraphPin* GraphPin = GetPinObj();
	if (!GraphPin || GraphPin->IsPendingKill() || GraphPin->Direction != EGPD_Input)
	{
		return EVisibility::Collapsed;
	}

	if (GraphPin->bNotConnectable && !GraphPin->bOrphanedPin)
	{
		return EVisibility::Visible;
	}

	return IsConnected() ? EVisibility::Collapsed : EVisibility::Visible;
}

TSharedRef<SWidget> SGraphPinESQLId::GetDefaultValueWidget()
{
	return SNew(SBox)
		.MinDesiredWidth(180.f)
		.Visibility(this, &SGraphPinESQLId::GetDefaultValueVisibility)
		[
			SAssignNew(ComboButton, SComboButton)
			.IsEnabled(this, &SGraphPinESQLId::IsPickerEnabled)
			.OnGetMenuContent(this, &SGraphPinESQLId::BuildPickerMenu)
			.ContentPadding(FMargin(8.f, 2.f))
			.ButtonContent()
			[
				SNew(STextBlock)
				.Text(this, &SGraphPinESQLId::GetButtonText)
				.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
		];
}

void SGraphPinESQLId::RefreshPickerConfig()
{
	bHasPickerConfig = TryResolvePickerConfig(PickerConfig);
}

void SGraphPinESQLId::ReloadOptions()
{
	RefreshPickerConfig();
	SourceEntries.Reset();
	AllOptions.Reset();
	FilteredOptions.Reset();
	SourceError.Reset();

	if (!bHasPickerConfig)
	{
		return;
	}

	if (!QueryEntries(PickerConfig, SourceEntries, SourceError))
	{
		return;
	}

	for (const FESQLIdGraphPinEntry& Entry : SourceEntries)
	{
		AllOptions.Add(MakeShared<FESQLIdGraphPinEntry>(Entry));
	}

	ApplyFilter();

	if (ListView.IsValid())
	{
		ListView->RequestListRefresh();
	}
	if (SearchBox.IsValid())
	{
		SearchBox->SetText(FText::GetEmpty());
	}
	SearchString.Reset();
}

void SGraphPinESQLId::ApplyFilter()
{
	FilteredOptions.Reset();

	for (const TSharedPtr<FESQLIdGraphPinEntry>& Entry : AllOptions)
	{
		if (!Entry.IsValid())
		{
			continue;
		}

		if (SearchString.IsEmpty()
			|| Entry->Id.Contains(SearchString, ESearchCase::IgnoreCase)
			|| Entry->Label.Contains(SearchString, ESearchCase::IgnoreCase))
		{
			FilteredOptions.Add(Entry);
		}
	}

	if (ListView.IsValid())
	{
		ListView->RequestListRefresh();
	}
}

bool SGraphPinESQLId::TryResolvePickerConfig(FPickerConfig& OutConfig) const
{
	OutConfig = FPickerConfig();

	FESQLId CurrentValue = GetCurrentValue();
	UESQLTableAsset* TableAsset = nullptr;

	if (GraphPinObj)
	{
		TableAsset = ESQLK2FieldFilterUtils::ResolveLiteralTableAsset(GraphPinObj->GetOwningNode(), TEXT("TableAsset"));
	}

	if (!TableAsset && !CurrentValue.SourceTable.IsNull())
	{
		TableAsset = CurrentValue.SourceTable.LoadSynchronous();
	}

	if (!TableAsset)
	{
		CachedTableAsset.Reset();
		return false;
	}

	CachedTableAsset = TableAsset;
	OutConfig.DatabaseName = TableAsset->DatabaseName;
	OutConfig.TableName = TableAsset->TableName;
	OutConfig.IdColumn = TableAsset->PrimaryKeyColumn;
	OutConfig.LabelColumn = TableAsset->DefaultLabelColumn;

	if (!CurrentValue.LabelColumn.IsEmpty())
	{
		OutConfig.LabelColumn = CurrentValue.LabelColumn;
	}

	if (OutConfig.IdColumn.IsEmpty())
	{
		OutConfig.IdColumn = TEXT("id");
	}
	if (OutConfig.LabelColumn.IsEmpty())
	{
		OutConfig.LabelColumn = OutConfig.IdColumn;
	}
	if (OutConfig.DatabaseName.IsEmpty())
	{
		OutConfig.DatabaseName = TEXT("EditorData");
	}

	return !OutConfig.TableName.IsEmpty();
}

bool SGraphPinESQLId::QueryEntries(const FPickerConfig& Config, TArray<FESQLIdGraphPinEntry>& OutEntries, FString& OutError)
{
	OutEntries.Reset();
	OutError.Reset();

	if (!IsSafeIdentifier(Config.TableName) || !IsSafeIdentifier(Config.IdColumn) || !IsSafeIdentifier(Config.LabelColumn))
	{
		OutError = TEXT("Invalid SQL identifier in picker configuration.");
		return false;
	}

	const FString DbPath = ResolveDatabasePath(Config.DatabaseName);
	if (DbPath.IsEmpty())
	{
		OutError = FString::Printf(TEXT("Database '%s' not found."), *Config.DatabaseName);
		return false;
	}

	const FString CacheKey = FString::Printf(TEXT("%s|%s|%s|%s"), *DbPath, *Config.TableName, *Config.IdColumn, *Config.LabelColumn);
	const FDateTime FileTimestamp = FPlatformFileManager::Get().GetPlatformFile().GetTimeStamp(*DbPath);

	if (const FCachedQuery* Cached = QueryCache.Find(CacheKey))
	{
		if (Cached->DbPath == DbPath && Cached->Timestamp == FileTimestamp)
		{
			OutEntries = Cached->Entries;
			return true;
		}
	}

	TSharedPtr<FESQLDatabase> Database;
	bool bOwnsConnection = false;

	if (CachedTableAsset.IsValid())
	{
		Database = CachedTableAsset->GetCachedDatabase();
	}

	if (!Database || !Database->IsOpen())
	{
		FString OpenError;
		Database = FESQLDatabase::OpenReadOnly(DbPath, OpenError);
		if (!Database)
		{
			OutError = FString::Printf(TEXT("Failed to open database: %s"), *OpenError);
			return false;
		}
		bOwnsConnection = true;
	}

	FESQLQueryResult TableCheck = Database->Execute(
		FString::Printf(TEXT("SELECT name FROM sqlite_master WHERE type='table' AND name='%s'"), *Config.TableName));
	if (!TableCheck.bSuccess || TableCheck.Rows.Num() == 0)
	{
		OutError = FString::Printf(TEXT("Table '%s' not found in database '%s'."), *Config.TableName, *Config.DatabaseName);
		if (bOwnsConnection)
		{
			Database->Close();
		}
		return false;
	}

	FString ResolvedLabelColumn = Config.LabelColumn;
	if (Config.IdColumn != Config.LabelColumn)
	{
		FESQLQueryResult TableInfo = Database->Execute(
			FString::Printf(TEXT("PRAGMA table_info(\"%s\")"), *Config.TableName));
		if (TableInfo.bSuccess)
		{
			bool bExactMatch = false;
			FString PrefixMatch;

			for (const FESQLRow& Row : TableInfo.Rows)
			{
				const FString ColumnName = Row.Columns.FindRef(TEXT("name"));
				if (ColumnName == Config.LabelColumn)
				{
					bExactMatch = true;
					break;
				}
				if (ColumnName.StartsWith(Config.LabelColumn + TEXT("_")) && PrefixMatch.IsEmpty())
				{
					PrefixMatch = ColumnName;
				}
			}

			if (!bExactMatch && !PrefixMatch.IsEmpty())
			{
				ResolvedLabelColumn = PrefixMatch;
			}
		}
	}

	FString Query;
	if (Config.IdColumn == ResolvedLabelColumn)
	{
		Query = FString::Printf(TEXT("SELECT \"%s\" FROM \"%s\" ORDER BY \"%s\""), *Config.IdColumn, *Config.TableName, *Config.IdColumn);
	}
	else
	{
		Query = FString::Printf(TEXT("SELECT \"%s\", \"%s\" FROM \"%s\" ORDER BY \"%s\""), *Config.IdColumn, *ResolvedLabelColumn, *Config.TableName, *Config.IdColumn);
	}

	FESQLQueryResult Result = Database->Execute(Query);
	if (bOwnsConnection)
	{
		Database->Close();
	}

	if (!Result.bSuccess)
	{
		OutError = FString::Printf(TEXT("Query failed: %s"), *Result.ErrorMessage);
		return false;
	}

	for (const FESQLRow& Row : Result.Rows)
	{
		FESQLIdGraphPinEntry Entry;
		Entry.Id = Row.Columns.FindRef(Config.IdColumn);
		Entry.Label = Row.Columns.FindRef(ResolvedLabelColumn);
		if (!Entry.Id.IsEmpty())
		{
			OutEntries.Add(MoveTemp(Entry));
		}
	}

	FCachedQuery& CachedResult = QueryCache.FindOrAdd(CacheKey);
	CachedResult.DbPath = DbPath;
	CachedResult.Timestamp = FileTimestamp;
	CachedResult.Entries = OutEntries;
	return true;
}

FString SGraphPinESQLId::ResolveDatabasePath(const FString& DatabaseName) const
{
	const FString FullPath = UESQLSettings::ResolveDatabaseFilePath(DatabaseName);
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (PlatformFile.FileExists(*FullPath))
	{
		return FullPath;
	}

	const FString ContentPath = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Database"), DatabaseName + TEXT(".db"));
	if (PlatformFile.FileExists(*ContentPath))
	{
		return ContentPath;
	}

	return FString();
}

bool SGraphPinESQLId::IsSafeIdentifier(const FString& Identifier)
{
	if (Identifier.IsEmpty())
	{
		return false;
	}

	for (const TCHAR Character : Identifier)
	{
		if (!FChar::IsAlnum(Character) && Character != TEXT('_'))
		{
			return false;
		}
	}

	return true;
}

FESQLId SGraphPinESQLId::GetCurrentValue() const
{
	FESQLId Result;
	if (!GraphPinObj)
	{
		return Result;
	}

	FOutputDeviceNull NullOutput;
	FESQLId::StaticStruct()->ImportText(*GraphPinObj->GetDefaultAsString(), &Result, nullptr, PPF_SerializedAsImportText, &NullOutput, GraphPinObj->PinName.ToString());
	return Result;
}

void SGraphPinESQLId::CommitValue(FESQLId NewValue)
{
	if (!GraphPinObj)
	{
		return;
	}

	if (CachedTableAsset.IsValid())
	{
		NewValue.SourceTable = CachedTableAsset.Get();
	}

	FString ExportText;
	FESQLId::StaticStruct()->ExportText(ExportText, &NewValue, &NewValue, nullptr, PPF_SerializedAsImportText, nullptr);
	if (ExportText == GraphPinObj->GetDefaultAsString())
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("ChangeSQLIdPinValue", "Change SQL Id Pin Value"));
	GraphPinObj->Modify();
	GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, ExportText);
	RefreshPickerConfig();
}

const FESQLIdGraphPinEntry* SGraphPinESQLId::FindEntryById(const FString& Id) const
{
	for (const FESQLIdGraphPinEntry& Entry : SourceEntries)
	{
		if (Entry.Id == Id)
		{
			return &Entry;
		}
	}
	return nullptr;
}

FText SGraphPinESQLId::GetButtonText() const
{
	const FESQLId CurrentValue = GetCurrentValue();
	if (CurrentValue.Value.IsEmpty())
	{
		return LOCTEXT("PickSqlId", "Pick SQL Id");
	}

	if (const FESQLIdGraphPinEntry* Entry = FindEntryById(CurrentValue.Value))
	{
		if (!Entry->Label.IsEmpty())
		{
			return FText::FromString(FString::Printf(TEXT("%s [%s]"), *Entry->Label, *Entry->Id));
		}
	}

	return FText::FromString(CurrentValue.Value);
}

FText SGraphPinESQLId::GetMenuStatusText() const
{
	if (!bHasPickerConfig)
	{
		return LOCTEXT("PickerNeedsLiteralTable", "Picker requires a literal Table Asset or a stored SourceTable.");
	}

	if (!SourceError.IsEmpty())
	{
		return FText::FromString(SourceError);
	}

	if (FilteredOptions.Num() == 0)
	{
		return SearchString.IsEmpty()
			? LOCTEXT("NoSqlIdsFound", "No SQL ids found.")
			: LOCTEXT("NoSqlIdsMatch", "No SQL ids match the current filter.");
	}

	return FText::GetEmpty();
}

bool SGraphPinESQLId::IsPickerEnabled() const
{
	FPickerConfig CurrentConfig;
	return TryResolvePickerConfig(CurrentConfig);
}

TSharedRef<SWidget> SGraphPinESQLId::BuildPickerMenu()
{
	ReloadOptions();

	return SNew(SBox)
		.MinDesiredWidth(360.f)
		.MaxDesiredHeight(420.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				[
					SAssignNew(SearchBox, SSearchBox)
					.HintText(LOCTEXT("SearchSqlIdHint", "Search SQL ids"))
					.OnTextChanged(this, &SGraphPinESQLId::OnSearchTextChanged)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4.f, 0.f, 0.f, 0.f)
				[
					SNew(SButton)
					.Text(LOCTEXT("RefreshSqlIds", "Refresh"))
					.OnClicked(this, &SGraphPinESQLId::OnRefreshClicked)
					.IsEnabled(bHasPickerConfig)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4.f, 0.f, 0.f, 0.f)
				[
					SNew(SButton)
					.Text(LOCTEXT("ClearSqlId", "Clear"))
					.OnClicked(this, &SGraphPinESQLId::OnClearClicked)
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 6.f, 0.f, 4.f)
			[
				SNew(STextBlock)
				.Text(this, &SGraphPinESQLId::GetMenuStatusText)
				.Visibility_Lambda([this]()
				{
					return GetMenuStatusText().IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
				})
				.ColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.3f, 0.3f, 1.f)))
				.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			[
				SAssignNew(ListView, SListView<TSharedPtr<FESQLIdGraphPinEntry>>)
				.ListItemsSource(&FilteredOptions)
				.OnGenerateRow(this, &SGraphPinESQLId::GenerateOptionRow)
				.OnSelectionChanged(this, &SGraphPinESQLId::OnOptionSelected)
				.SelectionMode(ESelectionMode::Single)
			]
		];
}

TSharedRef<ITableRow> SGraphPinESQLId::GenerateOptionRow(TSharedPtr<FESQLIdGraphPinEntry> Entry, const TSharedRef<STableViewBase>& OwnerTable) const
{
	const FString RowText = Entry.IsValid() && !Entry->Label.IsEmpty()
		? FString::Printf(TEXT("%s [%s]"), *Entry->Label, *Entry->Id)
		: (Entry.IsValid() ? Entry->Id : FString());

	return SNew(STableRow<TSharedPtr<FESQLIdGraphPinEntry>>, OwnerTable)
		[
			SNew(STextBlock)
			.Text(FText::FromString(RowText))
			.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		];
}

void SGraphPinESQLId::OnSearchTextChanged(const FText& NewText)
{
	SearchString = NewText.ToString();
	ApplyFilter();
}

void SGraphPinESQLId::OnOptionSelected(TSharedPtr<FESQLIdGraphPinEntry> SelectedEntry, ESelectInfo::Type SelectInfo)
{
	if (!SelectedEntry.IsValid() || SelectInfo == ESelectInfo::OnNavigation)
	{
		return;
	}

	FESQLId CurrentValue = GetCurrentValue();
	CurrentValue.Value = SelectedEntry->Id;
	CommitValue(MoveTemp(CurrentValue));

	if (ComboButton.IsValid())
	{
		ComboButton->SetIsOpen(false);
	}
}

FReply SGraphPinESQLId::OnRefreshClicked()
{
	QueryCache.Reset();
	ReloadOptions();
	return FReply::Handled();
}

FReply SGraphPinESQLId::OnClearClicked()
{
	FESQLId CurrentValue = GetCurrentValue();
	CurrentValue.Reset();
	CommitValue(MoveTemp(CurrentValue));
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE