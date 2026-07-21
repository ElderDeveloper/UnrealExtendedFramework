// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#include "EGQuestGraphNode_Root.h"

#include "EGQuestGraphNode.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Begin UEdGraphNode interface

FText UEGQuestGraphNode_Root::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	const TArray<UEGQuestNode*> StartNodes = GetQuest()->GetStartNodes();
	if (StartNodes.Num() == 1)
	{
		return NSLOCTEXT("QuestGraphNode_Root", "RootTitle", "Start");
	}

	const int32 StartNodeIndex = StartNodes.Find(QuestNode);
	const FString AsString = FString("Start ") + FString::FromInt(StartNodeIndex);
	return FText::FromString(AsString);
}

// End UEdGraphNode interface
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
