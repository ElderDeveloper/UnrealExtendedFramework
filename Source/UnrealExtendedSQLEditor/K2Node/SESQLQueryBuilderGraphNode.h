// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "SGraphNode.h"

class UEdGraphPin;

class SESQLQueryBuilderGraphNode : public SGraphNode
{
public:
	SLATE_BEGIN_ARGS(SESQLQueryBuilderGraphNode) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphNode* InNode);

	virtual void UpdateGraphNode() override;

private:
	struct FFilterPinSet
	{
		int32 Index = INDEX_NONE;
		UEdGraphPin* FieldPin = nullptr;
		UEdGraphPin* OperationPin = nullptr;
		UEdGraphPin* ValuePin = nullptr;
		UEdGraphPin* ValuesPin = nullptr;
	};

	struct FSortPinSet
	{
		int32 Index = INDEX_NONE;
		UEdGraphPin* FieldPin = nullptr;
		UEdGraphPin* AscendingPin = nullptr;
	};

	TSharedRef<SWidget> CreateNodeBody();
	TSharedRef<SWidget> CreateHeaderWidget() const;
	TSharedRef<SWidget> CreateTableAssetSection();
	TSharedRef<SWidget> CreateFiltersSection();
	TSharedRef<SWidget> CreateSortSection();
	TSharedRef<SWidget> CreateWindowSection();
	TSharedRef<SWidget> CreateFilterClauseCard(const FFilterPinSet& PinSet);
	TSharedRef<SWidget> CreateSortClauseCard(const FSortPinSet& PinSet);
	TSharedRef<SWidget> CreateInlinePinWidget(UEdGraphPin* Pin);
	void PopulateStandardPins();

	TArray<FFilterPinSet> CollectFilterPins() const;
	TArray<FSortPinSet> CollectSortPins() const;
	FText GetTableSubtitleText() const;
	EVisibility GetFilterValueVisibility(UEdGraphPin* OperationPin, UEdGraphPin* ValuePin) const;
	EVisibility GetFilterValuesVisibility(UEdGraphPin* OperationPin, UEdGraphPin* ValuesPin) const;
	bool ShouldInlinePin(const UEdGraphPin* Pin) const;
	bool CanAddFilterClause() const;
	bool CanAddSortClause() const;

	FReply HandleAddFilterClause() const;
	FReply HandleAddSortClause() const;
	FReply HandleRemoveClause(UEdGraphPin* Pin) const;
};