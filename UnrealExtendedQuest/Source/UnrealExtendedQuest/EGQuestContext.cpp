// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#include "EGQuestContext.h"
#include "EGQuestComponent.h"

#include "Engine/Texture2D.h"
#include "Engine/Blueprint.h"

#include "EGQuestConstants.h"
#include "Nodes/EGQuestNode.h"
#include "Nodes/EGQuestNode_End.h"
#include "Nodes/EGQuestNode_Stage.h"
#include "Nodes/EGQuestNode_Start.h"
#include "EGQuestMemory.h"
#include "Logging/EGQuestLogger.h"


UEGQuestContext::UEGQuestContext(const FObjectInitializer& ObjectInitializer)
	: UEGQuestObject(ObjectInitializer)
{
}

FText UEGQuestContext::ResolveRoleText(FName RoleName) const
{
	return RoleComponent ? RoleComponent->GetRoleDisplayText(RoleRunId, RoleName) : FText::GetEmpty();
}

bool UEGQuestContext::IsValidNodeIndex(int32 NodeIndex) const
{
	return Quest ? Quest->IsValidNodeIndex(NodeIndex) : false;
}

bool UEGQuestContext::IsValidNodeGUID(const FGuid& NodeGUID) const
{
	return Quest ? Quest->IsValidNodeGUID(NodeGUID) : false;
}

FGuid UEGQuestContext::GetNodeGUIDForIndex(int32 NodeIndex) const
{
	return Quest ? Quest->GetNodeGUIDForIndex(NodeIndex) : FGuid{};
}

int32 UEGQuestContext::GetNodeIndexForGUID(const FGuid& NodeGUID) const
{
	return Quest ? Quest->GetNodeIndexForGUID(NodeGUID) : INDEX_NONE;
}

bool UEGQuestContext::EnterNode(int32 NodeIndex)
{
	check(Quest);
	UEGQuestNode* Node = GetMutableNodeFromIndex(NodeIndex);
	if (!IsValid(Node))
	{
		LogErrorWithContext(FString::Printf(TEXT("EnterNode - FAILED because of INVALID NodeIndex = %d"), NodeIndex));
		return false;
	}

	ActiveNodeIndex = NodeIndex;
	SetNodeVisited(Node->GetGUID());

	return Node->HandleNodeEnter(*this);
}

void UEGQuestContext::FireActiveNodeEnterEvents()
{
	if (UEGQuestNode* Node = GetMutableActiveNode())
	{
		Node->FireNodeEnterEvents(*this);
	}
}

void UEGQuestContext::SetNodeVisited(const FGuid& NodeGUID)
{
	History.Add(NodeGUID);
}

UEGQuestNode* UEGQuestContext::GetMutableNodeFromIndex(int32 NodeIndex) const
{
	check(Quest);
	if (!Quest->IsValidNodeIndex(NodeIndex))
	{
		return nullptr;
	}

	return Quest->GetMutableNodeFromIndex(NodeIndex);
}

const UEGQuestNode* UEGQuestContext::GetNodeFromIndex(int32 NodeIndex) const
{
	check(Quest);
	if (!Quest->IsValidNodeIndex(NodeIndex))
	{
		return nullptr;
	}

	return Quest->GetMutableNodeFromIndex(NodeIndex);
}

UEGQuestNode* UEGQuestContext::GetMutableNodeFromGUID(const FGuid& NodeGUID) const
{
	check(Quest);
	if (!Quest->IsValidNodeGUID(NodeGUID))
	{
		return nullptr;
	}

	return Quest->GetMutableNodeFromGUID(NodeGUID);
}

const UEGQuestNode* UEGQuestContext::GetNodeFromGUID(const FGuid& NodeGUID) const
{
	check(Quest);
	if (!Quest->IsValidNodeGUID(NodeGUID))
	{
		return nullptr;
	}

	return Quest->GetMutableNodeFromGUID(NodeGUID);
}

bool UEGQuestContext::StartWithContext(const FString& ContextString, UEGQuestGraph* InQuest)
{
	const FString ContextMessage = ContextString.IsEmpty()
		? TEXT("Start")
		: FString::Printf(TEXT("%s - Start"), *ContextString);

	Quest = InQuest;
	if (!IsValid(Quest))
	{
		return false;
	}

	// The first stage a start node points at becomes active. A start node's arrow is satisfied by
	// the quest starting, so there is nothing to evaluate.
	for (const UEGQuestNode* StartNode : Quest->GetStartNodes())
	{
		for (const FEGQuestEdge& ChildLink : StartNode->GetNodeChildren())
		{
			if (ChildLink.IsValid() && EnterNode(ChildLink.TargetIndex))
			{
				return true;
			}
		}
	}

	LogErrorWithContext(FString::Printf(
		TEXT("%s - FAILED because no start node points at a node to make active"),
		*ContextMessage
	));
	return false;
}

bool UEGQuestContext::StartFromEntry(UEGQuestGraph* InQuest, const UEGQuestNode_Start& Entry)
{
	Quest = InQuest;
	if (!IsValid(Quest)) return false;
	for (const FEGQuestEdge& ChildLink : Entry.GetNodeChildren())
	{
		if (ChildLink.IsValid() && EnterNode(ChildLink.TargetIndex)) return true;
	}
	LogErrorWithContext(FString::Printf(TEXT("StartFromEntry - entry %s has no enterable destination"),
		*Entry.GetGUID().ToString()));
	return false;
}

bool UEGQuestContext::StartWithContextFromNode(
	const FString& ContextString,
	UEGQuestGraph* InQuest,
	int32 StartNodeIndex,
	const FGuid& StartNodeGUID,
	const FEGQuestHistory& StartHistory,
	bool bFireEnterEvents
)
{
	const FString ContextMessage = ContextString.IsEmpty()
		? TEXT("StartFromNode")
		: FString::Printf(TEXT("%s - StartFromNode"), *ContextString);

	Quest = InQuest;
	History = StartHistory;
	if (!IsValid(Quest))
	{
		return false;
	}

	// Get the StartNodeIndex from the GUID
	if (StartNodeGUID.IsValid())
	{
		StartNodeIndex = GetNodeIndexForGUID(StartNodeGUID);
	}

	UEGQuestNode* Node = GetMutableNodeFromIndex(StartNodeIndex);
	if (!IsValid(Node))
	{
		LogErrorWithContext(FString::Printf(
			TEXT("%s - FAILED because StartNodeIndex = %d  is INVALID. For StartNodeGUID = %s"),
			*ContextMessage, StartNodeIndex, *StartNodeGUID.ToString()
		));
		return false;
	}

	if (bFireEnterEvents)
	{
		return EnterNode(StartNodeIndex);
	}

	// Resuming into a stage that was already entered once: take it as active without re-firing its
	// enter events, but still rebuild this context's formatted texts.
	ActiveNodeIndex = StartNodeIndex;
	SetNodeVisited(Node->GetGUID());
	Node->RebuildConstructedText(*this);
	return true;
}

FString UEGQuestContext::GetContextString() const
{
	return FString::Printf(
		TEXT("Quest = `%s`, ActiveNodeIndex = %d"),
		Quest ? *Quest->GetPathName() : TEXT("INVALID"),
		ActiveNodeIndex
	);
}

void UEGQuestContext::LogErrorWithContext(const FString& ErrorMessage) const
{
	FEGQuestLogger::Get().Error(GetErrorMessageWithContext(ErrorMessage));
}

FString UEGQuestContext::GetErrorMessageWithContext(const FString& ErrorMessage) const
{
	return FString::Printf(TEXT("%s.\nContext:\n\t%s"), *ErrorMessage, *GetContextString());
}
