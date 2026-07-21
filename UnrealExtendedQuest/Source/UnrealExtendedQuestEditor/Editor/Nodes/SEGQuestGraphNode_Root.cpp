// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#include "SEGQuestGraphNode_Root.h"

#include "EGQuestGraphNode_Root.h"

void SEGQuestGraphNode_Root::Construct(const FArguments& InArgs, UEGQuestGraphNode_Root* InNode)
{
	Super::Construct(Super::FArguments(), InNode);
	QuestGraphNode_Root = InNode;
}
