// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "SqlId/ESQLIdCustomization.h"

#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "IPropertyUtilities.h"
#include "PropertyHandle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

#include "Shared/ESQLId.h"
#include "Shared/ESQLSettings.h"
#include "TableAsset/ESQLTableAsset.h"
#include "Core/ESQLDatabase.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "ESQLIdCustomization"

// ── Static cache ─────────────────────────────────────────────────────────────

TMap<FString, FESQLIdCustomization::FCachedQuery> FESQLIdCustomization::Cache;


// ── Factory ──────────────────────────────────────────────────────────────────

TSharedRef<IPropertyTypeCustomization> FESQLIdCustomization::MakeInstance()
{
	return MakeShared<FESQLIdCustomization>();
}


// ── Header ───────────────────────────────────────────────────────────────────

void FESQLIdCustomization::CustomizeHeader(
	TSharedRef<IPropertyHandle> PropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& Utils)
{
	StructHandle = PropertyHandle;
	ValueHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FESQLId, Value));
	LabelColumnHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FESQLId, LabelColumn));
	PropertyUtilities = Utils.GetPropertyUtilities();

	LoadMetadata(PropertyHandle);
	ReloadOptions();

	if (!bHasMetaTags)
	{
		if (TSharedPtr<IPropertyHandle> SourceTableHandle = PropertyHandle->GetChildHandle(
			GET_MEMBER_NAME_CHECKED(FESQLId, SourceTable)))
		{
			SourceTableHandle->SetOnPropertyValueChanged(
				FSimpleDelegate::CreateSP(this, &FESQLIdCustomization::OnPickerConfigChanged));
		}

		if (LabelColumnHandle.IsValid())
		{
			LabelColumnHandle->SetOnPropertyValueChanged(
				FSimpleDelegate::CreateSP(this, &FESQLIdCustomization::OnPickerConfigChanged));
		}
	}

	HeaderRow
		.NameContent()
		[
			PropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(420.f)
		.MaxDesiredWidth(720.f)
		[
			SNew(SVerticalBox)

			// Row 1: [TextBox] [Pick] [Refresh] [X]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SAssignNew(ValueTextBox, SEditableTextBox)
					.Text_Lambda([this]()
					{
						return FText::FromString(GetCurrentValue());
					})
					.OnTextCommitted(this, &FESQLIdCustomization::OnManualTextCommitted)
					.Font(IPropertyTypeCustomizationUtils::GetRegularFont())
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4.f, 0.f, 0.f, 0.f)
				[
					SNew(SComboButton)
					.OnGetMenuContent(this, &FESQLIdCustomization::BuildPickerMenu)
					.IsEnabled(bHasPickerConfig)
					.ButtonContent()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("PickBtn", "Pick"))
						.Font(IPropertyTypeCustomizationUtils::GetRegularFont())
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4.f, 0.f, 0.f, 0.f)
				[
					SNew(SButton)
					.Text(LOCTEXT("RefreshBtn", "\u21BB"))
					.ToolTipText(LOCTEXT("RefreshTip", "Reload entries from the database"))
					.OnClicked(this, &FESQLIdCustomization::OnRefreshClicked)
					.IsEnabled(bHasPickerConfig)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4.f, 0.f, 0.f, 0.f)
				[
					SNew(SButton)
					.Text(LOCTEXT("ClearBtn", "\u2715"))
					.ToolTipText(LOCTEXT("ClearTip", "Clear the current value"))
					.OnClicked(this, &FESQLIdCustomization::OnClearClicked)
				]
			]

			// Row 2: Resolved label (e.g. "Iron Sword [weapon_42]")
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 2.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(this, &FESQLIdCustomization::GetSelectionText)
				.ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.8f, 0.6f, 1.0f)))
				.Font(IPropertyTypeCustomizationUtils::GetRegularFont())
			]

			// Row 3: Status (error / unresolved warning)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 2.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(this, &FESQLIdCustomization::GetStatusText)
				.ColorAndOpacity(this, &FESQLIdCustomization::GetStatusColor)
				.Visibility(this, &FESQLIdCustomization::GetStatusVisibility)
				.Font(IPropertyTypeCustomizationUtils::GetRegularFont())
			]
		];
}

void FESQLIdCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> PropertyHandle,
	IDetailChildrenBuilder& ChildBuilder,
	IPropertyTypeCustomizationUtils& Utils)
{
	LoadMetadata(PropertyHandle);

	// If meta tags drive the picker, hide all children (compact C++ mode).
	if (bHasMetaTags)
	{
		return;
	}

	// Show the SourceTable property so Blueprint users can assign the table asset.
	// Hide Value — it's in the header picker.
	uint32 NumChildren = 0;
	PropertyHandle->GetNumChildren(NumChildren);
	for (uint32 i = 0; i < NumChildren; ++i)
	{
		TSharedPtr<IPropertyHandle> ChildHandle = PropertyHandle->GetChildHandle(i);
		if (!ChildHandle.IsValid()) continue;

		const FString ChildName = ChildHandle->GetProperty() ?
			ChildHandle->GetProperty()->GetName() : FString();

		// Skip Value — displayed in the header picker
		if (ChildName == GET_MEMBER_NAME_STRING_CHECKED(FESQLId, Value))
		{
			continue;
		}

		if (ChildName == GET_MEMBER_NAME_STRING_CHECKED(FESQLId, LabelColumn))
		{
			continue;
		}

		ChildBuilder.AddProperty(ChildHandle.ToSharedRef());
	}

	if (LabelColumnHandle.IsValid())
	{
		ChildBuilder.AddCustomRow(LabelColumnHandle->GetPropertyDisplayName())
			.NameContent()
			[
				LabelColumnHandle->CreatePropertyNameWidget()
			]
			.ValueContent()
			.MinDesiredWidth(250.f)
			.MaxDesiredWidth(450.f)
			[
				SAssignNew(LabelColumnComboBox, SComboBox<TSharedPtr<FString>>)
				.OptionsSource(&LabelColumnOptions)
				.IsEnabled(CachedTableAsset.IsValid())
				.OnGenerateWidget(this, &FESQLIdCustomization::GenerateLabelColumnOptionWidget)
				.OnSelectionChanged(this, &FESQLIdCustomization::OnLabelColumnSelected)
				.Content()
				[
					SNew(STextBlock)
					.Text(this, &FESQLIdCustomization::GetSelectedLabelColumnText)
					.Font(IPropertyTypeCustomizationUtils::GetRegularFont())
				]
			];
	}
}


// ── Metadata ─────────────────────────────────────────────────────────────────

void FESQLIdCustomization::LoadMetadata(TSharedRef<IPropertyHandle> PropertyHandle)
{
	CachedTableAsset.Reset();
	LabelColumnOptions.Reset();
	LabelColumnOverride.Reset();

	// ── Priority 1: C++ meta tags ────────────────────────────────────
	TableName = PropertyHandle->GetMetaData(TEXT("ESQLIdTable"));
	IdColumn = PropertyHandle->GetMetaData(TEXT("ESQLIdColumn"));
	LabelColumn = PropertyHandle->GetMetaData(TEXT("ESQLLabelColumn"));
	DatabaseName = PropertyHandle->GetMetaData(TEXT("ESQLDatabase"));

	bHasMetaTags = !TableName.IsEmpty();

	// ── Priority 2: SourceTable asset reference ──────────────────────
	if (!bHasMetaTags)
	{
		LabelColumnHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FESQLId, LabelColumn));
		if (LabelColumnHandle.IsValid())
		{
			LabelColumnHandle->GetValue(LabelColumnOverride);
		}

		TSharedPtr<IPropertyHandle> SourceTableHandle = PropertyHandle->GetChildHandle(
			GET_MEMBER_NAME_CHECKED(FESQLId, SourceTable));

		if (SourceTableHandle.IsValid())
		{
			// Try multiple approaches to read the soft object path

			// Approach 1: Read as formatted string (works for most cases)
			FString PathString;
			SourceTableHandle->GetValueAsFormattedString(PathString);

			// Clean up the path string — formatted string may include extra quotes/wrapper
			PathString.TrimStartAndEndInline();
			PathString.ReplaceInline(TEXT("\""), TEXT(""));

			// Approach 2: If formatted string failed, try reading through the struct data
			if (PathString.IsEmpty() || PathString == TEXT("None") || PathString == TEXT("()"))
			{
				// Access the raw struct data to read the TSoftObjectPtr directly
				TArray<void*> RawData;
				PropertyHandle->AccessRawData(RawData);
				for (void* Data : RawData)
				{
					if (Data)
					{
						const FESQLId* SqlId = static_cast<const FESQLId*>(Data);
						if (!SqlId->SourceTable.IsNull())
						{
							PathString = SqlId->SourceTable.ToSoftObjectPath().ToString();
							break;
						}
					}
				}
			}

			if (!PathString.IsEmpty() && PathString != TEXT("None"))
			{
				// Load the table asset to read its config
				UESQLTableAsset* Asset = Cast<UESQLTableAsset>(
					FSoftObjectPath(PathString).TryLoad());

				if (Asset)
				{
					DatabaseName = Asset->DatabaseName;
					TableName = Asset->TableName;
					IdColumn = Asset->PrimaryKeyColumn;
					LabelColumn = Asset->DefaultLabelColumn;
					CachedTableAsset = Asset;
				}
			}
		}

		if (!LabelColumnOverride.IsEmpty())
		{
			LabelColumn = LabelColumnOverride;
		}
	}

	// ── Apply defaults ───────────────────────────────────────────────
	if (IdColumn.IsEmpty())
	{
		IdColumn = TEXT("id");
	}
	if (LabelColumn.IsEmpty())
	{
		LabelColumn = IdColumn;
	}
	if (DatabaseName.IsEmpty())
	{
		DatabaseName = TEXT("EditorData");
	}

	bHasPickerConfig = !TableName.IsEmpty();
	RebuildLabelColumnOptions();
}

void FESQLIdCustomization::RebuildLabelColumnOptions()
{
	LabelColumnOptions.Reset();

	if (bHasMetaTags)
	{
		return;
	}

	LabelColumnOptions.Add(MakeShared<FString>(FString()));

	auto AddUniqueOption = [this](const FString& InOption)
	{
		if (InOption.IsEmpty())
		{
			return;
		}

		for (const TSharedPtr<FString>& Existing : LabelColumnOptions)
		{
			if (Existing.IsValid() && *Existing == InOption)
			{
				return;
			}
		}

		LabelColumnOptions.Add(MakeShared<FString>(InOption));
	};

	if (CachedTableAsset.IsValid())
	{
		for (const FString& Option : CachedTableAsset->GetLabelColumnOptions())
		{
			AddUniqueOption(Option);
		}
	}

	if (!LabelColumnOverride.IsEmpty())
	{
		AddUniqueOption(LabelColumnOverride);
	}
}

void FESQLIdCustomization::OnPickerConfigChanged()
{
	if (!StructHandle.IsValid())
	{
		return;
	}

	LoadMetadata(StructHandle.ToSharedRef());
	ReloadOptions();

	if (PropertyUtilities.IsValid())
	{
		PropertyUtilities->ForceRefresh();
	}
}


// ── Data Source ──────────────────────────────────────────────────────────────

void FESQLIdCustomization::ReloadOptions()
{
	SourceEntries.Reset();
	AllOptions.Reset();
	FilteredOptions.Reset();
	SourceError.Reset();

	if (!bHasPickerConfig)
	{
		// No config — no picker. This is fine for unconfigured FESQLId properties.
		return;
	}

	if (!QueryEntries(SourceEntries, SourceError))
	{
		return;
	}

	for (const FESQLIdEntry& Entry : SourceEntries)
	{
		AllOptions.Add(MakeShared<FESQLIdEntry>(Entry));
	}

	ApplyFilter();
}

bool FESQLIdCustomization::QueryEntries(
	TArray<FESQLIdEntry>& OutEntries, FString& OutError)
{
	OutEntries.Reset();
	OutError.Reset();

	if (!IsSafeIdentifier(TableName) || !IsSafeIdentifier(IdColumn))
	{
		OutError = TEXT("Invalid SQL identifier in configuration.");
		return false;
	}

	const FString DbPath = ResolveDatabasePath();
	if (DbPath.IsEmpty())
	{
		OutError = FString::Printf(TEXT("Database '%s' not found. Expected: Saved/Databases/%s.db"),
			*DatabaseName, *DatabaseName);
		return false;
	}

	// Check cache
	const FString CacheKey = FString::Printf(TEXT("%s|%s|%s|%s"),
		*DbPath, *TableName, *IdColumn, *LabelColumn);
	const FDateTime FileTimestamp = FPlatformFileManager::Get().GetPlatformFile().GetTimeStamp(*DbPath);

	if (const FCachedQuery* Cached = Cache.Find(CacheKey))
	{
		if (Cached->DbPath == DbPath && Cached->Timestamp == FileTimestamp)
		{
			OutEntries = Cached->Entries;
			return true;
		}
	}

	// Open a database connection for reading.
	// The VFS now uses FILE_SHARE_READ|WRITE on Windows, so concurrent access works.
	TSharedPtr<FESQLDatabase> Db;
	bool bOwnedConnection = false;

	// Strategy 1: Reuse the table asset's existing connection (zero cost)
	if (CachedTableAsset.IsValid())
	{
		Db = CachedTableAsset->GetCachedDatabase();
	}

	// Strategy 2: Open our own read-only connection
	if (!Db || !Db->IsOpen())
	{
		FString DbError;
		Db = FESQLDatabase::OpenReadOnly(DbPath, DbError);
		if (!Db)
		{
			OutError = FString::Printf(TEXT("Failed to open database: %s"), *DbError);
			return false;
		}
		bOwnedConnection = true;
	}

	// Verify the table exists
	FESQLQueryResult TableCheck = Db->Execute(
		FString::Printf(TEXT("SELECT name FROM sqlite_master WHERE type='table' AND name='%s'"), *TableName));
	if (!TableCheck.bSuccess || TableCheck.Rows.Num() == 0)
	{
		OutError = FString::Printf(TEXT("Table '%s' not found in database '%s'"),
			*TableName, *DatabaseName);
		if (bOwnedConnection) { Db->Close(); }
		return false;
	}

	// Resolve the actual SQL column name for the LabelColumn.
	// The user sets LabelColumn as an authored name (e.g. "PlayerName") but
	// Blueprint UserDefinedStruct fields store as GUID-suffixed names in SQLite
	// (e.g. "PlayerName_6_SET2E356E..."). Match by prefix.
	FString ResolvedLabelCol = LabelColumn;
	if (IdColumn != LabelColumn && IsSafeIdentifier(LabelColumn))
	{
		FESQLQueryResult PragmaResult = Db->Execute(
			FString::Printf(TEXT("PRAGMA table_info(\"%s\")"), *TableName));
		if (PragmaResult.bSuccess)
		{
			bool bExactMatch = false;
			FString PrefixMatch;

			for (const FESQLRow& Row : PragmaResult.Rows)
			{
				const FString ColName = Row.Columns.FindRef(TEXT("name"));
				if (ColName == LabelColumn)
				{
					bExactMatch = true;
					break;
				}
				// Check if this column starts with the authored name followed by "_"
				// e.g. "PlayerName_6_SET2E..." starts with "PlayerName_"
				if (ColName.StartsWith(LabelColumn + TEXT("_")) && PrefixMatch.IsEmpty())
				{
					PrefixMatch = ColName;
				}
			}

			if (!bExactMatch && !PrefixMatch.IsEmpty())
			{
				ResolvedLabelCol = PrefixMatch;
			}
		}
	}

	// Query entries
	FString SQL;
	if (IdColumn == ResolvedLabelCol || IdColumn == LabelColumn)
	{
		SQL = FString::Printf(TEXT("SELECT \"%s\" FROM \"%s\" ORDER BY \"%s\""),
			*IdColumn, *TableName, *IdColumn);
	}
	else
	{
		SQL = FString::Printf(TEXT("SELECT \"%s\", \"%s\" FROM \"%s\" ORDER BY \"%s\""),
			*IdColumn, *ResolvedLabelCol, *TableName, *IdColumn);
	}

	FESQLQueryResult Result = Db->Execute(SQL);
	if (bOwnedConnection) { Db->Close(); }

	if (!Result.bSuccess)
	{
		OutError = FString::Printf(TEXT("Query failed: %s"), *Result.ErrorMessage);
		return false;
	}

	for (const FESQLRow& Row : Result.Rows)
	{
		FESQLIdEntry Entry;
		Entry.Id = Row.Columns.FindRef(IdColumn);
		Entry.Label = Row.Columns.FindRef(ResolvedLabelCol);

		if (!Entry.Id.IsEmpty())
		{
			OutEntries.Add(MoveTemp(Entry));
		}
	}

	// Update cache
	FCachedQuery& CachedResult = Cache.FindOrAdd(CacheKey);
	CachedResult.DbPath = DbPath;
	CachedResult.Timestamp = FileTimestamp;
	CachedResult.Entries = OutEntries;

	return true;
}

FString FESQLIdCustomization::ResolveDatabasePath() const
{
	const FString FullPath = UESQLSettings::ResolveDatabaseFilePath(DatabaseName);

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (PlatformFile.FileExists(*FullPath))
	{
		return FullPath;
	}

	// Also check Content/Database/ for packaged databases
	const FString ContentPath = FPaths::Combine(
		FPaths::ProjectContentDir(), TEXT("Database"), DatabaseName + TEXT(".db"));
	if (PlatformFile.FileExists(*ContentPath))
	{
		return ContentPath;
	}

	return FString();
}

bool FESQLIdCustomization::IsSafeIdentifier(const FString& Id)
{
	if (Id.IsEmpty()) return false;
	for (const TCHAR Ch : Id)
	{
		if (!FChar::IsAlnum(Ch) && Ch != TEXT('_'))
		{
			return false;
		}
	}
	return true;
}


// ── Filtering ────────────────────────────────────────────────────────────────

void FESQLIdCustomization::ApplyFilter()
{
	FilteredOptions.Reset();

	for (const TSharedPtr<FESQLIdEntry>& Entry : AllOptions)
	{
		if (!Entry.IsValid()) continue;

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


// ── Value Access ─────────────────────────────────────────────────────────────

void FESQLIdCustomization::SetCurrentValue(const FString& NewValue) const
{
	if (ValueHandle.IsValid())
	{
		ValueHandle->SetValue(NewValue);
	}
	if (PropertyUtilities.IsValid())
	{
		PropertyUtilities->ForceRefresh();
	}
}

FString FESQLIdCustomization::GetCurrentValue() const
{
	if (!ValueHandle.IsValid()) return FString();
	FString CurrentValue;
	ValueHandle->GetValue(CurrentValue);
	return CurrentValue;
}

const FESQLIdEntry* FESQLIdCustomization::FindEntryById(const FString& Id) const
{
	for (const FESQLIdEntry& Entry : SourceEntries)
	{
		if (Entry.Id == Id)
		{
			return &Entry;
		}
	}
	return nullptr;
}

bool FESQLIdCustomization::IsCurrentValueResolved() const
{
	const FString CurrentValue = GetCurrentValue();
	return CurrentValue.IsEmpty() || FindEntryById(CurrentValue) != nullptr;
}


// ── UI Text ──────────────────────────────────────────────────────────────────

FText FESQLIdCustomization::GetSelectionText() const
{
	if (!bHasPickerConfig) return FText::GetEmpty();
	if (!SourceError.IsEmpty()) return FText::GetEmpty();

	const FString CurrentValue = GetCurrentValue();
	if (CurrentValue.IsEmpty())
	{
		return LOCTEXT("NoIdSelected", "No id selected.");
	}

	if (const FESQLIdEntry* Entry = FindEntryById(CurrentValue))
	{
		if (!Entry->Label.IsEmpty() && Entry->Label != Entry->Id)
		{
			return FText::FromString(FString::Printf(TEXT("%s [%s]"), *Entry->Label, *Entry->Id));
		}
	}

	return FText::FromString(CurrentValue);
}

FText FESQLIdCustomization::GetStatusText() const
{
	if (!SourceError.IsEmpty())
	{
		return FText::FromString(SourceError);
	}

	if (!bHasPickerConfig) return FText::GetEmpty();

	const FString CurrentValue = GetCurrentValue();
	if (!CurrentValue.IsEmpty() && !IsCurrentValueResolved())
	{
		return FText::FromString(FString::Printf(
			TEXT("Unresolved: '%s' not found in %s.%s"),
			*CurrentValue, *TableName, *IdColumn));
	}

	return FText::GetEmpty();
}

FSlateColor FESQLIdCustomization::GetStatusColor() const
{
	if (!SourceError.IsEmpty())
	{
		return FLinearColor(0.9f, 0.3f, 0.3f, 1.0f);
	}
	return FLinearColor(0.85f, 0.65f, 0.25f, 1.0f);
}

EVisibility FESQLIdCustomization::GetStatusVisibility() const
{
	return GetStatusText().IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
}

FText FESQLIdCustomization::GetMatchCountText() const
{
	if (SearchString.IsEmpty())
	{
		return FText::FromString(FString::Printf(TEXT("%d entries"), AllOptions.Num()));
	}
	return FText::FromString(FString::Printf(TEXT("%d of %d matching"),
		FilteredOptions.Num(), AllOptions.Num()));
}

FText FESQLIdCustomization::GetSelectedLabelColumnText() const
{
	if (!LabelColumnHandle.IsValid())
	{
		return FText::FromString(LabelColumn);
	}

	FString CurrentOverride;
	LabelColumnHandle->GetValue(CurrentOverride);
	if (!CurrentOverride.IsEmpty())
	{
		return FText::FromString(CurrentOverride);
	}

	const FString DefaultColumn = !LabelColumn.IsEmpty() ? LabelColumn : IdColumn;
	if (DefaultColumn.IsEmpty())
	{
		return LOCTEXT("DefaultLabelColumnEmpty", "Default");
	}

	return FText::FromString(FString::Printf(TEXT("Default (%s)"), *DefaultColumn));
}

FText FESQLIdCustomization::GetLabelColumnOptionText(TSharedPtr<FString> InItem) const
{
	if (!InItem.IsValid() || InItem->IsEmpty())
	{
		const FString DefaultColumn = !CachedTableAsset.IsValid()
			? LabelColumn
			: (!CachedTableAsset->DefaultLabelColumn.IsEmpty() ? CachedTableAsset->DefaultLabelColumn : IdColumn);

		if (DefaultColumn.IsEmpty())
		{
			return LOCTEXT("DefaultLabelColumnOption", "Default");
		}

		return FText::FromString(FString::Printf(TEXT("Default (%s)"), *DefaultColumn));
	}

	return FText::FromString(*InItem);
}


// ── Picker Popup ─────────────────────────────────────────────────────────────

TSharedRef<SWidget> FESQLIdCustomization::BuildPickerMenu()
{
	ReloadOptions();

	TSharedRef<SWidget> PickerWidget = SNew(SBox)
		.WidthOverride(380.f)
		.HeightOverride(420.f)
		[
			SNew(SBorder)
			.Padding(8.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SAssignNew(SearchBox, SSearchBox)
					.OnTextChanged(this, &FESQLIdCustomization::OnSearchTextChanged)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 6.f, 0.f, 2.f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(FString::Printf(TEXT("Select from %s.%s using %s:"), *TableName, *IdColumn, *(LabelColumn.IsEmpty() ? IdColumn : LabelColumn))))
					.ColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f, 1.0f)))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 0.f, 0.f, 4.f)
				[
					SNew(STextBlock)
					.Text_Lambda([this]() { return GetMatchCountText(); })
					.ColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f, 1.0f)))
				]
				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				[
					SAssignNew(ListView, SListView<TSharedPtr<FESQLIdEntry>>)
					.ListItemsSource(&FilteredOptions)
					.OnGenerateRow(this, &FESQLIdCustomization::GenerateOptionRow)
					.OnSelectionChanged(this, &FESQLIdCustomization::OnOptionSelected)
				]
			]
		];

	ScrollToCurrentSelection();
	return PickerWidget;
}

TSharedRef<SWidget> FESQLIdCustomization::GenerateLabelColumnOptionWidget(
	TSharedPtr<FString> InItem) const
{
	return SNew(STextBlock)
		.Text(GetLabelColumnOptionText(InItem));
}

TSharedRef<ITableRow> FESQLIdCustomization::GenerateOptionRow(
	TSharedPtr<FESQLIdEntry> InEntry,
	const TSharedRef<STableViewBase>& OwnerTable) const
{
	const FString Label = InEntry.IsValid() ? InEntry->Label : FString();
	const FString Id = InEntry.IsValid() ? InEntry->Id : FString();

	// Find index for row numbering
	int32 RowIndex = INDEX_NONE;
	for (int32 i = 0; i < FilteredOptions.Num(); ++i)
	{
		if (FilteredOptions[i] == InEntry)
		{
			RowIndex = i;
			break;
		}
	}

	FString DisplayText;
	if (RowIndex != INDEX_NONE)
	{
		if (!Label.IsEmpty() && Label != Id)
		{
			DisplayText = FString::Printf(TEXT("%d. %s  (%s)"), RowIndex + 1, *Label, *Id);
		}
		else
		{
			DisplayText = FString::Printf(TEXT("%d. %s"), RowIndex + 1, Id.IsEmpty() ? *Label : *Id);
		}
	}
	else
	{
		DisplayText = Label.IsEmpty() ? Id : Label;
	}

	return SNew(STableRow<TSharedPtr<FESQLIdEntry>>, OwnerTable)
		[
			SNew(STextBlock)
			.Text(FText::FromString(DisplayText))
		];
}


// ── Interaction ──────────────────────────────────────────────────────────────

void FESQLIdCustomization::OnSearchTextChanged(const FText& NewText)
{
	SearchString = NewText.ToString();
	ApplyFilter();
}

void FESQLIdCustomization::OnOptionSelected(
	TSharedPtr<FESQLIdEntry> SelectedEntry, ESelectInfo::Type SelectInfo)
{
	if (SelectInfo == ESelectInfo::OnNavigation || SelectInfo == ESelectInfo::Direct)
	{
		return;
	}

	if (!SelectedEntry.IsValid()) return;

	SetCurrentValue(SelectedEntry->Id);
	if (ValueTextBox.IsValid())
	{
		ValueTextBox->SetText(FText::FromString(SelectedEntry->Id));
	}
}

void FESQLIdCustomization::OnLabelColumnSelected(
	TSharedPtr<FString> SelectedEntry,
	ESelectInfo::Type SelectInfo)
{
	if (!LabelColumnHandle.IsValid() || !SelectedEntry.IsValid())
	{
		return;
	}

	if (SelectInfo == ESelectInfo::OnNavigation || SelectInfo == ESelectInfo::Direct)
	{
		return;
	}

	LabelColumnHandle->SetValue(*SelectedEntry);
	OnPickerConfigChanged();
}

FReply FESQLIdCustomization::OnRefreshClicked()
{
	// Clear cache for this database
	TArray<FString> KeysToRemove;
	for (const auto& Pair : Cache)
	{
		if (Pair.Key.Contains(DatabaseName))
		{
			KeysToRemove.Add(Pair.Key);
		}
	}
	for (const FString& Key : KeysToRemove)
	{
		Cache.Remove(Key);
	}

	ReloadOptions();
	return FReply::Handled();
}

FReply FESQLIdCustomization::OnClearClicked()
{
	SetCurrentValue(FString());
	if (ValueTextBox.IsValid())
	{
		ValueTextBox->SetText(FText::GetEmpty());
	}
	return FReply::Handled();
}

void FESQLIdCustomization::OnManualTextCommitted(
	const FText& NewText, ETextCommit::Type CommitType) const
{
	SetCurrentValue(NewText.ToString().TrimStartAndEnd());
}

void FESQLIdCustomization::ScrollToCurrentSelection()
{
	if (!ListView.IsValid()) return;

	const FString CurrentValue = GetCurrentValue();
	if (CurrentValue.IsEmpty()) return;

	for (const TSharedPtr<FESQLIdEntry>& Entry : FilteredOptions)
	{
		if (Entry.IsValid() && Entry->Id == CurrentValue)
		{
			ListView->SetSelection(Entry, ESelectInfo::OnNavigation);
			ListView->RequestScrollIntoView(Entry);
			break;
		}
	}
}

#undef LOCTEXT_NAMESPACE
