// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR
#include "EdGraph/EdGraphNode.h"

// FEGQuestDiagnostics is passed by reference below and is a plain (non-reflected) struct, so a
// forward declaration would do - but it is a header-only type in this same module and the runtime
// IsDataValid that calls through here has to construct one.
#include "UnrealExtendedQuest/EGQuestDiagnostics.h"

class UEdGraph;
class UEGQuestGraph;
class UEGQuestNode;

/**
 * Interface for quest graph interaction with the QuestPluginEditor module.
 * See QuestGraphEditorModule.h (in the QuestPluginEditor) for the implementation of this interface.
 */
class UNREALEXTENDEDQUEST_API IEGQuestEditorAccess
{
public:
	virtual ~IEGQuestEditorAccess() {}

	// Updates the graph node edges data to match the quest data
	virtual void UpdateGraphNodeEdges(UEdGraphNode* GraphNode) = 0;

	// Creates a new quest graph.
	virtual UEdGraph* CreateNewQuestEdGraph(UEGQuestGraph* Quest) const = 0;

	// Compiles the quest nodes from the graph nodes. Meaning it transforms the graph data -> (into) quest data.
	virtual void CompileQuestNodesFromGraphNodes(UEGQuestGraph* Quest) const = 0;

	/**
	 * Reports what a compile of Quest would report, WITHOUT compiling it. The read-only half of the
	 * compiler, for UEGQuestGraph::IsDataValid.
	 *
	 * Implementations must not mutate Quest, its graph, or its nodes in any way: no minting GUIDs, no
	 * seeding priorities, no SetNodes, no Modify(). The caller is const and runs on every asset that
	 * validate-on-save touches.
	 */
	virtual void ValidateQuest(const UEGQuestGraph* Quest, FEGQuestDiagnostics& OutDiagnostics) const = 0;

	// Removes all nodes from the graph.
	virtual void RemoveAllGraphNodes(UEGQuestGraph* Quest) const = 0;

	// Tells us if the number of quest nodes matches with the number of graph nodes (corresponding to quests).
	virtual bool AreQuestNodesInSyncWithGraphNodes(UEGQuestGraph* Quest) const = 0;

	// Updates the Quest to match the version UseOnlyOneOutputAndInputPin
	virtual void UpdateQuestToVersion_UseOnlyOneOutputAndInputPin(UEGQuestGraph* Quest) const = 0;

	// Tries to set the new outer for Object to the closes UEGQuestNode from UEdGraphNode
	virtual void SetNewOuterForObjectFromGraphNode(UObject* Object, UEdGraphNode* GraphNode) const = 0;

	// Recompiles the quest's embedded script blueprint when it is stale (dirty, or duplicated into a
	// new package) and points QuestScriptClass back at its generated class.
	virtual void RefreshQuestScriptBlueprint(UEGQuestGraph* Quest) const = 0;
};
#endif // WITH_EDITOR
