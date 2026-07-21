// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#include "EGQuestNewNode_GraphSchemaAction.h"

#include "ScopedTransaction.h"

#include "UnrealExtendedQuest/EGQuestGraph.h"
#include "UnrealExtendedQuest/Nodes/EGQuestNode_Start.h"
#include "UnrealExtendedQuestEditor/Editor/Nodes/EGQuestGraphNode.h"
#include "UnrealExtendedQuestEditor/Editor/Nodes/EGQuestGraphNode_Root.h"

#define LOCTEXT_NAMESPACE "NewNode_QuestEdGraphSchemaAction"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FEGQuestNewNode_GraphSchemaAction
UEdGraphNode* FEGQuestNewNode_GraphSchemaAction::PerformAction(
	UEdGraph* ParentGraph,
	UEdGraphPin* FromPin,
	FNYLocationVector2f Location,
	bool bSelectNewNode/* = true*/
)
{
	const FScopedTransaction Transaction(LOCTEXT("QuestditorNewDialgueNode", "Quest Editor: New Quest Node"));
	UEGQuestGraph* Quest = CastChecked<UEGQuestEdGraph>(ParentGraph)->GetQuest();

	// Mark for modification
	// Modify() can legitimately return false when no transaction buffer exists
	// (for example in commandlets). The dirtying side effect is still required,
	// but node creation must not assert merely because undo is unavailable.
	ParentGraph->Modify();
	if (FromPin)
	{
		FromPin->Modify();
	}
	Quest->Modify();

	// Create node, without needing to compile it
	UEdGraphNode* GraphNode = CreateNode(Quest, ParentGraph, FromPin, Location, bSelectNewNode);
	Quest->PostEditChange();
	Quest->MarkPackageDirty();
	ParentGraph->NotifyGraphChanged();

	return GraphNode;
}

UEdGraphNode* FEGQuestNewNode_GraphSchemaAction::CreateNode(
	UEGQuestGraph* Quest,
	UEdGraph* ParentGraph,
	UEdGraphPin* FromPin,
	FNYVector2f Location,
	bool bSelectNewNode
)
{
	// Maximum distance a drag can be off a node edge to require 'push off' from node
	static constexpr int32 NodeDistance = 60;

	// Create the quest node
	auto QuestNode = Quest->ConstructQuestNode<UEGQuestNode>(CreateNodeType);

	// Create the graph node
	if (UEGQuestNode_Start* AsStartNode = Cast<UEGQuestNode_Start>(QuestNode))
	{
		FGraphNodeCreator<UEGQuestGraphNode_Root> NodeCreator(*ParentGraph);
		UEGQuestGraphNode_Root* GraphNode = NodeCreator.CreateUserInvokedNode(bSelectNewNode);

		// Link quest node <-> graph node
		QuestNode->SetGraphNode(GraphNode);
		GraphNode->SetQuestNode(QuestNode);
		Quest->AddStartNode(QuestNode);

		// Finalize graph node creation
		NodeCreator.Finalize(); // Calls on the node: CreateNewGuid, PostPlacedNewNode, AllocateDefaultPins

		// Position graph node
		GraphNode->SetPosition(Location.X, Location.Y);
		//ResultNode->SnapToGrid(SNAP_GRID);

		return CastChecked<UEdGraphNode>(GraphNode);
	}

	FGraphNodeCreator<UEGQuestGraphNode> NodeCreator(*ParentGraph);
	UEGQuestGraphNode* GraphNode = NodeCreator.CreateUserInvokedNode(bSelectNewNode);

	// Link quest node <-> graph node
	QuestNode->SetGraphNode(GraphNode);
	const int32 QuestNodeIndex = Quest->AddNode(QuestNode);
	GraphNode->SetQuestNodeDataChecked(QuestNodeIndex, QuestNode);

	// Finalize graph node creation
	NodeCreator.Finalize(); // Calls on the node: CreateNewGuid, PostPlacedNewNode, AllocateDefaultPins
	GraphNode->AutowireNewNode(FromPin);

	// Position graph node
	// For input pins, new node will generally overlap node being dragged off
	// Work out if we want to visually push away from connected node
	int32 XLocation = Location.X;
	if (FromPin && FromPin->Direction == EGPD_Input)
	{
		UEdGraphNode* PinNode = FromPin->GetOwningNode();
		const float XDelta = FMath::Abs(PinNode->NodePosX - Location.X);

		if (XDelta < NodeDistance)
		{
			// Set location to edge of current node minus the max move distance
			// to force node to push off from connect node enough to give selection handle
			XLocation = PinNode->NodePosX - NodeDistance;
		}
	}

	GraphNode->SetPosition(XLocation, Location.Y);
	//ResultNode->SnapToGrid(SNAP_GRID);

	return CastChecked<UEdGraphNode>(GraphNode);
}

#undef LOCTEXT_NAMESPACE
