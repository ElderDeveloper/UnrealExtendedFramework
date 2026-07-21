// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#include "EGQuestEdGraphSchema.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "UObject/UObjectIterator.h"
#include "ScopedTransaction.h"
#include "AssetRegistry/AssetData.h"
#include "GraphEditorActions.h"
#include "Runtime/Launch/Resources/Version.h"

#if NY_ENGINE_VERSION >= 424
#include "ToolMenu.h"
#endif

#include "EGQuestEdGraph.h"
#include "UnrealExtendedQuestEditor/Editor/Nodes/EGQuestGraphNode.h"
#include "SchemaActions/EGQuestNewComment_GraphSchemaAction.h"
#include "SchemaActions/EGQuestNewNode_GraphSchemaAction.h"
#include "UnrealExtendedQuest/Nodes/EGQuestNode_Start.h"
#include "UnrealExtendedQuest/Nodes/EGQuestNode_Objective.h"
#include "UnrealExtendedQuest/Nodes/EGQuestNode_Custom.h"

#define LOCTEXT_NAMESPACE "QuestGraphGraphSchema"

// Initialize static properties
const FName UEGQuestEdGraphSchema::PIN_CATEGORY_Input(TEXT("ParentInputs"));
const FName UEGQuestEdGraphSchema::PIN_CATEGORY_Output(TEXT("ChildOutputs"));
const FName UEGQuestEdGraphSchema::PIN_CATEGORY_Success(TEXT("ObjectiveSuccess"));
const FName UEGQuestEdGraphSchema::PIN_CATEGORY_Fail(TEXT("ObjectiveFail"));

const FText UEGQuestEdGraphSchema::NODE_CATEGORY_Quest(LOCTEXT("QuestNodeAction", "Quest Node"));
const FText UEGQuestEdGraphSchema::NODE_CATEGORY_Graph(LOCTEXT("GraphAction", "Graph"));
const FText UEGQuestEdGraphSchema::NODE_CATEGORY_Convert(LOCTEXT("NodesConvertAction", "Convert Node(s)"));

TArray<TSubclassOf<UEGQuestNode>> UEGQuestEdGraphSchema::QuestNodeClasses;
bool UEGQuestEdGraphSchema::bQuestNodeClassesInitialized = false;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// UEGQuestEdGraphSchema
bool UEGQuestEdGraphSchema::ConnectionCausesLoop(const UEdGraphPin* InputPin, const UEdGraphPin* OutputPin) const
{
	// Only self-wiring counts as a provable loop; anything larger is a designer's routing decision.
	return InputPin->GetOwningNode() == OutputPin->GetOwningNode();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Begin EdGraphSchema Interface
void UEGQuestEdGraphSchema::GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const
{
	GetAllQuestNodeActions(ContextMenuBuilder);
	GetCommentAction(ContextMenuBuilder, ContextMenuBuilder.CurrentGraph);
}

#if NY_ENGINE_VERSION >= 424
void UEGQuestEdGraphSchema::GetContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const
{
	if (Context->Node && !Context->bIsDebugging)
	{
		// Menu for right clicking on node
		FToolMenuSection& Section = Menu->AddSection("QuestEdGraphSchemaNodeActions", LOCTEXT("NodeActionsMenuHeader", "Node Actions"));

		// This action is handled in UEGQuestEdGraphSchema::BreakNodeLinks, and the action is registered in SGraphEditorImpl (not by us)
		Section.AddMenuEntry(FGraphEditorCommands::Get().BreakNodeLinks);
	}

	// The rest of the menus are implemented in the each nodes GetContextMenuActions method
	Super::GetContextMenuActions(Menu, Context);
}

#else

void UEGQuestEdGraphSchema::GetContextMenuActions(
	const UEdGraph* CurrentGraph,
	const UEdGraphNode* InGraphNode,
	const UEdGraphPin* InGraphPin,
	FMenuBuilder* MenuBuilder,
	bool bIsDebugging
) const
{
	if (InGraphNode && !bIsDebugging)
	{
		// Menu for right clicking on node
		MenuBuilder->BeginSection("QuestEdGraphSchemaNodeActions", LOCTEXT("NodeActionsMenuHeader", "Node Actions"));
		{
			// This action is handled in UEGQuestEdGraphSchema::BreakNodeLinks, and the action is registered in SGraphEditorImpl (not by us)
			MenuBuilder->AddMenuEntry(FGraphEditorCommands::Get().BreakNodeLinks);
		}
		MenuBuilder->EndSection();
	}

	// The rest of the menus are implemented in the each nodes GetContextMenuActions method
	Super::GetContextMenuActions(CurrentGraph, InGraphNode, InGraphPin, MenuBuilder, bIsDebugging);
}
#endif // NY_ENGINE_VERSION >= 424

void UEGQuestEdGraphSchema::CreateDefaultNodesForGraph(UEdGraph& Graph) const
{
	// This should only be called on empty graphs
	check(Graph.Nodes.Num() == 0);
	UEGQuestEdGraph* QuestEdGraph = CastChecked<UEGQuestEdGraph>(&Graph);

	// Create, link and position nodes
	QuestEdGraph->CreateGraphNodesFromQuest();
	QuestEdGraph->LinkGraphNodesFromQuest();
	QuestEdGraph->AutoPositionGraphNodes();
}

FPinConnectionResponse UEGQuestEdGraphSchema::MovePinLinks(UEdGraphPin& MoveFromPin, UEdGraphPin& MoveToPin, bool bIsIntermediateMove, bool bNotifyLinkedNodes) const
{
	return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("ConnectionMovePinLinks", "Move Pin Links Not implemented"));
}

FPinConnectionResponse UEGQuestEdGraphSchema::CopyPinLinks(UEdGraphPin& CopyFromPin, UEdGraphPin& CopyToPin, bool bIsIntermediateCopy) const
{
	return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("ConnectionMovePinLinks", "Copy Pin Links Not implemented"));
}

const FPinConnectionResponse UEGQuestEdGraphSchema::CanCreateConnection(const UEdGraphPin* PinA, const UEdGraphPin* PinB) const
{
	// Make sure the pins are not on the same node
	if (PinA->GetOwningNode() == PinB->GetOwningNode())
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("ConnectionSameNode", "Both are on the same node"));
	}

	// Need one output (a root output or an objective outcome pin) and one input (a card).
	const UEdGraphPin* InputPin = nullptr;
	const UEdGraphPin* OutputPin = nullptr;
	if (!CategorizePinsByDirection(PinA, PinB, /*out*/ InputPin, /*out*/ OutputPin))
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("ConnectionSameDirection", "An arrow needs an output and an input pin"));
	}

	const UEGQuestGraphNode_Base* SourceNode = CastChecked<UEGQuestGraphNode_Base>(OutputPin->GetOwningNode());
	const UEGQuestGraphNode_Base* TargetNode = CastChecked<UEGQuestGraphNode_Base>(InputPin->GetOwningNode());

	// Does the source Node accept output connection?
	if (!SourceNode->CanHaveOutputConnections())
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("ConnectionNoOutputs", "This node does not lead anywhere"));
	}

	// Does the target Node accept input connection?
	if (!TargetNode->CanHaveInputConnections())
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("ConnectionNoInputs", "This node cannot be arrived at"));
	}

	// One arrow per (outcome pin, destination) pair.
	if (OutputPin->LinkedTo.Contains(const_cast<UEdGraphPin*>(InputPin)))
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("ConnectionAlreadyMade", "This arrow already exists"));
	}

	return FPinConnectionResponse(CONNECT_RESPONSE_MAKE, FText::GetEmpty());
}

bool UEGQuestEdGraphSchema::TryCreateConnection(UEdGraphPin* PinA, UEdGraphPin* PinB) const
{
	// Happens when connecting pin to itself, seems to be a editor bug
	if (PinA->GetOwningNode() == PinB->GetOwningNode())
	{
		return false;
	}

	// Mark for undo system, we do not know if there is transaction so just mark without verifying
	// This mostly fixes crashing on undo when there is a drag operation
	UEdGraph* Graph = PinA->GetOwningNode()->GetGraph();
	{
		PinA->GetOwningNode()->Modify();
		PinB->GetOwningNode()->Modify();
		Graph->Modify();
		FEGQuestEditorUtilities::GetQuestForGraph(Graph)->Modify();
	}

	const bool bModified = Super::TryCreateConnection(PinA, PinB);
	if (bModified)
	{
		UEGQuestGraphNode_Base* NodeB = CastChecked<UEGQuestGraphNode_Base>(PinB->GetOwningNode());
		// Update the internal structure (recompile of the Quest Node/Graph Nodes)
		NodeB->GetQuest()->CompileQuestNodesFromGraphNodes();
	}

	return bModified;
}

bool UEGQuestEdGraphSchema::SupportsDropPinOnNode(UEdGraphNode* InTargetNode, const FEdGraphPinType& InSourcePinType, EEdGraphPinDirection InSourcePinDirection, FText& OutErrorMessage) const
{
	if (InSourcePinDirection != EGPD_Output)
	{
		OutErrorMessage = LOCTEXT("DropOnNodeNeedsOutput", "An arrow can only start from an output pin");
		return false;
	}

	const UEGQuestGraphNode_Base* TargetNode = Cast<UEGQuestGraphNode_Base>(InTargetNode);
	if (!TargetNode || !TargetNode->CanHaveInputConnections() || !TargetNode->HasInputPin())
	{
		OutErrorMessage = LOCTEXT("ConnectionNoInputs", "This node cannot be arrived at");
		return false;
	}

	// FDragConnection only shows hover feedback for a node when there is a message, so the success
	// path must fill one too or the decorator falls back to "Place a new node".
	OutErrorMessage = LOCTEXT("DropOnNodeConnect", "Connect the arrow to this node");
	return true;
}

UEdGraphPin* UEGQuestEdGraphSchema::DropPinOnNode(UEdGraphNode* InTargetNode, const FName& InSourcePinName, const FEdGraphPinType& InSourcePinType, EEdGraphPinDirection InSourcePinDirection) const
{
	// The wire always ends on the card's one input pin; no pin is ever created by a drop.
	UEGQuestGraphNode_Base* TargetNode = Cast<UEGQuestGraphNode_Base>(InTargetNode);
	if (TargetNode && InSourcePinDirection == EGPD_Output && TargetNode->HasInputPin())
	{
		return TargetNode->GetInputPin();
	}
	return nullptr;
}

bool UEGQuestEdGraphSchema::ShouldHidePinDefaultValue(UEdGraphPin* Pin) const
{
	return true;
}

void UEGQuestEdGraphSchema::BreakNodeLinks(UEdGraphNode& TargetNode) const
{
	// NOTE: The SGraphEditorImpl::BreakNodeLinks that calls this (method) has the transaction declared, so do not make another one here.
	UEdGraph* Graph = TargetNode.GetGraph();
	UEGQuestGraph* Quest = FEGQuestEditorUtilities::GetQuestForGraph(Graph);

	// Mark for undo system
	Graph->Modify();
	TargetNode.Modify();
	Quest->Modify();

	Super::BreakNodeLinks(TargetNode);

	Quest->CompileQuestNodesFromGraphNodes();
}

void UEGQuestEdGraphSchema::BreakPinLinks(UEdGraphPin& TargetPin, bool bSendsNodeNotifcation) const
{
	const FScopedTransaction Transaction(LOCTEXT("GraphEd_BreakPinLinks", "Quest Editor: Break Pin Links"));
	UEdGraphNode* Node = TargetPin.GetOwningNode();
	UEdGraph* Graph = Node->GetGraph();
	UEGQuestGraph* Quest = FEGQuestEditorUtilities::GetQuestForGraph(Graph);

	// Mark for undo system
	Node->Modify();
	Graph->Modify();
	Quest->Modify();
	// Modify() is called in BreakLinkTo on the TargetPin

	Super::BreakPinLinks(TargetPin, bSendsNodeNotifcation);

	// If this would notify the node then we need to compile the Quest
	if (bSendsNodeNotifcation)
	{
		// Recompile
		Quest->CompileQuestNodesFromGraphNodes();
	}
}

void UEGQuestEdGraphSchema::BreakSinglePinLink(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin) const
{
	// Alt-clicking a wire lands here (SGraphPanel resolves the hovered spline's endpoints). Route
	// through BreakLinkTo so the break transacts, notifies both nodes and recompiles the quest.
	BreakLinkTo(SourcePin, TargetPin, true);
}

void UEGQuestEdGraphSchema::DroppedAssetsOnGraph(const TArray<FAssetData>& Assets, const FVector2D& GraphPosition, UEdGraph* Graph) const
{

}

void UEGQuestEdGraphSchema::DroppedAssetsOnNode(const TArray<FAssetData>& Assets, const FVector2D& GraphPosition, UEdGraphNode* Node) const
{

}
// End EdGraphSchema Interface
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Begin own functions
void UEGQuestEdGraphSchema::BreakLinkTo(UEdGraphPin* FromPin, UEdGraphPin* ToPin, bool bSendsNodeNotifcation) const
{
	const FScopedTransaction Transaction(LOCTEXT("GraphEd_BreakPinLink", "Quest Editor: Break Pin Link"));
	UEdGraphNode* FromNode = FromPin->GetOwningNode();
	UEdGraphNode* ToNode = ToPin->GetOwningNode();
	UEdGraph* Graph = FromNode->GetGraph();
	UEGQuestGraph* Quest = FEGQuestEditorUtilities::GetQuestForGraph(Graph);

	// Mark for undo system
	FromNode->Modify();
	ToNode->Modify();
	Graph->Modify();
	Quest->Modify();

	// Break
	FromPin->BreakLinkTo(ToPin);

	// Notify
	FromNode->PinConnectionListChanged(FromPin);
	ToNode->PinConnectionListChanged(ToPin);
	if (bSendsNodeNotifcation)
	{
		FromNode->NodeConnectionListChanged();
		ToNode->NodeConnectionListChanged();
	}

	// If this would notify the node then we need to Recompile the Quest
	if (bSendsNodeNotifcation)
	{
		Quest->CompileQuestNodesFromGraphNodes();
	}
}

void UEGQuestEdGraphSchema::GetCommentAction(FGraphActionMenuBuilder& ActionMenuBuilder, const UEdGraph* CurrentGraph) const
{
	// Do not allow to spawn a comment when we drag are dragging from a selected pin.
	if (ActionMenuBuilder.FromPin)
	{
		return;
	}

	// The rest of the comment actions are in the UEdGraphSchema::GetContextMenuActions
	const bool bIsManyNodesSelected = CurrentGraph ? GetNodeSelectionCount(CurrentGraph) > 0 : false;
	const FText MenuDescription = bIsManyNodesSelected ?
		LOCTEXT("CreateCommentAction", "Create Comment from Selection") : LOCTEXT("AddCommentAction", "Add Comment...");
	const FText ToolTip = LOCTEXT("CreateCommentToolTip", "Creates a comment.");
	constexpr int32 Grouping = 1;

	TSharedPtr<FEGQuestNewComment_GraphSchemaAction> NewAction(new FEGQuestNewComment_GraphSchemaAction(
		NODE_CATEGORY_Graph, MenuDescription, ToolTip, Grouping));
	ActionMenuBuilder.AddAction(NewAction);
}

void UEGQuestEdGraphSchema::GetAllQuestNodeActions(FGraphActionMenuBuilder& ActionMenuBuilder) const
{
	InitQuestNodeClasses();
	FText ToolTip, MenuDesc;

	// when dragging from an input pin
	if (ActionMenuBuilder.FromPin == nullptr)
	{
		// Just right clicked on the empty graph
		ToolTip = LOCTEXT("NewQuestNodeTooltip", "Adds {Name} to the graph");
		MenuDesc = LOCTEXT("NewQuestNodeMenuDescription", "Add {Name}");
	}
	else if (ActionMenuBuilder.FromPin->Direction == EGPD_Input)
	{
		// From an input pin
		ToolTip = LOCTEXT("NewQuestNodeTooltip_FromInputPin", "Adds {Name} to the graph as a parent to the current node");
		MenuDesc = LOCTEXT("NewQuestNodeMenuDescription_FromInputPin", "Add {Name} parent");
	}
	else
	{
		// From an output pin
		check(ActionMenuBuilder.FromPin->Direction == EGPD_Output);
		ToolTip = LOCTEXT("NewQuestNodeTooltip_FromOutputPin", "Adds {Name} to the graph as a child to the current node");
		MenuDesc = LOCTEXT("NewQuestNodeMenuDescription_FromOutputPin", "Add {Name} child");
	}

	int32 Grouping = 0;
	FFormatNamedArguments Arguments;

	// Generate menu actions for all the node types
	for (TSubclassOf<UEGQuestNode> QuestNodeClass : QuestNodeClasses)
	{
		const UEGQuestNode* QuestNode = QuestNodeClass->GetDefaultObject<UEGQuestNode>();
		Arguments.Add(TEXT("Name"), FText::FromString(QuestNode->GetNodeTypeString()));

		// Use per-node category for custom nodes to create subcategories
		FText NodeCategory = NODE_CATEGORY_Quest;
		if (const UEGQuestNode_Custom* CustomNode = Cast<UEGQuestNode_Custom>(QuestNode))
		{
			const FName CategoryName = CustomNode->GetCategoryName();
			if (CategoryName != NAME_None && CategoryName != TEXT("Custom"))
			{
				NodeCategory = FText::FromString(FString::Printf(TEXT("Quest Node|%s"), *CategoryName.ToString()));
			}
		}

		TSharedPtr<FEGQuestNewNode_GraphSchemaAction> Action(new FEGQuestNewNode_GraphSchemaAction(
			NodeCategory, FText::Format(MenuDesc, Arguments), FText::Format(ToolTip, Arguments),
			Grouping++, QuestNodeClass));
		ActionMenuBuilder.AddAction(Action);
	}
}

void UEGQuestEdGraphSchema::InitQuestNodeClasses()
{
	if (bQuestNodeClassesInitialized)
	{
		return;
	}

	// The placeable vocabulary is Stage and End (plus any custom node subclass a game registers).
	// Start exists once and never gets placed; objectives are rows of a stage card, never nodes.
	for (TObjectIterator<UClass> It; It; ++It)
	{
		if (!It->IsChildOf(UEGQuestNode::StaticClass()) || It->HasAnyClassFlags(CLASS_Abstract))
		{
			continue;
		}
		if (It->IsChildOf(UEGQuestNode_Start::StaticClass()) || It->IsChildOf(UEGQuestNode_Objective::StaticClass()))
		{
			continue;
		}

		QuestNodeClasses.Add(*It);
	}

	bQuestNodeClassesInitialized = true;
}
//~ End own functions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
