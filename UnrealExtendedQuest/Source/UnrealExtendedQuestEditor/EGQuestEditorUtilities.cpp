// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#include "EGQuestEditorUtilities.h"

#include "Toolkits/IToolkit.h"
#include "Toolkits/ToolkitManager.h"
#include "Templates/Casts.h"
#include "Containers/Queue.h"
#include "EdGraphNode_Comment.h"
#include "FileHelpers.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/SClassPickerDialog.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Engine/Blueprint.h"

#include "EGQuestPluginEditorModule.h"
#include "Editor/IEGQuestEditor.h"
#include "Editor/Nodes/EGQuestGraphNode.h"
#include "UnrealExtendedQuest/Nodes/EGQuestNode_Stage.h"
#include "UnrealExtendedQuest/Nodes/EGQuestNode_Objective.h"
#include "UnrealExtendedQuest/EGQuestHelper.h"
#include "UnrealExtendedQuest/EGQuestManager.h"
#include "Factories/EGQuestClassViewerFilters.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "K2Node_Event.h"

/** Useful for auto positioning */
struct NodeWithParentPosition
{
	NodeWithParentPosition() {}
	NodeWithParentPosition(UEGQuestGraphNode* InNode, const int32 InParentNodeX, const int32 InParentNodeY) :
		Node(InNode), ParentNodeX(InParentNodeX), ParentNodeY(InParentNodeY) {}

	UEGQuestGraphNode* Node = nullptr;
	int32 ParentNodeX = 0;
	int32 ParentNodeY = 0;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FEGQuestEditorUtilities
void FEGQuestEditorUtilities::LoadAllQuestsAndCheckGUIDs()
{
	//const int32 NumQuestsBefore = UEGQuestManager::GetAllQuestsFromMemory().Num();
	const int32 NumLoadedQuests = UEGQuestManager::LoadAllQuestsIntoMemory(false);
	//const int32 NumQuestsAfter = UEGQuestManager::GetAllQuestsFromMemory().Num();
	//check(NumQuestsBefore == NumQuestsAfter);
	UE_LOG(LogEGQuestPluginEditor, Log, TEXT("UEGQuestManager::LoadAllQuestsIntoMemory loaded %d Quests into Memory"), NumLoadedQuests);

	// Try to fix duplicate GUID
	// Can happen for one of the following reasons:
	// - duplicated files outside of UE
	// - somehow loaded from text files?
	// - the universe hates us? +_+
	for (UEGQuestGraph* Quest : UEGQuestManager::GetQuestsWithDuplicateGUIDs())
	{
		UE_LOG(
			LogEGQuestPluginEditor,
			Warning,
			TEXT("Quest = `%s`, GUID = `%s` has a Duplicate GUID. Regenerating."),
			*Quest->GetPathName(), *Quest->GetGUID().ToString()
		)
		Quest->RegenerateGUID();
		Quest->MarkPackageDirty();
	}

	// Give it another try, Give up :((
	// May the math Gods have mercy on us!
	for (const UEGQuestGraph* Quest : UEGQuestManager::GetQuestsWithDuplicateGUIDs())
	{
		// GUID already exists (╯°□°）╯︵ ┻━┻
		// Does this break the universe?
		UE_LOG(
			LogEGQuestPluginEditor,
			Error,
			TEXT("Quest = `%s`, GUID = `%s`"),
			*Quest->GetPathName(), *Quest->GetGUID().ToString()
		)

		UE_LOG(
			LogEGQuestPluginEditor,
			Fatal,
			TEXT("(╯°□°）╯︵ ┻━┻ Congrats, you just broke the universe, are you even human? Now please go and proove an NP complete problem."
				"The chance of generating two equal random FGuid (picking 4, uint32 numbers) is p = 9.3132257 * 10^(-10) %% (or something like this)")
		)
	}
}

const TSet<UObject*> FEGQuestEditorUtilities::GetSelectedNodes(const UEdGraph* Graph)
{
	TSharedPtr<IEGQuestEditor> QuestEditor = GetQuestEditorForGraph(Graph);
	if (QuestEditor.IsValid())
	{
		return QuestEditor->GetSelectedNodes();
	}

	return {};
}

bool FEGQuestEditorUtilities::GetBoundsForSelectedNodes(const UEdGraph* Graph, class FSlateRect& Rect, float Padding)
{
	TSharedPtr<IEGQuestEditor> QuestEditor = GetQuestEditorForGraph(Graph);
	if (QuestEditor.IsValid())
	{
		return QuestEditor->GetBoundsForSelectedNodes(Rect, Padding);
	}

	return false;
}

void FEGQuestEditorUtilities::RefreshDetailsView(const UEdGraph* Graph, bool bRestorePreviousSelection)
{
	TSharedPtr<IEGQuestEditor> QuestEditor = GetQuestEditorForGraph(Graph);
	if (QuestEditor.IsValid())
	{
		QuestEditor->RefreshDetailsView(bRestorePreviousSelection);
	}
}

void FEGQuestEditorUtilities::Refresh(const UEdGraph* Graph, bool bRestorePreviousSelection)
{
	TSharedPtr<IEGQuestEditor> QuestEditor = GetQuestEditorForGraph(Graph);
	if (QuestEditor.IsValid())
	{
		QuestEditor->Refresh(bRestorePreviousSelection);
	}
}

TSharedPtr<class IEGQuestEditor> FEGQuestEditorUtilities::GetQuestEditorForGraph(const UEdGraph* Graph)
{
	// Find the associated Quest
	const UEGQuestGraph* Quest = GetQuestForGraph(Graph);
	TSharedPtr<IEGQuestEditor> QuestEditor;

	// This Quest has already an asset editor opened
	TSharedPtr<IToolkit> FoundAssetEditor = FToolkitManager::Get().FindEditorForAsset(Quest);
	if (FoundAssetEditor.IsValid())
	{
		QuestEditor = StaticCastSharedPtr<IEGQuestEditor>(FoundAssetEditor);
	}

	return QuestEditor;
}

bool FEGQuestEditorUtilities::RemoveNode(UEdGraphNode* NodeToRemove)
{
	if (!IsValid(NodeToRemove))
	{
		return false;
	}

	UEGQuestEdGraph* Graph = CastChecked<UEGQuestEdGraph>(NodeToRemove->GetGraph());
	if (!IsValid(Graph))
	{
		return false;
	}

	// Transactions should be declared in the code that calls this method
	if (!Graph->Modify())
	{
		UE_LOG(LogEGQuestPluginEditor, Fatal, TEXT("FEGQuestEditorUtilities::RemoveNode No transaction was declared before calling this method, aborting!"));
		return false;
	}
	if (!NodeToRemove->Modify())
	{
		UE_LOG(LogEGQuestPluginEditor, Fatal, TEXT("FEGQuestEditorUtilities::RemoveNode No transaction was declared before calling this method, aborting!"));
		return false;
	}

	return Graph->RemoveGraphNode(NodeToRemove);
}

UEdGraph* FEGQuestEditorUtilities::CreateNewGraph(
	UObject* ParentScope,
	FName GraphName,
	TSubclassOf<UEdGraph> GraphClass,
	TSubclassOf<UEdGraphSchema> SchemaClass
)
{
	// Mostly copied from FBlueprintEditorUtils::CreateNewGraph
	UEdGraph* NewGraph;
	bool bRename = false;

	// Ensure this name isn't already being used for a graph
	if (GraphName != NAME_None)
	{
		UEdGraph* ExistingGraph = FindObject<UEdGraph>(ParentScope, *(GraphName.ToString()));
		ensureMsgf(!ExistingGraph, TEXT("Graph %s already exists: %s"), *GraphName.ToString(), *ExistingGraph->GetFullName());

		// Rename the old graph out of the way; but we have already failed at this point
		if (ExistingGraph)
		{
			ExistingGraph->Rename(nullptr, ExistingGraph->GetOuter(), REN_DoNotDirty | REN_ForceNoResetLoaders);
		}

		// Construct new graph with the supplied name
		NewGraph = NewObject<UEdGraph>(ParentScope, GraphClass, NAME_None, RF_Transactional);
		bRename = true;
	}
	else
	{
		// Construct a new graph with a default name
		NewGraph = NewObject<UEdGraph>(ParentScope, GraphClass, NAME_None, RF_Transactional);
	}

	NewGraph->Schema = SchemaClass;

	// Now move to where we want it to. Workaround to ensure transaction buffer is correctly utilized
	if (bRename)
	{
		NewGraph->Rename(*GraphName.ToString(), ParentScope, REN_DoNotDirty | REN_ForceNoResetLoaders);
	}

	return NewGraph;
}

bool FEGQuestEditorUtilities::CheckAndTryToFixQuest(UEGQuestGraph* Quest, bool bDisplayWarning)
{
	bool bIsDataValid = true;
#if DO_CHECK
	const TArray<UEGQuestNode*>& QuestNodes = Quest->GetNodes();
	// Do some additional checks to ensure the data is safe, useful in development
	auto checkIfMultipleEdgesToSameNode = [QuestNodes, bDisplayWarning](UEGQuestNode* Node)
	{
		if (!IsValid(Node))
		{
			return true;
		}

		TSet<int32> NodeEdgesFound;
		TSet<int32> EdgesToRemove;
		// Find the duplicate edges
		const TArray<FEGQuestEdge>& NodeChildren = Node->GetNodeChildren();
		for (int32 EdgeIndex = 0, EdgesNum = NodeChildren.Num(); EdgeIndex < EdgesNum; EdgeIndex++)
		{
			const FEGQuestEdge& Edge = NodeChildren[EdgeIndex];
			if (Edge.TargetIndex == INDEX_NONE)
			{
				continue;
			}

			if (NodeEdgesFound.Contains(Edge.TargetIndex))
			{
				// Mark for deletion
				EdgesToRemove.Add(EdgeIndex);

				if (!bDisplayWarning)
				{
					continue;
				}

				// Find source and destination
				const int32 IndexToNode = Edge.TargetIndex;
				int32 IndexFromNode = QuestNodes.Find(Node);
				if (IndexFromNode == INDEX_NONE) // start node
				{
					IndexFromNode = -1;
				}

				const FString Message = FString::Printf(
					TEXT("Node with index = `%d` connects multiple times to destination Node with index = `%d`. One of the Edges will be removed."),
					IndexFromNode, IndexToNode);
				ShowMessageBox(EAppMsgType::Ok, Message, TEXT("Invalid Quest data"));
			}
			else
			{
				NodeEdgesFound.Add(Edge.TargetIndex);
			}
		}

		// Remove if any duplicate edges
		for (int32 EdgeIndex : EdgesToRemove)
		{
			Node->RemoveChildAt(EdgeIndex);
		}

		return EdgesToRemove.Num() == 0;
	};

	for (UEGQuestNode* Node : Quest->GetMutableStartNodes())
	{
		bIsDataValid = bIsDataValid && checkIfMultipleEdgesToSameNode(Node);
	}

	for (UEGQuestNode* Node : QuestNodes)
	{
		bIsDataValid = bIsDataValid && checkIfMultipleEdgesToSameNode(Node);
	}

#endif

	return bIsDataValid;
}

void FEGQuestEditorUtilities::TryToCreateDefaultGraph(UEGQuestGraph* Quest, bool bPrompt)
{
	// Clear the graph if the number of nodes differ
	if (AreQuestNodesInSyncWithGraphNodes(Quest))
	{
		return;
	}

	// Simply do the operations without any consent
	if (!bPrompt)
	{
		// Always keep in sync with the .quest text file.
		Quest->InitialSyncWithTextFile();
		CheckAndTryToFixQuest(Quest);
		Quest->ClearGraph();
		return;
	}

	// Prompt to the user to initial sync with the text file
	{
		const EAppReturnType::Type Response = ShowMessageBox(EAppMsgType::YesNo,
			FString::Printf(TEXT("Initial sync the Quests nodes of `%s` from the text file with the same name?"), *Quest->GetName()),
			TEXT("Get Quest nodes from the text file"));

		if (Response == EAppReturnType::Yes)
		{
			Quest->InitialSyncWithTextFile();
		}
	}
	CheckAndTryToFixQuest(Quest);

	// Prompt the user and only if he answers yes we clear the graph
	{
		const int32 NumGraphNodes = CastChecked<UEGQuestEdGraph>(Quest->GetGraph())->GetAllQuestGraphNodes().Num();
		const int32 NumQuestNodes = Quest->GetNodes().Num() + 1; // (plus the start node)
		const FString Message = FString::Printf(TEXT("Quest with name = `%s` has number of graph nodes (%d) != number quest nodes (%d)."),
			*Quest->GetName(), NumGraphNodes, NumQuestNodes);
		const EAppReturnType::Type Response = ShowMessageBox(EAppMsgType::YesNo,
			FString::Printf(TEXT("%s%s"), *Message, TEXT("\nWould you like to autogenerate the graph nodes from the quest nodes?\n WARNING: Graph nodes will be lost")),
			TEXT("Autogenerate graph nodes from quest nodes?"));

		// This will trigger the CreateDefaultNodesForGraph in the the GraphSchema
		if (Response == EAppReturnType::Yes)
		{
			Quest->ClearGraph();
		}
	}
}

bool FEGQuestEditorUtilities::AreQuestNodesInSyncWithGraphNodes(const UEGQuestGraph* Quest)
{
	// A stage's objectives are rows of its card, not graph nodes: expect one graph node per
	// start/stage/end/custom node, and none for the objectives that stages own.
	const TArray<UEGQuestNode*>& QuestNodes = Quest->GetNodes();
	int32 NumExpectedGraphNodes = Quest->GetStartNodes().Num();
	for (const UEGQuestNode* QuestNode : QuestNodes)
	{
		if (QuestNode && !QuestNode->IsA<UEGQuestNode_Objective>())
		{
			NumExpectedGraphNodes++;
		}
	}

	const int32 NumGraphNodes = CastChecked<UEGQuestEdGraph>(Quest->GetGraph())->GetAllQuestGraphNodes().Num();
	return NumGraphNodes == NumExpectedGraphNodes;
}

UEGQuestNode* FEGQuestEditorUtilities::GetClosestNodeFromGraphNode(UEdGraphNode* GraphNode)
{
	const UEGQuestGraphNode_Base* BaseNode = Cast<UEGQuestGraphNode_Base>(GraphNode);
	if (!BaseNode)
	{
		return nullptr;
	}

	// Node
	if (const UEGQuestGraphNode* Node = Cast<UEGQuestGraphNode>(BaseNode))
	{
		return Node->GetMutableQuestNode();
	}

	return nullptr;
}

void FEGQuestEditorUtilities::AutoPositionGraphNodes(
	UEGQuestGraphNode* RootNode,
	const TArray<UEGQuestGraphNode*>& GraphNodes,
	int32 OffsetBetweenColumnsX,
	int32 OffsetBetweenRowsY,
	bool bIsDirectionVertical
)
{
	TSet<UEGQuestGraphNode*> VisitedNodes;
	VisitedNodes.Add(RootNode);
	TQueue<NodeWithParentPosition> Queue;
	verify(Queue.Enqueue(NodeWithParentPosition(RootNode, 0, 0)));

	// Find first node with children so that we do not get all the graph with orphan nodes
	{
		auto HasAnyOutputConnection = [](const UEGQuestGraphNode* GraphNode)
		{
			for (const UEdGraphPin* OutputPin : GraphNode->GetOutputPins())
			{
				if (OutputPin->LinkedTo.Num() > 0)
				{
					return true;
				}
			}
			return false;
		};

		UEGQuestGraphNode* Node = RootNode;
		int32 Index = 0;
		while (Index < GraphNodes.Num() && !HasAnyOutputConnection(Node))
		{
			Node = GraphNodes[Index];
			Index++;
		}
		if (Node != RootNode)
		{
			NodeWithParentPosition ParentPosition;
			if (bIsDirectionVertical)
			{
				ParentPosition = NodeWithParentPosition(Node, 0, OffsetBetweenRowsY);
			}
			else
			{
				ParentPosition = NodeWithParentPosition(Node, OffsetBetweenColumnsX, 0);
			}

			verify(Queue.Enqueue(ParentPosition));
		}
	}

	// Just some BFS
	while (!Queue.IsEmpty())
	{
		NodeWithParentPosition NodeWithPosition;
		verify(Queue.Dequeue(NodeWithPosition));
		UEGQuestGraphNode* Node = NodeWithPosition.Node;

		if (bIsDirectionVertical)
		{
			// Position this node at the same level only one row further (down)
			Node->SetPosition(
				NodeWithPosition.ParentNodeX,
				NodeWithPosition.ParentNodeY + OffsetBetweenRowsY
			);
		}
		else
		{
			// Position this node at the same level only one column further (to the right)
			Node->SetPosition(
				NodeWithPosition.ParentNodeX + OffsetBetweenColumnsX,
				NodeWithPosition.ParentNodeY
			);
		}

		// Gather the list of unvisited child nodes, useful for not drawing weird children
		TArray<UEGQuestGraphNode*> ChildNodesUnvisited;
		for (UEGQuestGraphNode* ChildNode : Node->GetChildNodes())
		{
			// Prevent double visiting
			if (!VisitedNodes.Contains(ChildNode))
			{
				ChildNodesUnvisited.Add(ChildNode);
			}
		}

		// Adjust
		int32 ChildOffsetPos;
		if (bIsDirectionVertical)
		{
			// Adjust for the number of nodes, so that we are left, down by half
			int32 ChildOffsetPosX = Node->NodePosX;
			if (ChildNodesUnvisited.Num() > 1) // only adjust X position if we have more than one child
			{
				ChildOffsetPosX -= OffsetBetweenColumnsX * ChildNodesUnvisited.Num() / 2;
			}

			ChildOffsetPos = ChildOffsetPosX;
		}
		else
		{
			// Adjust for the number of nodes, so that we are right above (top) by half
			int32 ChildOffsetPosY = Node->NodePosY;
			if (ChildNodesUnvisited.Num() > 1) // only adjust Y position if we have more than one child
			{
				ChildOffsetPosY -= OffsetBetweenRowsY * ChildNodesUnvisited.Num() / 2;
			}

			ChildOffsetPos = ChildOffsetPosY;
		}

		// Position children
		for (int32 ChildIndex = 0, ChildNum = ChildNodesUnvisited.Num(); ChildIndex < ChildNum; ChildIndex++)
		{
			UEGQuestGraphNode* ChildNode = ChildNodesUnvisited[ChildIndex];
			if (bIsDirectionVertical)
			{
				ChildNode->SetPosition(ChildOffsetPos, Node->NodePosY);
			}
			else
			{
				ChildNode->SetPosition(Node->NodePosX, ChildOffsetPos);
			}

			VisitedNodes.Add(ChildNode);

			NodeWithParentPosition ParentPosition;
			if (bIsDirectionVertical)
			{
				ParentPosition = NodeWithParentPosition(ChildNode, ChildOffsetPos, Node->NodePosY);
				ChildOffsetPos += OffsetBetweenColumnsX + ChildNode->EstimateNodeWidth();
			}
			else
			{
				// Next child on this level will set X aka columns to Node->NodePosX + OffsetBetweenColumnsX
				// And Y aka row will be the same as this parent node, so it will be ChildOffsetPosY
				ParentPosition = NodeWithParentPosition(ChildNode, Node->NodePosX + ChildNode->EstimateNodeWidth() * 1.5, ChildOffsetPos);
				ChildOffsetPos += OffsetBetweenRowsY;
			}

			Queue.Enqueue(ParentPosition);
		}
	}

	// Fix position of orphans (nodes/node group with no parents)
	if (GraphNodes.Num() != VisitedNodes.Num())
	{
		TSet<UEGQuestGraphNode*> NodesSet(GraphNodes);
		// Nodes that are in the graph but not in the visited nodes set
		TSet<UEGQuestGraphNode*> OrphanedNodes = NodesSet.Difference(VisitedNodes);
		for (UEGQuestGraphNode* Node : OrphanedNodes)
		{
			// Finds the highest bottom left point
			const FVector2D NodePos = Node->GetGraph()->GetGoodPlaceForNewNode();
			if (bIsDirectionVertical)
			{
				Node->SetPosition(
					NodePos.X,
					NodePos.Y + OffsetBetweenRowsY
				);
			}
			else
			{
				Node->SetPosition(
					NodePos.X + OffsetBetweenColumnsX,
					NodePos.Y
				);
			}
		}
	}
}

void FEGQuestEditorUtilities::CloseOtherEditors(UObject* Asset, IAssetEditorInstance* OnlyEditor)
{
	if (!IsValid(Asset) || !GEditor)
	{
		return;
	}

#if NY_ENGINE_VERSION >= 424
	GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->CloseOtherEditors(Asset, OnlyEditor);
#else
	FAssetEditorManager::Get().CloseOtherEditors(Asset, OnlyEditor);
#endif
}

bool FEGQuestEditorUtilities::OpenEditorForAsset(const UObject* Asset)
{
	if (!IsValid(Asset) || !GEditor)
	{
		return false;
	}

#if NY_ENGINE_VERSION >= 424
	return GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(const_cast<UObject*>(Asset));
#else
	return FAssetEditorManager::Get().OpenEditorForAsset(const_cast<UObject*>(Asset));
#endif
}

IAssetEditorInstance* FEGQuestEditorUtilities::FindEditorForAsset(UObject* Asset, bool bFocusIfOpen)
{
	if (!IsValid(Asset) || !GEditor)
	{
		return nullptr;
	}

#if NY_ENGINE_VERSION >= 424
	return GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->FindEditorForAsset(Asset, bFocusIfOpen);
#else
	return FAssetEditorManager::Get().FindEditorForAsset(Asset, bFocusIfOpen);
#endif
}

bool FEGQuestEditorUtilities::OpenEditorAndJumpToGraphNode(const UEdGraphNode* GraphNode, bool bFocusIfOpen /*= false*/)
{
	if (!IsValid(GraphNode))
	{
		return false;
	}

	// Open if not already.
	UEGQuestGraph* Quest = GetQuestFromGraphNode(GraphNode);
	if (!OpenEditorForAsset(Quest))
	{
		return false;
	}

	// Could still fail focus on the graph node
	if (IAssetEditorInstance* EditorInstance = FindEditorForAsset(Quest, bFocusIfOpen))
	{
		EditorInstance->FocusWindow(const_cast<UEdGraphNode*>(GraphNode));
		return true;
	}

	return false;
}

bool FEGQuestEditorUtilities::JumpToGraphNode(const UEdGraphNode* GraphNode)
{
	if (!IsValid(GraphNode))
	{
		return false;
	}

	TSharedPtr<IEGQuestEditor> QuestEditor = GetQuestEditorForGraph(GraphNode->GetGraph());
	if (QuestEditor.IsValid())
	{
		QuestEditor->JumpToObject(GraphNode);
		return true;
	}

	return false;
}

bool FEGQuestEditorUtilities::JumpToGraphNodeIndex(const UEGQuestGraph* Quest, int32 NodeIndex)
{
	if (!Quest)
	{
		return false;
	}

	if (UEGQuestNode* Node = Quest->GetMutableNodeFromIndex(NodeIndex))
	{
		return JumpToGraphNode(Node->GetGraphNode());
	}

	return false;
}


EAppReturnType::Type FEGQuestEditorUtilities::ShowMessageBox(EAppMsgType::Type MsgType, const FString& Text, const FString& Caption)
{
	UE_LOG(LogEGQuestPluginEditor, Warning, TEXT("%s\n%s"), *Caption, *Text);
	return FPlatformMisc::MessageBoxExt(MsgType, *Text, *Caption);
}

UEGQuestGraph* FEGQuestEditorUtilities::GetQuestFromGraphNode(const UEdGraphNode* GraphNode)
{
	if (const UEGQuestGraphNode_Base* QuestBaseNode = Cast<UEGQuestGraphNode_Base>(GraphNode))
	{
		return QuestBaseNode->GetQuest();
	}

	// Last change
	if (const UEGQuestEdGraph* QuestEdGraph = Cast<UEGQuestEdGraph>(GraphNode->GetGraph()))
	{
		return QuestEdGraph->GetQuest();
	}

	return nullptr;
}

bool FEGQuestEditorUtilities::SaveAllQuests()
{
	const TArray<UEGQuestGraph*> Quests = UEGQuestManager::GetAllQuestsFromMemory();
	TArray<UPackage*> PackagesToSave;
	const bool bBatchOnlyInGameQuests = GetDefault<UEGQuestPluginSettings>()->bBatchOnlyInGameQuests;

	for (UEGQuestGraph* Quest : Quests)
	{
		// Ignore, not in game directory
		if (bBatchOnlyInGameQuests && !Quest->IsInProjectDirectory())
		{
			continue;
		}

		Quest->MarkPackageDirty();
		PackagesToSave.Add(Quest->GetOutermost());
	}

	static constexpr bool bCheckDirty = false;
	static constexpr bool bPromptToSave = false;
	return FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, bCheckDirty, bPromptToSave) == FEditorFileUtils::EPromptReturnCode::PR_Success;
}

bool FEGQuestEditorUtilities::DeleteAllQuestsTextFiles()
{
	const TArray<UEGQuestGraph*> Quests = UEGQuestManager::GetAllQuestsFromMemory();
	const bool bBatchOnlyInGameQuests = GetDefault<UEGQuestPluginSettings>()->bBatchOnlyInGameQuests;
	for (const UEGQuestGraph* Quest : Quests)
	{
		// Ignore, not in game directory
		if (bBatchOnlyInGameQuests && !Quest->IsInProjectDirectory())
		{
			continue;
		}

		Quest->DeleteAllTextFiles();
	}

	return true;
}

bool FEGQuestEditorUtilities::PickChildrenOfClass(const FText& TitleText, UClass*& OutChosenClass, UClass* Class)
{
	// Create filter
	TSharedPtr<FEGQuestChildrenOfClassFilterViewer> Filter = MakeShareable(new FEGQuestChildrenOfClassFilterViewer);
	Filter->AllowedChildrenOfClasses.Add(Class);

	// Fill in options
	FClassViewerInitializationOptions Options;
	Options.Mode = EClassViewerMode::ClassPicker;

	const UEGQuestPluginSettings* Settings = GetDefault<UEGQuestPluginSettings>();
	Options.DisplayMode = Settings->GetUnrealClassPickerDisplayMode();
#if NY_ENGINE_VERSION >= 500
	Options.ClassFilters.Add(Filter.ToSharedRef());
#else
	Options.ClassFilter = Filter;
#endif
	Options.bShowUnloadedBlueprints = true;
	Options.bExpandRootNodes = true;
	Options.NameTypeToDisplay = EClassViewerNameTypeToDisplay::Dynamic;

	return SClassPickerDialog::PickClass(TitleText, Options, OutChosenClass, Class);
}

bool FEGQuestEditorUtilities::OpenBlueprintEditor(
	UBlueprint* Blueprint,
	EEGQuestBlueprintOpenType OpenType,
	FName FunctionNameToOpen,
	bool bForceFullEditor,
	bool bAddBlueprintFunctionIfItDoesNotExist
)
{
	if (!Blueprint)
	{
		return false;
	}

	Blueprint->bForceFullEditor = bForceFullEditor;

	// Find Function Graph
	UObject* ObjectToFocusOn = nullptr;
	if (OpenType != EEGQuestBlueprintOpenType::None && FunctionNameToOpen != NAME_None)
	{
		UClass* Class = Blueprint->GeneratedClass;
		check(Class);

		if (OpenType == EEGQuestBlueprintOpenType::Function)
		{
			ObjectToFocusOn = bAddBlueprintFunctionIfItDoesNotExist
				? BlueprintGetOrAddFunction(Blueprint, FunctionNameToOpen, Class)
				: BlueprintGetFunction(Blueprint, FunctionNameToOpen, Class);
		}
		else if (OpenType == EEGQuestBlueprintOpenType::Event)
		{
			ObjectToFocusOn = bAddBlueprintFunctionIfItDoesNotExist
				? BlueprintGetOrAddEvent(Blueprint, FunctionNameToOpen, Class)
				: BlueprintGetEvent(Blueprint, FunctionNameToOpen, Class);
		}
	}

	// Default to the last uber graph
	if (ObjectToFocusOn == nullptr)
	{
		ObjectToFocusOn = Blueprint->GetLastEditedUberGraph();
	}
	if (ObjectToFocusOn)
	{
		FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(ObjectToFocusOn);
		return true;
	}

	return OpenEditorForAsset(Blueprint);
}

UEdGraph* FEGQuestEditorUtilities::BlueprintGetOrAddFunction(UBlueprint* Blueprint, FName FunctionName, UClass* FunctionClassSignature)
{
	if (!Blueprint || Blueprint->BlueprintType != BPTYPE_Normal)
	{
		return nullptr;
	}

	// Find existing function
	if (UEdGraph* GraphFunction = BlueprintGetFunction(Blueprint, FunctionName, FunctionClassSignature))
	{
		return GraphFunction;
	}

	// Create a new function
	UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, FunctionName, UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
	FBlueprintEditorUtils::AddFunctionGraph(Blueprint, NewGraph, /*bIsUserCreated=*/ false, FunctionClassSignature);
	Blueprint->LastEditedDocuments.Add(NewGraph);
	return NewGraph;
}

UEdGraph* FEGQuestEditorUtilities::BlueprintGetFunction(UBlueprint* Blueprint, FName FunctionName, UClass* FunctionClassSignature)
{
	if (!Blueprint || Blueprint->BlueprintType != BPTYPE_Normal)
	{
		return nullptr;
	}

	// Find existing function
	for (UEdGraph* GraphFunction : Blueprint->FunctionGraphs)
	{
		if (FunctionName == GraphFunction->GetFName())
		{
			return GraphFunction;
		}
	}

	// Find in the implemented Interfaces Graphs
	for (const FBPInterfaceDescription& Interface : Blueprint->ImplementedInterfaces)
	{
		for (UEdGraph* GraphFunction : Interface.Graphs)
		{
			if (FunctionName == GraphFunction->GetFName())
			{
				return GraphFunction;
			}
		}
	}

	return nullptr;
}

UK2Node_Event* FEGQuestEditorUtilities::BlueprintGetOrAddEvent(UBlueprint* Blueprint, FName EventName, UClass* EventClassSignature)
{
	if (!Blueprint || Blueprint->BlueprintType != BPTYPE_Normal)
	{
		return nullptr;
	}

	// Find existing event
	if (UK2Node_Event* EventNode = BlueprintGetEvent(Blueprint, EventName, EventClassSignature))
	{
		return EventNode;
	}

	// Create a New Event
	if (Blueprint->UbergraphPages.Num())
	{
		int32 NodePositionY = 0;
		UK2Node_Event* NodeEvent = FKismetEditorUtilities::AddDefaultEventNode(
			Blueprint,
			Blueprint->UbergraphPages[0],
			EventName,
			EventClassSignature,
			NodePositionY
		);
		NodeEvent->SetEnabledState(ENodeEnabledState::Enabled);
		NodeEvent->NodeComment = "";
		NodeEvent->bCommentBubbleVisible = false;
		return NodeEvent;
	}

	return nullptr;
}

UK2Node_Event* FEGQuestEditorUtilities::BlueprintGetEvent(UBlueprint* Blueprint, FName EventName, UClass* EventClassSignature)
{
	if (!Blueprint || Blueprint->BlueprintType != BPTYPE_Normal)
	{
		return nullptr;
	}

	TArray<UK2Node_Event*> AllEvents;
	FBlueprintEditorUtils::GetAllNodesOfClass<UK2Node_Event>(Blueprint, AllEvents);
	for (UK2Node_Event* EventNode : AllEvents)
	{
		if (EventNode->bOverrideFunction && EventNode->EventReference.GetMemberName() == EventName)
		{
			return EventNode;
		}
	}

	return nullptr;
}

UEdGraphNode_Comment* FEGQuestEditorUtilities::BlueprintAddComment(UBlueprint* Blueprint, const FString& CommentString, FNYVector2f Location)
{
	if (!Blueprint || Blueprint->BlueprintType != BPTYPE_Normal || Blueprint->UbergraphPages.Num() == 0)
	{
		return nullptr;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	TSharedPtr<FEdGraphSchemaAction> Action = Graph->GetSchema()->GetCreateCommentAction();
	if (!Action.IsValid())
	{
		return nullptr;
	}

	UEdGraphNode* GraphNode = Action->PerformAction(Graph, nullptr, Location);
	if (UEdGraphNode_Comment* CommentNode = Cast<UEdGraphNode_Comment>(GraphNode))
	{
		CommentNode->NodeComment = CommentString;
		return CommentNode;
	}

	return nullptr;
}

void FEGQuestEditorUtilities::RefreshQuestEditorForGraph(const UEdGraph* Graph)
{
	TSharedPtr<IEGQuestEditor> QuestEditor = GetQuestEditorForGraph(Graph);
	if (QuestEditor.IsValid())
	{
		QuestEditor->Refresh(true);
	}
}
