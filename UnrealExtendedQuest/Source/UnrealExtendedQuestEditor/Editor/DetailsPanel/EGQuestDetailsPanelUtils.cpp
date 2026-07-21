// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#include "EGQuestDetailsPanelUtils.h"
#include "UnrealExtendedQuest/EGQuestHelper.h"

UEGQuestGraphNode_Base* FEGQuestDetailsPanelUtils::GetGraphNodeBaseFromPropertyHandle(const TSharedRef<IPropertyHandle>& PropertyHandle)
{
	TArray<UObject*> OuterObjects;
	PropertyHandle->GetOuterObjects(OuterObjects);

	for (UObject* Object : OuterObjects)
	{
		if (UEGQuestNode* Node = Cast<UEGQuestNode>(Object))
		{
			return CastChecked<UEGQuestGraphNode_Base>(Node->GetGraphNode());
		}

		if (UEGQuestGraphNode_Base* Node = Cast<UEGQuestGraphNode_Base>(Object))
		{
			return Node;
		}
	}

	return nullptr;
}

UEGQuestGraphNode* FEGQuestDetailsPanelUtils::GetClosestGraphNodeFromPropertyHandle(const TSharedRef<IPropertyHandle>& PropertyHandle)
{
	return Cast<UEGQuestGraphNode>(GetGraphNodeBaseFromPropertyHandle(PropertyHandle));
}

UEGQuestGraph* FEGQuestDetailsPanelUtils::GetQuestFromPropertyHandle(const TSharedRef<IPropertyHandle>& PropertyHandle)
{
	UEGQuestGraph* Quest = nullptr;

	// Check first children objects of property handle, should be a quest node or a graph node
	if (UEGQuestGraphNode_Base* GraphNode = GetGraphNodeBaseFromPropertyHandle(PropertyHandle))
	{
		Quest = GraphNode->GetQuest();
	}

	// One last try, get to the root of the problem ;)
	if (!IsValid(Quest))
	{
		TSharedPtr<IPropertyHandle> ParentHandle = PropertyHandle->GetParentHandle();
		// Find the root property handle
		while (ParentHandle.IsValid() && ParentHandle->GetParentHandle().IsValid())
		{
			ParentHandle = ParentHandle->GetParentHandle();
		}

		// The outer should be a quest
		if (ParentHandle.IsValid())
		{
			TArray<UObject*> OuterObjects;
			ParentHandle->GetOuterObjects(OuterObjects);
			for (UObject* Object : OuterObjects)
			{
				if (UEGQuestGraph* FoundQuest = Cast<UEGQuestGraph>(Object))
				{
					Quest = FoundQuest;
					break;
				}
			}
		}
	}

	return Quest;
}

