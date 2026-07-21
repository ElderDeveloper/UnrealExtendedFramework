// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#include "EGQuestObject.h"

#include "EGQuestManager.h"
#include "EGQuestGraph.h"
#include "UObject/Object.h"

void UEGQuestObject::PostInitProperties()
{
	// We must always set the outer to be something that exists at runtime
#if WITH_EDITOR
	if (UEdGraphNode* GraphNode =  Cast<UEdGraphNode>(GetOuter()))
	{
		UEGQuestGraph::GetQuestEditorAccess()->SetNewOuterForObjectFromGraphNode(this, GraphNode);
	}
#endif

	Super::PostInitProperties();
}

UWorld* UEGQuestObject::GetWorld() const
{
	if (HasAnyFlags(RF_ArchetypeObject | RF_ClassDefaultObject))
	{
		return nullptr;
	}

	// Get from outer
	if (UObject* Outer = GetOuter())
	{
		if (UWorld* World = Outer->GetWorld())
		{
			return World;
		}
	}

	// Last resort
	return UEGQuestManager::GetQuestWorld();
}
