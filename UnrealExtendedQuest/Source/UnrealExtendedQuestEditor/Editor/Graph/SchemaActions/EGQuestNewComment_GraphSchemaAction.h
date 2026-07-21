// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "EdGraph/EdGraphSchema.h"
#include "Templates/SubclassOf.h"
#include "UnrealExtendedQuest/NYEngineVersionHelpers.h"

#include "EGQuestNewComment_GraphSchemaAction.generated.h"

class UEdGraph;

/** Action to create new comment */
USTRUCT()
struct UNREALEXTENDEDQUESTEDITOR_API FEGQuestNewComment_GraphSchemaAction : public FEdGraphSchemaAction
{
	GENERATED_USTRUCT_BODY();

	FEGQuestNewComment_GraphSchemaAction() : FEdGraphSchemaAction() {}
	FEGQuestNewComment_GraphSchemaAction(const FText& InNodeCategory, const FText& InMenuDesc, const FText& InToolTip, int32 InGrouping)
		: FEdGraphSchemaAction(InNodeCategory, InMenuDesc, InToolTip, InGrouping) {}

	//~ Begin FEdGraphSchemaAction Interface
	UEdGraphNode* PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, FNYLocationVector2f Location, bool bSelectNewNode = true) override;
	//~ End FEdGraphSchemaAction Interface
};
