// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "K2Node.h"
#include "K2Node_AddPinInterface.h"
#include "ESQLQueryBuilderK2Nodes.generated.h"

class FKismetCompilerContext;
class UEdGraph;
class UEdGraphPin;
class UGraphNodeContextMenuContext;
class UToolMenu;

USTRUCT()
struct UNREALEXTENDEDSQLEDITOR_API FESQLQueryClauseUiState
{
	GENERATED_BODY()

	UPROPERTY()
	int32 NumFilterClauses = 1;

	UPROPERTY()
	int32 NumSortClauses = 0;
};

class UNREALEXTENDEDSQLEDITOR_API FESQLQueryClauseUiBase
{
protected:
	virtual UK2Node* GetQueryClauseNode() = 0;
	virtual const UK2Node* GetQueryClauseNode() const = 0;
	virtual FESQLQueryClauseUiState& GetQueryClauseUiState() = 0;
	virtual const FESQLQueryClauseUiState& GetQueryClauseUiState() const = 0;
	virtual FName GetQueryClauseTableAssetPinName() const;

public:
	enum class EClausePinKind : uint8
	{
		None,
		FilterField,
		FilterOperation,
		FilterValue,
		FilterValues,
		SortField,
		SortAscending,
	};

	static constexpr int32 MaxClauseCount = 8;

	static FName GetTableAssetPinName();
	static FName GetLimitPinName();
	static FName GetOffsetPinName();
	static EClausePinKind DescribeClausePinName(FName PinName, int32& OutIndex);
	static bool UsesMultiValuePins(const UEdGraphPin* OperationPin);
	static bool IsValuePinName(const FName PinName);
	static bool IsValuesPinName(const FName PinName);

	void AllocateQueryClausePins();
	void RefreshQueryClausePinTypes() const;
	void ValidateQueryClausePins(class FCompilerResultsLog& MessageLog) const;
	void AddFilterClause();
	void AddSortClause();
	void RemoveClauseForPin(UEdGraphPin* Pin);
	void GetQueryClauseContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const;
	UEdGraphPin* BuildQuerySpecPin(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph);

	static bool IsFieldNamePinName(const FName PinName);
	static bool IsQueryClausePinName(const FName PinName);

protected:
	UEdGraphPin* FindLimitPin() const;
	UEdGraphPin* FindOffsetPin() const;

private:
	static FName MakeFilterFieldPinName(int32 Index);
	static FName MakeFilterOperationPinName(int32 Index);
	static FName MakeFilterValuePinName(int32 Index);
	static FName MakeFilterValuesPinName(int32 Index);
	static FName MakeSortFieldPinName(int32 Index);
	static FName MakeSortAscendingPinName(int32 Index);
	static EClausePinKind ParseClausePinName(const FName PinName, int32& OutIndex);

	void AddClausePinsForIndex(int32 Index);
	void AddSortPinsForIndex(int32 Index);
	void ReconstructClauseNode(const FText& TransactionText);
	void SetPinToolTip(UEdGraphPin& Pin, const FText& Description) const;
	UEdGraphPin* FindPinChecked(FName PinName) const;
	UEdGraphPin* FindPin(FName PinName) const;
	UEdGraphPin* BuildFilterArrayPin(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph);
	UEdGraphPin* BuildSortArrayPin(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph);
	UEdGraphPin* BuildFilterPin(int32 Index, FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph);
	UEdGraphPin* BuildSortPin(int32 Index, FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph);
};

UCLASS(meta = (ESQLHideFromBlueprintPalette = "true"))
class UNREALEXTENDEDSQLEDITOR_API UK2Node_ESQLMakeQuerySpec : public UK2Node, public IK2Node_AddPinInterface, public FESQLQueryClauseUiBase
{
	GENERATED_BODY()

public:
	virtual void AllocateDefaultPins() override;
	virtual bool IsNodePure() const override { return true; }
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual void ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual void PinDefaultValueChanged(UEdGraphPin* Pin) override;
	virtual void NotifyPinConnectionListChanged(UEdGraphPin* Pin) override;
	virtual void PostReconstructNode() override;
	virtual void GetMenuActions(class FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual FText GetMenuCategory() const override;
	virtual void GetNodeContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const override;
	virtual bool IncludeParentNodeContextMenu() const override { return true; }
	virtual void EarlyValidation(class FCompilerResultsLog& MessageLog) const override;

	virtual void AddInputPin() override;
	virtual bool CanAddPin() const override;
	virtual void RemoveInputPin(UEdGraphPin* Pin) override;
	virtual bool CanRemovePin(const UEdGraphPin* Pin) const override;

protected:
	virtual UK2Node* GetQueryClauseNode() override { return this; }
	virtual const UK2Node* GetQueryClauseNode() const override { return this; }
	virtual FESQLQueryClauseUiState& GetQueryClauseUiState() override { return ClauseUiState; }
	virtual const FESQLQueryClauseUiState& GetQueryClauseUiState() const override { return ClauseUiState; }

private:
	UEdGraphPin* GetTableAssetPin() const;
	UEdGraphPin* GetQuerySpecPin() const;

	UPROPERTY()
	FESQLQueryClauseUiState ClauseUiState;
};