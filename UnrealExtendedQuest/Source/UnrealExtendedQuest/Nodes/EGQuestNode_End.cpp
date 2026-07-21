// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#include "EGQuestNode_End.h"

FString UEGQuestNode_End::GetDesc()
{
	return TEXT("Node ending the Quest.\nDoes not have text, if it is entered the Quest is over.\nStamps its QuestResult onto the run.");
}
