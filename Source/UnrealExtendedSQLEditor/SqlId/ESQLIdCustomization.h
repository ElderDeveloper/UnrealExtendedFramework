// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "IPropertyTypeCustomization.h"

class UESQLTableAsset;
class IPropertyHandle;
class IPropertyUtilities;
class SEditableTextBox;
class SSearchBox;
class STableViewBase;
class ITableRow;
template<typename OptionType> class SComboBox;
template<typename ItemType> class SListView;

/**
 * Entry in the SQL id picker dropdown.
 */
struct FESQLIdEntry
{
	FString Id;
	FString Label;
};

/**
 * Property customization for FESQLId.
 * Shows a text box with a searchable picker popup that reads
 * live data from a SQLite database.
 *
 * Configuration priority:
 *   1. C++ meta tags: ESQLIdTable, ESQLIdColumn, ESQLLabelColumn, ESQLDatabase
 *   2. FESQLId.LabelColumn override (Blueprint/editor)
 *   3. SourceTable asset reference: reads DatabaseName, TableName,
 *      PrimaryKeyColumn, DefaultLabelColumn from UESQLTableAsset
 *   3. Fallback: plain text box (no picker)
 */
class FESQLIdCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle,
		FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& Utils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle,
		IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& Utils) override;

private:
	// ── Metadata ─────────────────────────────────────────────────────
	void LoadMetadata(TSharedRef<IPropertyHandle> PropertyHandle);
	void RebuildLabelColumnOptions();
	void OnPickerConfigChanged();
	FString TableName;
	FString IdColumn;
	FString LabelColumn;
	FString LabelColumnOverride;
	FString DatabaseName;
	bool bHasMetaTags = false;
	bool bHasPickerConfig = false;
	TWeakObjectPtr<UESQLTableAsset> CachedTableAsset;

	// ── Data Source ──────────────────────────────────────────────────
	void ReloadOptions();
	void ApplyFilter();
	bool QueryEntries(TArray<FESQLIdEntry>& OutEntries, FString& OutError);
	FString ResolveDatabasePath() const;
	static bool IsSafeIdentifier(const FString& Id);

	TArray<FESQLIdEntry> SourceEntries;
	TArray<TSharedPtr<FESQLIdEntry>> AllOptions;
	TArray<TSharedPtr<FESQLIdEntry>> FilteredOptions;
	FString SourceError;

	// ── Cache ────────────────────────────────────────────────────────
	struct FCachedQuery
	{
		FString DbPath;
		FDateTime Timestamp;
		TArray<FESQLIdEntry> Entries;
	};
	static TMap<FString, FCachedQuery> Cache;

	// ── Value access ─────────────────────────────────────────────────
	void SetCurrentValue(const FString& NewValue) const;
	FString GetCurrentValue() const;
	const FESQLIdEntry* FindEntryById(const FString& Id) const;
	bool IsCurrentValueResolved() const;

	// ── UI ───────────────────────────────────────────────────────────
	FText GetSelectionText() const;
	FText GetStatusText() const;
	FSlateColor GetStatusColor() const;
	EVisibility GetStatusVisibility() const;
	FText GetMatchCountText() const;
	FText GetSelectedLabelColumnText() const;
	FText GetLabelColumnOptionText(TSharedPtr<FString> InItem) const;
	TSharedRef<SWidget> BuildPickerMenu();
	TSharedRef<SWidget> GenerateLabelColumnOptionWidget(TSharedPtr<FString> InItem) const;
	TSharedRef<ITableRow> GenerateOptionRow(TSharedPtr<FESQLIdEntry> InEntry,
		const TSharedRef<STableViewBase>& OwnerTable) const;
	void OnOptionSelected(TSharedPtr<FESQLIdEntry> SelectedEntry, ESelectInfo::Type SelectInfo);
	void OnLabelColumnSelected(TSharedPtr<FString> SelectedEntry, ESelectInfo::Type SelectInfo);
	void OnSearchTextChanged(const FText& NewText);
	void OnManualTextCommitted(const FText& NewText, ETextCommit::Type CommitType) const;
	FReply OnRefreshClicked();
	FReply OnClearClicked();
	void ScrollToCurrentSelection();

	TSharedPtr<IPropertyHandle> StructHandle;
	TSharedPtr<IPropertyHandle> ValueHandle;
	TSharedPtr<IPropertyHandle> LabelColumnHandle;
	TSharedPtr<IPropertyUtilities> PropertyUtilities;
	TSharedPtr<SEditableTextBox> ValueTextBox;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> LabelColumnComboBox;
	TSharedPtr<SSearchBox> SearchBox;
	TSharedPtr<SListView<TSharedPtr<FESQLIdEntry>>> ListView;
	TArray<TSharedPtr<FString>> LabelColumnOptions;
	FString SearchString;
};
