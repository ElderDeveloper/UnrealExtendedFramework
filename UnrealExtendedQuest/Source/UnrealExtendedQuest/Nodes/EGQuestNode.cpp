// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#include "EGQuestNode.h"

#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "Sound/SoundWave.h"

#include "UnrealExtendedQuest/EGQuestContext.h"
#include "UnrealExtendedQuest/Logging/EGQuestLogger.h"
#include "UnrealExtendedQuest/EGQuestLocalizationHelper.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Begin UObject interface
void UEGQuestNode::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
#if NY_ENGINE_VERSION >= 500
	const auto CurrentVersion = Ar.UEVer();
#else
	const auto CurrentVersion = Ar.UE4Ver();
#endif
	if (CurrentVersion >= VER_UE4_COOKED_ASSETS_IN_EDITOR_SUPPORT)
	{
		// NOTE: This modifies the Archive
		// DO NOT REMOVE THIS
		const FStripDataFlags StripFlags(Ar);

		// Only in editor, add the graph node
#if WITH_EDITOR
		if (!StripFlags.IsEditorDataStripped())
		{
			Ar << GraphNode;
		}
#endif // WITH_EDITOR
	}
	else
	{
		// Super old version, is this possible?
#if WITH_EDITOR
		Ar << GraphNode;
#endif // WITH_EDITOR
	}
}

void UEGQuestNode::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UEGQuestNode* This = CastChecked<UEGQuestNode>(InThis);

	// Add the GraphNode to the referenced objects
#if WITH_EDITOR
	Collector.AddReferencedObject(This->GraphNode, This);
#endif

	Super::AddReferencedObjects(InThis, Collector);
}

void UEGQuestNode::PostLoad()
{
	Super::PostLoad();

	// NOTE: the GUID is deliberately not minted here - the compiler mints it. RegenerateGUID calls
	// Modify(), so minting on load would dirty every quest asset merely by opening it, and a
	// validate-on-save pass would then rewrite assets it only meant to read.
	//
	// RoutePriorities is not migrated here either, for the same reason and one more: seeding it needs
	// the destination GUIDs, which do not exist until the compiler has minted them. The graph carries
	// the migration flag (UEGQuestGraph::NeedsPriorityMigration) and the compiler seeds from the canvas
	// layout once the GUIDs are in place.
}

void UEGQuestNode::PostInitProperties()
{
	Super::PostInitProperties();

	// Ignore these cases
	if (HasAnyFlags(RF_ClassDefaultObject | RF_NeedLoad))
	{
		return;
	}

	// GUID is set in the quest compile phase
}

void UEGQuestNode::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	// Used when duplicating Nodes.
	// We only generate a new GUID is the existing one is valid, otherwise it will be set in the compile phase
	if (HasGUID())
	{
		RegenerateGUID();
	}
}

void UEGQuestNode::PostEditImport()
{
	Super::PostEditImport();

	// Used when duplicating Nodes.
	// We only generate a new GUID is the existing one is valid, otherwise it will be set in the compile phase
	if (HasGUID())
	{
		RegenerateGUID();
	}
}

#if WITH_EDITOR
void UEGQuestNode::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Signal to the listeners
	OnQuestNodePropertyChanged.Broadcast(PropertyChangedEvent, BroadcastPropertyEdgeIndexChanged);
	BroadcastPropertyEdgeIndexChanged = INDEX_NONE;
}

void UEGQuestNode::PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent)
{
	// The Super::PostEditChangeChainProperty will construct a new FPropertyChangedEvent that will only have the Property and the
	// MemberProperty name and it will call the PostEditChangeProperty, so we must get the array index of the Nodes modified from here.
	// If you want to preserve all the change history of the tree you must broadcast the event from here to the children, but be warned
	// that Property and MemberProperty are not set properly.
	BroadcastPropertyEdgeIndexChanged = PropertyChangedEvent.GetArrayIndex(GET_MEMBER_NAME_STRING_CHECKED(UEGQuestNode, Children));
	Super::PostEditChangeChainProperty(PropertyChangedEvent);
}


#endif //WITH_EDITOR
// End UObject interface
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Begin own function
bool UEGQuestNode::HandleNodeEnter(UEGQuestContext& Context)
{
	// NOTE: enter events are deliberately NOT fired here. The caller fires them once the snapshot
	// describes this node, so a handler that reads the quest sees the node it was just told about
	// rather than the previous one. See UEGQuestContext::FireActiveNodeEnterEvents.
	//
	// Formatting writes to the context, never to this (shared) node or the graph asset.
	RebuildConstructedText(Context);
	return true;
}

void UEGQuestNode::FireNodeEnterEvents(UEGQuestContext& Context)
{
	for (const TObjectPtr<UEGQuestEventCustom>& Event : EnterEvents)
	{
		// An empty entry does nothing; the editor raises a compile warning for it.
		if (Event != nullptr)
		{
			Event->EnterEvent(&Context);
		}
	}
}

FEGQuestEdge* UEGQuestNode::GetMutableNodeChildForTargetIndex(int32 TargetIndex)
{
	for (FEGQuestEdge& Edge : Children)
	{
		if (Edge.TargetIndex == TargetIndex)
		{
			return &Edge;
		}
	}

	return nullptr;
}

int32 UEGQuestNode::FindRoutePriority(const FGuid& DestinationGuid, EEGQuestArrowOutcome Outcome) const
{
	for (const FEGQuestRoutePriority& Route : RoutePriorities)
	{
		// Success and Fail are independent groups, so a destination reachable on both outcomes has one
		// entry per outcome and the outcome is part of the key.
		if (Route.DestinationGuid == DestinationGuid && Route.Outcome == Outcome)
		{
			return Route.Priority;
		}
	}

	return INDEX_NONE;
}

int32 UEGQuestNode::GetMaxRoutePriority(EEGQuestArrowOutcome Outcome) const
{
	int32 MaxPriority = INDEX_NONE;
	for (const FEGQuestRoutePriority& Route : RoutePriorities)
	{
		if (Route.Outcome == Outcome)
		{
			MaxPriority = FMath::Max(MaxPriority, Route.Priority);
		}
	}

	// INDEX_NONE cannot collide with a real priority: the property clamps at 0.
	return MaxPriority;
}

bool UEGQuestNode::SetRoutePriority(
	const FGuid& DestinationGuid, EEGQuestArrowOutcome Outcome, int32 Priority)
{
	const int32 ClampedPriority = FMath::Max(0, Priority);
	for (FEGQuestRoutePriority& Route : RoutePriorities)
	{
		if (Route.DestinationGuid == DestinationGuid && Route.Outcome == Outcome)
		{
			if (Route.Priority == ClampedPriority)
			{
				return false;
			}
			Route.Priority = ClampedPriority;
			return true;
		}
	}
	return false;
}


void UEGQuestNode::UpdateTextsValuesFromDefaultsAndRemappings(
	const UEGQuestPluginSettings& Settings, bool bUpdateGraphNode
)
{
	// Nothing on the base node: edges carry no text, so only node subclasses have texts to remap.
	if (bUpdateGraphNode)
	{
		UpdateGraphNode();
	}
}

void UEGQuestNode::UpdateTextsNamespacesAndKeys(const UEGQuestPluginSettings& Settings, bool bUpdateGraphNode)
{
	if (bUpdateGraphNode)
	{
		UpdateGraphNode();
	}
}

void UEGQuestNode::RebuildTextArguments(bool bUpdateGraphNode)
{
	if (bUpdateGraphNode)
	{
		UpdateGraphNode();
	}
}

void UEGQuestNode::UpdateGraphNode()
{
#if WITH_EDITOR
	UEGQuestGraph::GetQuestEditorAccess()->UpdateGraphNodeEdges(GraphNode);
#endif // WITH_EDITOR
}

UEGQuestGraph* UEGQuestNode::GetQuest() const
{
	// Nodes are normally outered straight to the quest, but assets saved by older editor builds can
	// have objective rows outered to the editor card (details-panel array add); the quest is still
	// further up the outer chain, so walk it instead of fataling on the direct outer.
	UEGQuestGraph* Quest = GetTypedOuter<UEGQuestGraph>();
	checkf(Quest, TEXT("Quest node %s is not owned by a UEGQuestGraph"), *GetPathName());
	return Quest;
}

// End own functions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
