// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "K2Node/ESQLQueryBuilderK2Nodes.h"
#include "K2Node_AddPinInterface.h"
#include "K2Node_BaseAsyncTask.h"
#include "ESQLAsyncTypedK2Nodes.generated.h"

class FBlueprintActionDatabaseRegistrar;
class FKismetCompilerContext;
class UEdGraph;
class UEdGraphPin;
class UGraphNodeContextMenuContext;
class UToolMenu;
class UESQLTableAsset;

UCLASS(Abstract)
class UNREALEXTENDEDSQLEDITOR_API UK2Node_ESQLAsyncTypedResultBase : public UK2Node_BaseAsyncTask
{
	GENERATED_BODY()

public:
	UK2Node_ESQLAsyncTypedResultBase(const FObjectInitializer& ObjectInitializer);

	virtual void AllocateDefaultPins() override;
	virtual bool CanSplitPin(const UEdGraphPin* Pin) const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual void PinDefaultValueChanged(UEdGraphPin* Pin) override;
	virtual void PostReconstructNode() override;
	virtual void ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins) override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual FText GetMenuCategory() const override;
	virtual bool IsConnectionDisallowed(const UEdGraphPin* MyPin, const UEdGraphPin* OtherPin, FString& OutReason) const override;
	virtual void PreloadRequiredAssets() override;
	virtual void NotifyPinConnectionListChanged(UEdGraphPin* Pin) override;
	virtual void ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const override;

protected:
	virtual FText GetMenuTitleText() const PURE_VIRTUAL(UK2Node_ESQLAsyncTypedResultBase::GetMenuTitleText, return FText::GetEmpty(););
	virtual FText GetNodeTooltipText() const PURE_VIRTUAL(UK2Node_ESQLAsyncTypedResultBase::GetNodeTooltipText, return FText::GetEmpty(););
	virtual FName GetTypedDataPinName() const PURE_VIRTUAL(UK2Node_ESQLAsyncTypedResultBase::GetTypedDataPinName, return NAME_None;);
	virtual FText GetTypedDataPinFriendlyName() const PURE_VIRTUAL(UK2Node_ESQLAsyncTypedResultBase::GetTypedDataPinFriendlyName, return FText::GetEmpty(););
	virtual FText GetTypedDataPinDescription() const PURE_VIRTUAL(UK2Node_ESQLAsyncTypedResultBase::GetTypedDataPinDescription, return FText::GetEmpty(););
	virtual EPinContainerType GetTypedDataPinContainerType() const PURE_VIRTUAL(UK2Node_ESQLAsyncTypedResultBase::GetTypedDataPinContainerType, return EPinContainerType::None;);
	virtual FName GetHydrationFunctionName() const PURE_VIRTUAL(UK2Node_ESQLAsyncTypedResultBase::GetHydrationFunctionName, return NAME_None;);

	UEdGraphPin* GetTableAssetPin(const TArray<UEdGraphPin*>* PinsToSearch = nullptr) const;
	UEdGraphPin* GetTypedDataPin() const;
	UEdGraphPin* GetResultPin() const;
	UScriptStruct* ResolveRowStructType() const;
	void RefreshTypedDataPinType();
	void SetPinToolTip(UEdGraphPin& Pin, const FText& Description) const;
	virtual bool HandleDelegates(const TArray<FBaseAsyncTaskHelper::FOutputPinAndLocalVariable>& VariableOutputs, UEdGraphPin* ProxyObjectPin, UEdGraphPin*& InOutLastThenPin, UEdGraph* SourceGraph, FKismetCompilerContext& CompilerContext) override;

private:
	void SetTypedDataPinType(UScriptStruct* NewRowStruct);

	mutable FNodeTextCache CachedNodeTitle;
};

UCLASS()
class UNREALEXTENDEDSQLEDITOR_API UK2Node_ESQLAsyncGetRow : public UK2Node_ESQLAsyncTypedResultBase
{
	GENERATED_BODY()

public:
	UK2Node_ESQLAsyncGetRow(const FObjectInitializer& ObjectInitializer);

protected:
	virtual FText GetMenuTitleText() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual FName GetTypedDataPinName() const override;
	virtual FText GetTypedDataPinFriendlyName() const override;
	virtual FText GetTypedDataPinDescription() const override;
	virtual EPinContainerType GetTypedDataPinContainerType() const override;
	virtual FName GetHydrationFunctionName() const override;
};

UCLASS()
class UNREALEXTENDEDSQLEDITOR_API UK2Node_ESQLAsyncGetRows : public UK2Node_ESQLAsyncTypedResultBase
{
	GENERATED_BODY()

public:
	UK2Node_ESQLAsyncGetRows(const FObjectInitializer& ObjectInitializer);

protected:
	virtual FText GetMenuTitleText() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual FName GetTypedDataPinName() const override;
	virtual FText GetTypedDataPinFriendlyName() const override;
	virtual FText GetTypedDataPinDescription() const override;
	virtual EPinContainerType GetTypedDataPinContainerType() const override;
	virtual FName GetHydrationFunctionName() const override;
};

UCLASS(meta = (ESQLHideFromBlueprintPalette = "true"))
class UNREALEXTENDEDSQLEDITOR_API UK2Node_ESQLAsyncFindRows : public UK2Node_ESQLAsyncTypedResultBase, public IK2Node_AddPinInterface, public FESQLQueryClauseUiBase
{
	GENERATED_BODY()

public:
	UK2Node_ESQLAsyncFindRows(const FObjectInitializer& ObjectInitializer);
	virtual void AllocateDefaultPins() override;
	virtual void PinDefaultValueChanged(UEdGraphPin* Pin) override;
	virtual void PostReconstructNode() override;
	virtual void NotifyPinConnectionListChanged(UEdGraphPin* Pin) override;
	virtual void ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const override;
	virtual void ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual void GetNodeContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const override;

	virtual void AddInputPin() override;
	virtual bool CanAddPin() const override;
	virtual void RemoveInputPin(UEdGraphPin* Pin) override;
	virtual bool CanRemovePin(const UEdGraphPin* Pin) const override;

protected:
	virtual FText GetMenuTitleText() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual FName GetTypedDataPinName() const override;
	virtual FText GetTypedDataPinFriendlyName() const override;
	virtual FText GetTypedDataPinDescription() const override;
	virtual EPinContainerType GetTypedDataPinContainerType() const override;
	virtual FName GetHydrationFunctionName() const override;

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
class UNREALEXTENDEDSQLEDITOR_API UK2Node_ESQLAsyncSaveRow : public UK2Node_BaseAsyncTask
{
	GENERATED_BODY()

public:
	UK2Node_ESQLAsyncSaveRow(const FObjectInitializer& ObjectInitializer);

	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual void PinDefaultValueChanged(UEdGraphPin* Pin) override;
	virtual void PostReconstructNode() override;
	virtual void ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins) override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual FText GetMenuCategory() const override;
	virtual bool IsConnectionDisallowed(const UEdGraphPin* MyPin, const UEdGraphPin* OtherPin, FString& OutReason) const override;
	virtual void PreloadRequiredAssets() override;
	virtual void NotifyPinConnectionListChanged(UEdGraphPin* Pin) override;
	virtual void ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const override;
	virtual void ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;

protected:
	UEdGraphPin* GetTableAssetPin(const TArray<UEdGraphPin*>* PinsToSearch = nullptr) const;
	UEdGraphPin* GetRowDataPin() const;
	UScriptStruct* ResolveRowStructType() const;
	void RefreshRowDataPinType();
	void SetPinToolTip(UEdGraphPin& Pin, const FText& Description) const;

private:
	void SetRowDataPinType(UScriptStruct* NewRowStruct);

	mutable FNodeTextCache CachedNodeTitle;
};