// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraph.h"

#include "UnrealExtendedQuest/EGQuestGraph.h"

#include "EGQuestEdGraph.generated.h"

class UEGQuestNode;
class UEGQuestNode_Objective;
class UEGQuestGraphNode_Base;
class UEGQuestGraphNode;
class UEGQuestGraphNode_Root;
class UEGQuestEdGraphSchema;

UCLASS()
class UEGQuestEdGraph : public UEdGraph
{
	GENERATED_BODY()

public:
	// Begin UObject Interface.
	/**
	 * Note that the object will be modified. If we are currently recording into the
	 * transaction buffer (undo/redo), save a copy of this object into the buffer and
	 * marks the package as needing to be saved.
	 *
	 * @param	bAlwaysMarkDirty	if true, marks the package dirty even if we aren't
	 *								currently recording an active undo/redo transaction
	 * @return true if the object was saved to the transaction buffer
	 */
	virtual bool Modify(bool bAlwaysMarkDirty = true) override;

	// Begin UEdGraph
	/** Remove a node from this graph. Variant of UEdGraph::RemoveNode */
	bool RemoveGraphNode(UEdGraphNode* NodeToRemove);

	// Begin Own methods
	/** Gets the QuestGraph that contains this graph */
	UEGQuestGraph* GetQuest() const
	{
		// Unreal engine magic, get the object that owns this graph, that is our Quest.
		return CastChecked<UEGQuestGraph>(GetOuter());
	}

	/** Gets the root graph node of this graph */
	TArray<UEGQuestGraphNode_Root*> GetRootGraphNodes() const;

	/** Gets all the graph nodes of this  Graph */
	const TArray<UEdGraphNode*>& GetAllGraphNodes() const { return Nodes;  }

	/** Gets the all the quest graph nodes (that inherit from UEGQuestGraphNode_Base). Includes Root node. */
	TArray<UEGQuestGraphNode_Base*> GetAllBaseQuestGraphNodes() const;

	/** Gets the all the quest graph nodes (that inherit from UEGQuestGraphNode). Includes Root node. */
	TArray<UEGQuestGraphNode*> GetAllQuestGraphNodes() const;

	/** Creates the graph nodes from the Quest that contains this graph */
	void CreateGraphNodesFromQuest();

	/** Creates all the links between the graph nodes from the Quest nodes */
	void LinkGraphNodesFromQuest() const;

	/** Automatically reposition all the nodes in the graph. */
	void AutoPositionGraphNodes() const;

	/** Remove all nodes from the graph. Without notifying anyone. This operation is atomic to the graph */
	void RemoveAllNodes();

	/** Helper method to get directly the Quest Graph Schema */
	const UEGQuestEdGraphSchema* GetQuestEdGraphSchema() const;

private:
	UEGQuestEdGraph(const FObjectInitializer& ObjectInitializer);
};
