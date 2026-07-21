// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#include "EGQuestMemory.h"

void FEGQuestHistory::Add(const FGuid& NodeGUID)
{
	if (NodeGUID.IsValid())
	{
		VisitedNodeGUIDs.Add(NodeGUID);
	}
}
