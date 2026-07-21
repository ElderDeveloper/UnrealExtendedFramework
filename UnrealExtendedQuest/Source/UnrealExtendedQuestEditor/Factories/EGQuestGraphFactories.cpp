// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#include "EGQuestGraphFactories.h"

#include "UnrealExtendedQuestEditor/Editor/Nodes/EGQuestGraphNode_Base.h"
#include "UnrealExtendedQuestEditor/Editor/Nodes/EGQuestGraphNode_Root.h"
#include "UnrealExtendedQuestEditor/Editor/Nodes/SEGQuestGraphNode.h"
#include "UnrealExtendedQuestEditor/Editor/Nodes/SEGQuestGraphNode_Root.h"
#include "UnrealExtendedQuestEditor/Editor/Nodes/SEGQuestGraphPin.h"

TSharedPtr<class SGraphNode> FEGQuestGraphNodeFactory::CreateNode(class UEdGraphNode* InNode) const
{
	// Quest Editor Nodes
	if (UEGQuestGraphNode* QuestNode = Cast<UEGQuestGraphNode>(InNode))
	{
		if (UEGQuestGraphNode_Root* QuestStartNode = Cast<UEGQuestGraphNode_Root>(QuestNode))
		{
			return SNew(SEGQuestGraphNode_Root, QuestStartNode);
		}

		return SNew(SEGQuestGraphNode, QuestNode);
	}

	return nullptr;
}

TSharedPtr<class SGraphPin> FEGQuestGraphPinFactory::CreatePin(class UEdGraphPin* Pin) const
{
	if (Pin->GetSchema()->IsA<UEGQuestEdGraphSchema>())
	{
		return SNew(SEGQuestGraphPin, Pin);
	}

	return nullptr;
}
