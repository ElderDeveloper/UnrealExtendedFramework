// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "CoreTypes.h"

#include "EGQuestGraphNode.h"

#include "EGQuestGraphNode_Root.generated.h"


UCLASS()
class UNREALEXTENDEDQUESTEDITOR_API UEGQuestGraphNode_Root : public UEGQuestGraphNode
{
	GENERATED_BODY()

public:
	// Begin UEdGraphNode interface
	/** Gets the name of this node, shown in title bar */
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;

	/** Gets the tooltip to display when over the node */
	virtual FText GetTooltipText() const override
	{
		return NSLOCTEXT("QuestGraphNode_Root", "RootToolTip", "The root start node of this graph");
	}

	// Begin UEGQuestGraphNode interface
	virtual bool IsRootNode() const override { return true; }

	/** Sets the Quest node index number, this represents the index from the QuestGraph.Nodes Array */
	virtual void SetQuestNodeIndex(int32 InIndex) override { NodeIndex = INDEX_NONE; }

	/** Gets the Quest node index number for the QuestGraph.Nodes Array */
	virtual int32 GetQuestNodeIndex() const override { return INDEX_NONE; }

	/** Gets the background color of this node. */
	virtual FLinearColor GetNodeBackgroundColor() const override { return GetDefault<UEGQuestPluginSettings>()->RootNodeColor; }
};
