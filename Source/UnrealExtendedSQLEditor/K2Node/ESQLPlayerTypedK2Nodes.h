// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "K2Node.h"
#include "K2Node/ESQLQueryBuilderK2Nodes.h"
#include "K2Node_AddPinInterface.h"
#include "ESQLPlayerTypedK2Nodes.generated.h"

class FBlueprintActionDatabaseRegistrar;
class FKismetCompilerContext;
class UEdGraph;
class UEdGraphPin;
class UGraphNodeContextMenuContext;
class UToolMenu;
class UK2Node_CallFunction;
class UESQLPlayerDBComponent;
class UESQLTableAsset;
struct FLinearColor;

UCLASS(Abstract)
class UNREALEXTENDEDSQLEDITOR_API UK2Node_ESQLPlayerTypedRowBridgeBase : public UK2Node
{
	GENERATED_BODY()

public:
	virtual void AllocateDefaultPins() override;
	virtual bool CanSplitPin(const UEdGraphPin* Pin) const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual void PinDefaultValueChanged(UEdGraphPin* Pin) override;
	virtual FText GetTooltipText() const override;
	virtual void ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	virtual void PostReconstructNode() override;
	virtual void ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins) override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual FText GetMenuCategory() const override;
	virtual bool IsConnectionDisallowed(const UEdGraphPin* MyPin, const UEdGraphPin* OtherPin, FString& OutReason) const override;
	virtual void EarlyValidation(class FCompilerResultsLog& MessageLog) const override;
	virtual void PreloadRequiredAssets() override;
	virtual void NotifyPinConnectionListChanged(UEdGraphPin* Pin) override;
	virtual bool IsNodeSafeToIgnore() const override { return true; }

protected:
	virtual FName GetBridgeFunctionName() const PURE_VIRTUAL(UK2Node_ESQLPlayerTypedRowBridgeBase::GetBridgeFunctionName, return NAME_None;);
	virtual FText GetMenuTitleText() const PURE_VIRTUAL(UK2Node_ESQLPlayerTypedRowBridgeBase::GetMenuTitleText, return FText::GetEmpty(););
	virtual FText GetNodeTooltipText() const PURE_VIRTUAL(UK2Node_ESQLPlayerTypedRowBridgeBase::GetNodeTooltipText, return FText::GetEmpty(););
	virtual FName GetTypedRowPinName() const PURE_VIRTUAL(UK2Node_ESQLPlayerTypedRowBridgeBase::GetTypedRowPinName, return NAME_None;);
	virtual FText GetTypedRowPinFriendlyName() const PURE_VIRTUAL(UK2Node_ESQLPlayerTypedRowBridgeBase::GetTypedRowPinFriendlyName, return FText::GetEmpty(););
	virtual FText GetTypedRowPinDescription() const PURE_VIRTUAL(UK2Node_ESQLPlayerTypedRowBridgeBase::GetTypedRowPinDescription, return FText::GetEmpty(););
	virtual EEdGraphPinDirection GetTypedRowPinDirection() const PURE_VIRTUAL(UK2Node_ESQLPlayerTypedRowBridgeBase::GetTypedRowPinDirection, return EGPD_Input;);
	virtual void AllocateOperationPins() PURE_VIRTUAL(UK2Node_ESQLPlayerTypedRowBridgeBase::AllocateOperationPins, );
	virtual void ExpandOperationPins(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph, UK2Node_CallFunction* FunctionNode) PURE_VIRTUAL(UK2Node_ESQLPlayerTypedRowBridgeBase::ExpandOperationPins, );
	virtual void ValidateOperationPins(class FCompilerResultsLog& MessageLog) const {}

	UEdGraphPin* GetPlayerDBPin(const TArray<UEdGraphPin*>* PinsToSearch = nullptr) const;
	UEdGraphPin* GetTableAssetPin(const TArray<UEdGraphPin*>* PinsToSearch = nullptr) const;
	UEdGraphPin* GetTypedRowPin() const;
	UEdGraphPin* GetQueryResultPin() const;
	UScriptStruct* ResolveRowStructType() const;
	void RefreshTypedRowPinType();
	void SetPinToolTip(UEdGraphPin& Pin, const FText& Description) const;

private:
	void SetTypedRowPinType(UScriptStruct* NewRowStruct);

	mutable FNodeTextCache CachedNodeTitle;
};

UCLASS()
class UNREALEXTENDEDSQLEDITOR_API UK2Node_ESQLGetPlayerRow : public UK2Node_ESQLPlayerTypedRowBridgeBase
{
	GENERATED_BODY()

protected:
	virtual FName GetBridgeFunctionName() const override;
	virtual FText GetMenuTitleText() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual FName GetTypedRowPinName() const override;
	virtual FText GetTypedRowPinFriendlyName() const override;
	virtual FText GetTypedRowPinDescription() const override;
	virtual EEdGraphPinDirection GetTypedRowPinDirection() const override;
	virtual void AllocateOperationPins() override;
	virtual void ExpandOperationPins(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph, UK2Node_CallFunction* FunctionNode) override;
	virtual void ValidateOperationPins(class FCompilerResultsLog& MessageLog) const override;
};

UCLASS()
class UNREALEXTENDEDSQLEDITOR_API UK2Node_ESQLSavePlayerRow : public UK2Node_ESQLPlayerTypedRowBridgeBase
{
	GENERATED_BODY()

protected:
	virtual FName GetBridgeFunctionName() const override;
	virtual FText GetMenuTitleText() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual FName GetTypedRowPinName() const override;
	virtual FText GetTypedRowPinFriendlyName() const override;
	virtual FText GetTypedRowPinDescription() const override;
	virtual EEdGraphPinDirection GetTypedRowPinDirection() const override;
	virtual void AllocateOperationPins() override;
	virtual void ExpandOperationPins(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph, UK2Node_CallFunction* FunctionNode) override;

private:
	UEdGraphPin* GetResolvedRowIdPin() const;
};

UCLASS(Abstract)
class UNREALEXTENDEDSQLEDITOR_API UK2Node_ESQLPlayerRowArrayBridgeBase : public UK2Node
{
	GENERATED_BODY()

public:
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual void PinDefaultValueChanged(UEdGraphPin* Pin) override;
	virtual FText GetTooltipText() const override;
	virtual void ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	virtual void PostReconstructNode() override;
	virtual void ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins) override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual FText GetMenuCategory() const override;
	virtual bool IsConnectionDisallowed(const UEdGraphPin* MyPin, const UEdGraphPin* OtherPin, FString& OutReason) const override;
	virtual void EarlyValidation(class FCompilerResultsLog& MessageLog) const override;
	virtual void PreloadRequiredAssets() override;
	virtual void NotifyPinConnectionListChanged(UEdGraphPin* Pin) override;
	virtual bool IsNodeSafeToIgnore() const override { return true; }

protected:
	virtual FName GetBridgeFunctionName() const PURE_VIRTUAL(UK2Node_ESQLPlayerRowArrayBridgeBase::GetBridgeFunctionName, return NAME_None;);
	virtual FText GetMenuTitleText() const PURE_VIRTUAL(UK2Node_ESQLPlayerRowArrayBridgeBase::GetMenuTitleText, return FText::GetEmpty(););
	virtual FText GetNodeTooltipText() const PURE_VIRTUAL(UK2Node_ESQLPlayerRowArrayBridgeBase::GetNodeTooltipText, return FText::GetEmpty(););
	virtual void AllocateOperationPins() PURE_VIRTUAL(UK2Node_ESQLPlayerRowArrayBridgeBase::AllocateOperationPins, );
	virtual void ExpandOperationPins(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph, UK2Node_CallFunction* FunctionNode) PURE_VIRTUAL(UK2Node_ESQLPlayerRowArrayBridgeBase::ExpandOperationPins, );
	virtual void ValidateOperationPins(class FCompilerResultsLog& MessageLog) const {}

	UEdGraphPin* GetPlayerDBPin(const TArray<UEdGraphPin*>* PinsToSearch = nullptr) const;
	UEdGraphPin* GetTableAssetPin(const TArray<UEdGraphPin*>* PinsToSearch = nullptr) const;
	UEdGraphPin* GetRowsPin() const;
	UEdGraphPin* GetQueryResultPin() const;
	UScriptStruct* ResolveRowStructType() const;
	void RefreshRowsPinType();
	void SetPinToolTip(UEdGraphPin& Pin, const FText& Description) const;

private:
	void SetRowsPinType(UScriptStruct* NewRowStruct);

	mutable FNodeTextCache CachedNodeTitle;
};

UCLASS()
class UNREALEXTENDEDSQLEDITOR_API UK2Node_ESQLGetPlayerRows : public UK2Node_ESQLPlayerRowArrayBridgeBase
{
	GENERATED_BODY()

protected:
	virtual FName GetBridgeFunctionName() const override;
	virtual FText GetMenuTitleText() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual void AllocateOperationPins() override;
	virtual void ExpandOperationPins(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph, UK2Node_CallFunction* FunctionNode) override;
};

UCLASS(meta = (ESQLHideFromBlueprintPalette = "true"))
class UNREALEXTENDEDSQLEDITOR_API UK2Node_ESQLFindPlayerRows : public UK2Node_ESQLPlayerRowArrayBridgeBase, public IK2Node_AddPinInterface, public FESQLQueryClauseUiBase
{
	GENERATED_BODY()

public:
	virtual void PinDefaultValueChanged(UEdGraphPin* Pin) override;
	virtual void PostReconstructNode() override;
	virtual void NotifyPinConnectionListChanged(UEdGraphPin* Pin) override;
	virtual void GetNodeContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const override;

	virtual void AddInputPin() override;
	virtual bool CanAddPin() const override;
	virtual void RemoveInputPin(UEdGraphPin* Pin) override;
	virtual bool CanRemovePin(const UEdGraphPin* Pin) const override;

protected:
	virtual FName GetBridgeFunctionName() const override;
	virtual FText GetMenuTitleText() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual void AllocateOperationPins() override;
	virtual void ExpandOperationPins(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph, UK2Node_CallFunction* FunctionNode) override;
	virtual void ValidateOperationPins(class FCompilerResultsLog& MessageLog) const override;

	virtual UK2Node* GetQueryClauseNode() override { return this; }
	virtual const UK2Node* GetQueryClauseNode() const override { return this; }
	virtual FESQLQueryClauseUiState& GetQueryClauseUiState() override { return ClauseUiState; }
	virtual const FESQLQueryClauseUiState& GetQueryClauseUiState() const override { return ClauseUiState; }

private:
	UEdGraphPin* GetLegacyQuerySpecPin() const;

	UPROPERTY()
	FESQLQueryClauseUiState ClauseUiState;
};

UCLASS()
class UNREALEXTENDEDSQLEDITOR_API UK2Node_ESQLDeletePlayerRow : public UK2Node
{
	GENERATED_BODY()

public:
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual void PinDefaultValueChanged(UEdGraphPin* Pin) override;
	virtual FText GetTooltipText() const override;
	virtual void ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	virtual void PostReconstructNode() override;
	virtual void ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins) override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual FText GetMenuCategory() const override;
	virtual void EarlyValidation(class FCompilerResultsLog& MessageLog) const override;
	virtual void PreloadRequiredAssets() override;
	virtual void NotifyPinConnectionListChanged(UEdGraphPin* Pin) override;
	virtual bool IsNodeSafeToIgnore() const override { return true; }

private:
	UEdGraphPin* GetPlayerDBPin(const TArray<UEdGraphPin*>* PinsToSearch = nullptr) const;
	UEdGraphPin* GetTableAssetPin(const TArray<UEdGraphPin*>* PinsToSearch = nullptr) const;
	UEdGraphPin* GetRowIdPin() const;
	UEdGraphPin* GetQueryResultPin() const;
	void SetPinToolTip(UEdGraphPin& Pin, const FText& Description) const;

	mutable FNodeTextCache CachedNodeTitle;
};