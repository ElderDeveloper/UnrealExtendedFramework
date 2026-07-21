// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#include "EGQuestEdGraph.h"

#include "GraphEditAction.h"

#include "UnrealExtendedQuest/EGQuestGraph.h"
#include "UnrealExtendedQuest/Nodes/EGQuestNode_Objective.h"
#include "UnrealExtendedQuest/Nodes/EGQuestNode_Stage.h"
#include "EGQuestEdGraphSchema.h"
#include "UnrealExtendedQuestEditor/Editor/Nodes/EGQuestGraphNode_Root.h"
#include "UnrealExtendedQuestEditor/Editor/Nodes/EGQuestGraphNode.h"
#include "UnrealExtendedQuestEditor/Editor/EGQuestCompiler.h"
#include "UnrealExtendedQuestEditor/EGQuestEditorAccess.h"
#include "UnrealExtendedQuestEditor/EGQuestPluginEditorModule.h"

UEGQuestEdGraph::UEGQuestEdGraph(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Set the static editor module interface used by all the quests in the QuestPlugin module to communicate with the editor.
	if (!UEGQuestGraph::GetQuestEditorAccess().IsValid())
	{
		UEGQuestGraph::SetQuestEditorAccess(TSharedPtr<IEGQuestEditorAccess>(new FEGQuestEditorAccess));
	}
}

bool UEGQuestEdGraph::Modify(bool bAlwaysMarkDirty)
{
	if (!CanModify())
	{
		return false;
	}

	bool bWasSaved = Super::Modify(bAlwaysMarkDirty);

	// Mark all nodes for modification
	// question of space (save them all here) or recompile them after every undo
	for (UEGQuestGraphNode_Base* BaseNode : GetAllBaseQuestGraphNodes())
	{
		bWasSaved = bWasSaved && BaseNode->Modify(bAlwaysMarkDirty);
	}

	return bWasSaved;
}

TArray<UEGQuestGraphNode_Root*> UEGQuestEdGraph::GetRootGraphNodes() const
{
	TArray<UEGQuestGraphNode_Root*> RootNodeList;
	GetNodesOfClass<UEGQuestGraphNode_Root>(/*out*/ RootNodeList);
	check(RootNodeList.Num() >= 1);
	return RootNodeList;
}

TArray<UEGQuestGraphNode_Base*> UEGQuestEdGraph::GetAllBaseQuestGraphNodes() const
{
	TArray<UEGQuestGraphNode_Base*> AllBaseQuestGraphNodes;
	GetNodesOfClass<UEGQuestGraphNode_Base>(/*out*/ AllBaseQuestGraphNodes);
	return AllBaseQuestGraphNodes;
}

TArray<UEGQuestGraphNode*> UEGQuestEdGraph::GetAllQuestGraphNodes() const
{
	TArray<UEGQuestGraphNode*> AllQuestGraphNodes;
	GetNodesOfClass<UEGQuestGraphNode>(/*out*/ AllQuestGraphNodes);
	return AllQuestGraphNodes;
}

bool UEGQuestEdGraph::RemoveGraphNode(UEdGraphNode* NodeToRemove)
{
	Modify();
	const int32 NumTimesNodeRemoved = Nodes.Remove(NodeToRemove);

	// This will trigger the compile in the UEGQuestEdGraphSchema::BreakNodeLinks
	// NOTE: do not call BreakAllNodeLinks on the node as it does not register properly with the
	// undo system
	GetSchema()->BreakNodeLinks(*NodeToRemove);

	// Notify
	FEdGraphEditAction RemovalAction;
	RemovalAction.Graph = this;
	RemovalAction.Action = GRAPHACTION_RemoveNode;
	RemovalAction.Nodes.Add(NodeToRemove);
	NotifyGraphChanged(RemovalAction);

	return NumTimesNodeRemoved > 0;
}

void UEGQuestEdGraph::CreateGraphNodesFromQuest()
{
	// Assume empty graph
	check(Nodes.Num() == 0);
	UEGQuestGraph* Quest = GetQuest();
	FEGQuestEditorUtilities::CheckAndTryToFixQuest(Quest, false);

	// Step 1: Create the root (start) nodes
	{
		TArray<UEGQuestNode*> StartNodes = Quest->GetMutableStartNodes();
		FGraphNodeCreator<UEGQuestGraphNode_Root> NodeCreator(*this);
		check(StartNodes.Num() > 0);
		for (int32 i = 0; i < StartNodes.Num(); ++i)
		{
			UEGQuestGraphNode_Root* StartGraphNode = NodeCreator.CreateNode();

			// Create two way direction for both Quest Node and Graph Node.
			StartGraphNode->SetQuestNode(StartNodes[i]);

			// Finalize creation
			StartGraphNode->SetPosition(i * 300.0f, 0);
			NodeCreator.Finalize();
		}
	}

	// Step 2: Figure out which objectives are owned by a stage. Owned objectives become rows of
	// their stage's card, everything else becomes a card of its own.
	const TArray<UEGQuestNode*>& QuestNodes = Quest->GetNodes();
	TMap<const UEGQuestNode*, TArray<UEGQuestNode_Objective*>> StageToObjectives;
	TSet<const UEGQuestNode*> OwnedObjectives;
	for (const UEGQuestNode* QuestNode : QuestNodes)
	{
		if (!QuestNode || !QuestNode->IsA<UEGQuestNode_Stage>())
		{
			continue;
		}
		TArray<UEGQuestNode_Objective*>& OwnedList = StageToObjectives.Add(QuestNode);
		for (const FEGQuestEdge& OwnershipEdge : QuestNode->GetNodeChildren())
		{
			if (!QuestNodes.IsValidIndex(OwnershipEdge.TargetIndex))
			{
				continue;
			}
			if (UEGQuestNode_Objective* Objective = Cast<UEGQuestNode_Objective>(QuestNodes[OwnershipEdge.TargetIndex]))
			{
				OwnedList.Add(Objective);
				OwnedObjectives.Add(Objective);
			}
		}
	}

	// Step 3: Create the graph nodes: one card per stage/end/custom node.
	for (int32 NodeIndex = 0; NodeIndex < QuestNodes.Num(); NodeIndex++)
	{
		UEGQuestNode* QuestNode = QuestNodes[NodeIndex];
		if (OwnedObjectives.Contains(QuestNode))
		{
			continue;
		}
		if (QuestNode->IsA<UEGQuestNode_Objective>())
		{
			// Degenerate data: an objective no stage owns has no card to live on. It stays in the
			// asset until the next compile drops it.
			UE_LOG(LogEGQuestPluginEditor, Warning,
				TEXT("Quest %s: objective at index %d is owned by no stage and cannot be shown; it will be dropped on the next compile"),
				*Quest->GetName(), NodeIndex);
			continue;
		}

		FGraphNodeCreator<UEGQuestGraphNode> NodeCreator(*this);
		UEGQuestGraphNode* GraphNode = NodeCreator.CreateNode();

		// Create two way direction for both Quest Node and Graph Node.
		// Objectives must be in place before Finalize: pin allocation reads the rows.
		GraphNode->SetQuestNodeDataChecked(NodeIndex, QuestNode);
		if (const TArray<UEGQuestNode_Objective*>* OwnedList = StageToObjectives.Find(QuestNode))
		{
			GraphNode->SetObjectives(*OwnedList);
		}

		// Finalize creation
		GraphNode->SetPosition(0, 0);
		NodeCreator.Finalize();
	}
}

void UEGQuestEdGraph::LinkGraphNodesFromQuest() const
{
	UEGQuestGraph* Quest = GetQuest();
	const TArray<UEGQuestNode*>& NodesQuest = Quest->GetNodes();

	auto GetGraphNodeForIndex = [&NodesQuest](int32 TargetIndex) -> UEGQuestGraphNode*
	{
		if (!NodesQuest.IsValidIndex(TargetIndex))
		{
			return nullptr;
		}
		return Cast<UEGQuestGraphNode>(NodesQuest[TargetIndex]->GetGraphNode());
	};

	auto LinkPinToEdgeTargets = [&](UEdGraphPin* OutputPin, const TArray<FEGQuestEdge>& Edges, EEGQuestArrowOutcome Outcome)
	{
		if (!OutputPin)
		{
			return;
		}
		for (const FEGQuestEdge& Edge : Edges)
		{
			if (!Edge.IsValid() || Edge.Outcome != Outcome)
			{
				continue;
			}
			UEGQuestGraphNode* TargetGraphNode = GetGraphNodeForIndex(Edge.TargetIndex);
			if (TargetGraphNode && TargetGraphNode->HasInputPin())
			{
				OutputPin->MakeLinkTo(TargetGraphNode->GetInputPin());
			}
		}
	};

	// Step 1. The root's children are the first stage(s).
	for (UEGQuestNode* StartNode : Quest->GetStartNodes())
	{
		UEGQuestGraphNode* RootGraphNode = Cast<UEGQuestGraphNode>(StartNode->GetGraphNode());
		if (!RootGraphNode || !RootGraphNode->HasOutputPin())
		{
			continue;
		}
		UEdGraphPin* OutputPin = RootGraphNode->GetOutputPin();
		OutputPin->BreakAllPinLinks();
		for (const FEGQuestEdge& Edge : StartNode->GetNodeChildren())
		{
			if (!Edge.IsValid())
			{
				continue;
			}
			UEGQuestGraphNode* TargetGraphNode = GetGraphNodeForIndex(Edge.TargetIndex);
			if (TargetGraphNode && TargetGraphNode->HasInputPin())
			{
				OutputPin->MakeLinkTo(TargetGraphNode->GetInputPin());
			}
		}
	}

	// Step 2. Objective routing arrows come off the owning stage card's outcome pins; custom nodes
	// route generically off their single output pin.
	for (UEGQuestNode* QuestNode : NodesQuest)
	{
		UEGQuestGraphNode* GraphNode = Cast<UEGQuestGraphNode>(QuestNode->GetGraphNode());
		if (!GraphNode || GraphNode->GetMutableQuestNode() != QuestNode)
		{
			// Owned objectives point back at their stage's card; they are handled with the card below.
			continue;
		}

		if (GraphNode->IsStageNode())
		{
			for (UEGQuestNode_Objective* Objective : GraphNode->GetObjectives())
			{
				if (!Objective)
				{
					continue;
				}
				LinkPinToEdgeTargets(
					GraphNode->FindObjectivePin(*Objective, EEGQuestArrowOutcome::Success),
					Objective->GetNodeChildren(), EEGQuestArrowOutcome::Success);
				LinkPinToEdgeTargets(
					GraphNode->FindObjectivePin(*Objective, EEGQuestArrowOutcome::Fail),
					Objective->GetNodeChildren(), EEGQuestArrowOutcome::Fail);
			}
		}
		else if (GraphNode->IsCustomNode() && GraphNode->HasOutputPin())
		{
			UEdGraphPin* OutputPin = GraphNode->GetOutputPin();
			OutputPin->BreakAllPinLinks();
			for (const FEGQuestEdge& Edge : QuestNode->GetNodeChildren())
			{
				if (!Edge.IsValid())
				{
					continue;
				}
				UEGQuestGraphNode* TargetGraphNode = GetGraphNodeForIndex(Edge.TargetIndex);
				if (TargetGraphNode && TargetGraphNode->HasInputPin())
				{
					OutputPin->MakeLinkTo(TargetGraphNode->GetInputPin());
				}
			}
		}

		GraphNode->ApplyCompilerWarnings();
	}

	// Links are made with MakeLinkTo, which never reaches the schema - so nothing recompiled, which
	// is right: this graph was just built from the compiled data.
}

void UEGQuestEdGraph::AutoPositionGraphNodes() const
{
	static constexpr bool bIsDirectionVertical = true;
	UEGQuestGraphNode_Root* RootNode = GetRootGraphNodes()[0];
	const TArray<UEGQuestGraphNode*> QuestGraphNodes = GetAllQuestGraphNodes();
	const UEGQuestPluginSettings* Settings = GetDefault<UEGQuestPluginSettings>();

	FEGQuestEditorUtilities::AutoPositionGraphNodes(
		RootNode,
		QuestGraphNodes,
		Settings->OffsetBetweenColumnsX,
		Settings->OffsetBetweenRowsY,
		bIsDirectionVertical
	);
}

void UEGQuestEdGraph::RemoveAllNodes()
{
	Modify();

	// Could have used RemoveNode on each node but that is unecessary as that is slow and notifies external objects
	Nodes.Empty();
	check(Nodes.Num() == 0);
}

const UEGQuestEdGraphSchema* UEGQuestEdGraph::GetQuestEdGraphSchema() const
{
	return GetDefault<UEGQuestEdGraphSchema>(Schema);
}
