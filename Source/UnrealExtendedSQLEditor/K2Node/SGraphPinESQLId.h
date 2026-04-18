// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "SGraphPin.h"

class SComboButton;
class SSearchBox;
class STableViewBase;
class UESQLTableAsset;
struct FESQLId;
struct FESQLIdGraphPinEntry;
class ITableRow;
template<typename ItemType> class SListView;

class UNREALEXTENDEDSQLEDITOR_API SGraphPinESQLId : public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SGraphPinESQLId) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj);

protected:
	virtual EVisibility GetDefaultValueVisibility() const override;
	virtual TSharedRef<SWidget> GetDefaultValueWidget() override;

private:
	struct FPickerConfig
	{
		FString DatabaseName;
		FString TableName;
		FString IdColumn;
		FString LabelColumn;
	};

	struct FCachedQuery
	{
		FString DbPath;
		FDateTime Timestamp;
		TArray<FESQLIdGraphPinEntry> Entries;
	};

	void RefreshPickerConfig();
	void ReloadOptions();
	void ApplyFilter();
	bool TryResolvePickerConfig(FPickerConfig& OutConfig) const;
	bool QueryEntries(const FPickerConfig& Config, TArray<FESQLIdGraphPinEntry>& OutEntries, FString& OutError);
	FString ResolveDatabasePath(const FString& DatabaseName) const;
	static bool IsSafeIdentifier(const FString& Identifier);

	FESQLId GetCurrentValue() const;
	void CommitValue(FESQLId NewValue);
	const FESQLIdGraphPinEntry* FindEntryById(const FString& Id) const;

	FText GetButtonText() const;
	FText GetMenuStatusText() const;
	bool IsPickerEnabled() const;
	TSharedRef<SWidget> BuildPickerMenu();
	TSharedRef<ITableRow> GenerateOptionRow(TSharedPtr<FESQLIdGraphPinEntry> Entry, const TSharedRef<STableViewBase>& OwnerTable) const;
	void OnSearchTextChanged(const FText& NewText);
	void OnOptionSelected(TSharedPtr<FESQLIdGraphPinEntry> SelectedEntry, ESelectInfo::Type SelectInfo);
	FReply OnRefreshClicked();
	FReply OnClearClicked();

	static TMap<FString, FCachedQuery> QueryCache;

	FPickerConfig PickerConfig;
	bool bHasPickerConfig = false;
	mutable TWeakObjectPtr<UESQLTableAsset> CachedTableAsset;
	TArray<FESQLIdGraphPinEntry> SourceEntries;
	TArray<TSharedPtr<FESQLIdGraphPinEntry>> AllOptions;
	TArray<TSharedPtr<FESQLIdGraphPinEntry>> FilteredOptions;
	FString SourceError;
	FString SearchString;
	TSharedPtr<SComboButton> ComboButton;
	TSharedPtr<SSearchBox> SearchBox;
	TSharedPtr<SListView<TSharedPtr<FESQLIdGraphPinEntry>>> ListView;
};

struct FESQLIdGraphPinEntry
{
	FString Id;
	FString Label;
};