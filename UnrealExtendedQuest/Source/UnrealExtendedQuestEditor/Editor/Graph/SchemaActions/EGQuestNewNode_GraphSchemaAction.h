// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "EdGraph/EdGraphSchema.h"
#include "Templates/SubclassOf.h"

#include "UnrealExtendedQuest/Nodes/EGQuestNode.h"
#include "UnrealExtendedQuest/NYEngineVersionHelpers.h"

#include "EGQuestNewNode_GraphSchemaAction.generated.h"

class UEGQuestGraphNode;
class UEGQuestGraph;
class UEdGraph;

/** Action to add a node to the graph */
USTRUCT()
struct UNREALEXTENDEDQUESTEDITOR_API FEGQuestNewNode_GraphSchemaAction : public FEdGraphSchemaAction
{
private:
	typedef FEGQuestNewNode_GraphSchemaAction Self;

public:
	GENERATED_USTRUCT_BODY();

	FEGQuestNewNode_GraphSchemaAction() : FEdGraphSchemaAction() {}
	FEGQuestNewNode_GraphSchemaAction(
		const FText& InNodeCategory,
		const FText& InMenuDesc, const FText& InToolTip,
		int32 InGrouping, TSubclassOf<UEGQuestNode> InCreateNodeType
	) : FEdGraphSchemaAction(InNodeCategory, InMenuDesc, InToolTip, InGrouping), CreateNodeType(InCreateNodeType) {}

	//~ Begin FEdGraphSchemaAction Interface
	UEdGraphNode* PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, FNYLocationVector2f Location, bool bSelectNewNode = true) override;
	//~ End FEdGraphSchemaAction Interface

	// Spawns a new UEGQuestGraphNode of type GraphNodeType that must have a valid QuestNode of TSubclassOf<UEGQuestNode>
	template <typename GraphNodeType>
	static GraphNodeType* SpawnGraphNodeWithQuestNodeFromTemplate(
		UEdGraph* ParentGraph,
		TSubclassOf<UEGQuestNode> CreateNodeType,
		const FNYVector2f Location,
		bool bSelectNewNode = true
	)
	{
		Self Action(FText::GetEmpty(), FText::GetEmpty(), FText::GetEmpty(), 0, CreateNodeType);
		return CastChecked<GraphNodeType>(Action.PerformAction(ParentGraph, nullptr, Location, bSelectNewNode));
	}

private:
	/** Creates a new quest node from the template */
	UEdGraphNode* CreateNode(UEGQuestGraph* Quest, UEdGraph* ParentGraph, UEdGraphPin* FromPin, FNYVector2f Location, bool bSelectNewNode);

	/** Connects new node to output of selected nodes */
//	void ConnectToSelectedNodes(UEGQuestNode* NewNodeclass, UEdGraph* ParentGraph) const;

	// The node type used for when creating a new node (used by CreateNode)
	TSubclassOf<UEGQuestNode> CreateNodeType;
};

