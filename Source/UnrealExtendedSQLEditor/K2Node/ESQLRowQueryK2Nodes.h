// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "K2Node.h"
#include "K2Node/ESQLQueryBuilderK2Nodes.h"
#include "K2Node_AddPinInterface.h"
#include "ESQLRowQueryK2Nodes.generated.h"

class FBlueprintActionDatabaseRegistrar;
class FKismetCompilerContext;
class UEdGraph;
class UEdGraphPin;
class UGraphNodeContextMenuContext;
class UToolMenu;
class UK2Node_CallFunction;
class UESQLTableAsset;
struct FLinearColor;

UCLASS(Abstract)
class UNREALEXTENDEDSQLEDITOR_API UK2Node_ESQLRowArraySubsystemBase : public UK2Node
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
	virtual FName GetSubsystemFunctionName() const PURE_VIRTUAL(UK2Node_ESQLRowArraySubsystemBase::GetSubsystemFunctionName, return NAME_None;);
	virtual FText GetMenuTitleText() const PURE_VIRTUAL(UK2Node_ESQLRowArraySubsystemBase::GetMenuTitleText, return FText::GetEmpty(););
	virtual FText GetNodeTooltipText() const PURE_VIRTUAL(UK2Node_ESQLRowArraySubsystemBase::GetNodeTooltipText, return FText::GetEmpty(););
	virtual void AllocateOperationPins() PURE_VIRTUAL(UK2Node_ESQLRowArraySubsystemBase::AllocateOperationPins, );
	virtual void ExpandOperationPins(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph, UK2Node_CallFunction* FunctionNode) PURE_VIRTUAL(UK2Node_ESQLRowArraySubsystemBase::ExpandOperationPins, );
	virtual void ValidateOperationPins(class FCompilerResultsLog& MessageLog) const {}
	virtual void RefreshCustomPinTypes() {}

	UEdGraphPin* GetWorldContextPin(const TArray<UEdGraphPin*>* PinsToSearch = nullptr) const;
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

UCLASS(Abstract)
class UNREALEXTENDEDSQLEDITOR_API UK2Node_ESQLQuerySpecSubsystemBase : public UK2Node, public IK2Node_AddPinInterface, public FESQLQueryClauseUiBase
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
	virtual void GetNodeContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const override;
	virtual bool IsNodeSafeToIgnore() const override { return true; }

	virtual void AddInputPin() override;
	virtual bool CanAddPin() const override;
	virtual void RemoveInputPin(UEdGraphPin* Pin) override;
	virtual bool CanRemovePin(const UEdGraphPin* Pin) const override;

protected:
	virtual FName GetSubsystemFunctionName() const PURE_VIRTUAL(UK2Node_ESQLQuerySpecSubsystemBase::GetSubsystemFunctionName, return NAME_None;);
	virtual FText GetMenuTitleText() const PURE_VIRTUAL(UK2Node_ESQLQuerySpecSubsystemBase::GetMenuTitleText, return FText::GetEmpty(););
	virtual FText GetNodeTooltipText() const PURE_VIRTUAL(UK2Node_ESQLQuerySpecSubsystemBase::GetNodeTooltipText, return FText::GetEmpty(););
	virtual void AllocateResultPins() PURE_VIRTUAL(UK2Node_ESQLQuerySpecSubsystemBase::AllocateResultPins, );
	virtual void ExpandResultPins(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph, UK2Node_CallFunction* FunctionNode) PURE_VIRTUAL(UK2Node_ESQLQuerySpecSubsystemBase::ExpandResultPins, );
	virtual void ValidateAdditionalPins(class FCompilerResultsLog& MessageLog) const {}

	UEdGraphPin* GetWorldContextPin(const TArray<UEdGraphPin*>* PinsToSearch = nullptr) const;
	UEdGraphPin* GetTableAssetPin(const TArray<UEdGraphPin*>* PinsToSearch = nullptr) const;
	UEdGraphPin* GetResultPin() const;

	virtual UK2Node* GetQueryClauseNode() override { return this; }
	virtual const UK2Node* GetQueryClauseNode() const override { return this; }
	virtual FESQLQueryClauseUiState& GetQueryClauseUiState() override { return ClauseUiState; }
	virtual const FESQLQueryClauseUiState& GetQueryClauseUiState() const override { return ClauseUiState; }

	UEdGraphPin* GetQuerySpecPin() const;

private:
	mutable FNodeTextCache CachedNodeTitle;

	UPROPERTY()
	FESQLQueryClauseUiState ClauseUiState;
};

UCLASS()
class UNREALEXTENDEDSQLEDITOR_API UK2Node_ESQLGetRows : public UK2Node_ESQLRowArraySubsystemBase
{
	GENERATED_BODY()

protected:
	virtual FName GetSubsystemFunctionName() const override;
	virtual FText GetMenuTitleText() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual void AllocateOperationPins() override;
	virtual void ExpandOperationPins(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph, UK2Node_CallFunction* FunctionNode) override;
};

UCLASS(meta = (ESQLHideFromBlueprintPalette = "true"))
class UNREALEXTENDEDSQLEDITOR_API UK2Node_ESQLQueryRows : public UK2Node_ESQLQuerySpecSubsystemBase
{
	GENERATED_BODY()

protected:
	virtual FName GetSubsystemFunctionName() const override;
	virtual FText GetMenuTitleText() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual void AllocateResultPins() override;
	virtual void ExpandResultPins(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph, UK2Node_CallFunction* FunctionNode) override;
};

UCLASS(meta = (ESQLHideFromBlueprintPalette = "true"))
class UNREALEXTENDEDSQLEDITOR_API UK2Node_ESQLCountRowsQuery : public UK2Node_ESQLQuerySpecSubsystemBase
{
	GENERATED_BODY()

protected:
	virtual FName GetSubsystemFunctionName() const override;
	virtual FText GetMenuTitleText() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual void AllocateResultPins() override;
	virtual void ExpandResultPins(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph, UK2Node_CallFunction* FunctionNode) override;

private:
	UEdGraphPin* GetCountPin() const;
};

UCLASS(meta = (ESQLHideFromBlueprintPalette = "true"))
class UNREALEXTENDEDSQLEDITOR_API UK2Node_ESQLFindRows : public UK2Node_ESQLRowArraySubsystemBase, public IK2Node_AddPinInterface, public FESQLQueryClauseUiBase
{
	GENERATED_BODY()

public:
	virtual void GetNodeContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const override;

	virtual void AddInputPin() override;
	virtual bool CanAddPin() const override;
	virtual void RemoveInputPin(UEdGraphPin* Pin) override;
	virtual bool CanRemovePin(const UEdGraphPin* Pin) const override;

protected:
	virtual FName GetSubsystemFunctionName() const override;
	virtual FText GetMenuTitleText() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual void AllocateOperationPins() override;
	virtual void ExpandOperationPins(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph, UK2Node_CallFunction* FunctionNode) override;
	virtual void ValidateOperationPins(class FCompilerResultsLog& MessageLog) const override;
	virtual void RefreshCustomPinTypes() override;

	virtual UK2Node* GetQueryClauseNode() override { return this; }
	virtual const UK2Node* GetQueryClauseNode() const override { return this; }
	virtual FESQLQueryClauseUiState& GetQueryClauseUiState() override { return ClauseUiState; }
	virtual const FESQLQueryClauseUiState& GetQueryClauseUiState() const override { return ClauseUiState; }

private:
	UEdGraphPin* GetQuerySpecPin() const;

	UPROPERTY()
	FESQLQueryClauseUiState ClauseUiState;
};

UCLASS(meta = (ESQLHideFromBlueprintPalette = "true"))
class UNREALEXTENDEDSQLEDITOR_API UK2Node_ESQLFindRowsByField : public UK2Node_ESQLRowArraySubsystemBase
{
	GENERATED_BODY()

protected:
	virtual FName GetSubsystemFunctionName() const override;
	virtual FText GetMenuTitleText() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual void AllocateOperationPins() override;
	virtual void ExpandOperationPins(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph, UK2Node_CallFunction* FunctionNode) override;
	virtual void ValidateOperationPins(class FCompilerResultsLog& MessageLog) const override;
	virtual void RefreshCustomPinTypes() override;
};

UCLASS(meta = (ESQLHideFromBlueprintPalette = "true"))
class UNREALEXTENDEDSQLEDITOR_API UK2Node_ESQLLoadPage : public UK2Node_ESQLRowArraySubsystemBase
{
	GENERATED_BODY()

protected:
	virtual FName GetSubsystemFunctionName() const override;
	virtual FText GetMenuTitleText() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual void AllocateOperationPins() override;
	virtual void ExpandOperationPins(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph, UK2Node_CallFunction* FunctionNode) override;
	virtual void ValidateOperationPins(class FCompilerResultsLog& MessageLog) const override;
	virtual void RefreshCustomPinTypes() override;
};

UCLASS(meta = (ESQLHideFromBlueprintPalette = "true"))
class UNREALEXTENDEDSQLEDITOR_API UK2Node_ESQLCountRows : public UK2Node
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
	UEdGraphPin* GetWorldContextPin(const TArray<UEdGraphPin*>* PinsToSearch = nullptr) const;
	UEdGraphPin* GetTableAssetPin(const TArray<UEdGraphPin*>* PinsToSearch = nullptr) const;
	UEdGraphPin* GetFieldNamePin() const;
	UEdGraphPin* GetOperationPin() const;
	UEdGraphPin* GetValuePin() const;
	UEdGraphPin* GetCountPin() const;
	UEdGraphPin* GetQueryResultPin() const;
	void RefreshValuePinType();
	void SetPinToolTip(UEdGraphPin& Pin, const FText& Description) const;

	mutable FNodeTextCache CachedNodeTitle;
};
