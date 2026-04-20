// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "K2Node.h"
#include "ESQLTypedRowK2Nodes.generated.h"

class FBlueprintActionDatabaseRegistrar;
class FKismetCompilerContext;
class UEdGraph;
class UEdGraphPin;
class UK2Node_CallFunction;
class UESQLTableAsset;
struct FLinearColor;

UCLASS(Abstract)
class UNREALEXTENDEDSQLEDITOR_API UK2Node_ESQLTypedRowSubsystemBase : public UK2Node
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
	virtual FName GetSubsystemFunctionName() const PURE_VIRTUAL(UK2Node_ESQLTypedRowSubsystemBase::GetSubsystemFunctionName, return NAME_None;);
	virtual FText GetMenuTitleText() const PURE_VIRTUAL(UK2Node_ESQLTypedRowSubsystemBase::GetMenuTitleText, return FText::GetEmpty(););
	virtual FText GetNodeTooltipText() const PURE_VIRTUAL(UK2Node_ESQLTypedRowSubsystemBase::GetNodeTooltipText, return FText::GetEmpty(););
	virtual FName GetTypedRowPinName() const PURE_VIRTUAL(UK2Node_ESQLTypedRowSubsystemBase::GetTypedRowPinName, return NAME_None;);
	virtual FText GetTypedRowPinFriendlyName() const PURE_VIRTUAL(UK2Node_ESQLTypedRowSubsystemBase::GetTypedRowPinFriendlyName, return FText::GetEmpty(););
	virtual FText GetTypedRowPinDescription() const PURE_VIRTUAL(UK2Node_ESQLTypedRowSubsystemBase::GetTypedRowPinDescription, return FText::GetEmpty(););
	virtual EEdGraphPinDirection GetTypedRowPinDirection() const PURE_VIRTUAL(UK2Node_ESQLTypedRowSubsystemBase::GetTypedRowPinDirection, return EGPD_Input;);
	virtual void AllocateOperationPins() PURE_VIRTUAL(UK2Node_ESQLTypedRowSubsystemBase::AllocateOperationPins, );
	virtual void ExpandOperationPins(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph, UK2Node_CallFunction* FunctionNode) PURE_VIRTUAL(UK2Node_ESQLTypedRowSubsystemBase::ExpandOperationPins, );
	virtual void ValidateOperationPins(class FCompilerResultsLog& MessageLog) const {}
	virtual void RefreshCustomPinTypes() {}

	UEdGraphPin* GetWorldContextPin(const TArray<UEdGraphPin*>* PinsToSearch = nullptr) const;
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
class UNREALEXTENDEDSQLEDITOR_API UK2Node_ESQLGetRow : public UK2Node_ESQLTypedRowSubsystemBase
{
	GENERATED_BODY()

protected:
	virtual FName GetSubsystemFunctionName() const override;
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
class UNREALEXTENDEDSQLEDITOR_API UK2Node_ESQLResolveSQLIdRow : public UK2Node_ESQLTypedRowSubsystemBase
{
	GENERATED_BODY()

protected:
	virtual FName GetSubsystemFunctionName() const override;
	virtual FText GetMenuTitleText() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual FName GetTypedRowPinName() const override;
	virtual FText GetTypedRowPinFriendlyName() const override;
	virtual FText GetTypedRowPinDescription() const override;
	virtual EEdGraphPinDirection GetTypedRowPinDirection() const override;
	virtual void AllocateOperationPins() override;
	virtual void ExpandOperationPins(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph, UK2Node_CallFunction* FunctionNode) override;
};

UCLASS()
class UNREALEXTENDEDSQLEDITOR_API UK2Node_ESQLSaveRow : public UK2Node_ESQLTypedRowSubsystemBase
{
	GENERATED_BODY()

protected:
	virtual FName GetSubsystemFunctionName() const override;
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

UCLASS(meta = (ESQLHideFromBlueprintPalette = "true"))
class UNREALEXTENDEDSQLEDITOR_API UK2Node_ESQLFindFirstRowByField : public UK2Node_ESQLTypedRowSubsystemBase
{
	GENERATED_BODY()

protected:
	virtual FName GetSubsystemFunctionName() const override;
	virtual FText GetMenuTitleText() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual FName GetTypedRowPinName() const override;
	virtual FText GetTypedRowPinFriendlyName() const override;
	virtual FText GetTypedRowPinDescription() const override;
	virtual EEdGraphPinDirection GetTypedRowPinDirection() const override;
	virtual void AllocateOperationPins() override;
	virtual void ExpandOperationPins(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph, UK2Node_CallFunction* FunctionNode) override;
	virtual void ValidateOperationPins(class FCompilerResultsLog& MessageLog) const override;
	virtual void RefreshCustomPinTypes() override;
};
