// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FKismetCompilerContext;
class FProperty;
class UEdGraph;
class UEdGraphNode;
class UEdGraphPin;
class UK2Node;
class UESQLTableAsset;

namespace ESQLK2FieldFilterUtils
{
	UNREALEXTENDEDSQLEDITOR_API UESQLTableAsset* ResolveLiteralTableAsset(const UEdGraphNode* Node, FName TableAssetPinName);
	UNREALEXTENDEDSQLEDITOR_API const FProperty* ResolveLiteralFieldProperty(const UEdGraphNode* Node, FName TableAssetPinName, FName FieldNamePinName);
	UNREALEXTENDEDSQLEDITOR_API void ApplyFieldValuePinType(UEdGraphPin& ValuePin, const FProperty* FieldProperty);
	UNREALEXTENDEDSQLEDITOR_API bool IsBindingValuePin(const UEdGraphPin* Pin);
	UNREALEXTENDEDSQLEDITOR_API void CollectFieldNameOptions(const UEdGraphNode* Node, FName TableAssetPinName, TArray<TSharedPtr<FName>>& OutOptions);
	UNREALEXTENDEDSQLEDITOR_API void MoveValuePinToBindingPin(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph, UK2Node* SourceNode, UEdGraphPin* ValuePin, UEdGraphPin* TargetBindingPin);
}