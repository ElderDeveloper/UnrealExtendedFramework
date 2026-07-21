// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "EGQuestObject.h"
#include "EGQuestGraph.h"
#include "Nodes/EGQuestNode.h"
#include "EGQuestMemory.h"
// OnGameplayNotify carries an FGameplayTag. This used to arrive transitively through EGQuestEvent.h.
#include "GameplayTagContainer.h"

#include "EGQuestContext.generated.h"

class UEGQuestNode;
class UEGQuestContext;
class UEGQuestNode_Start;
class UEGQuestComponent;

DECLARE_MULTICAST_DELEGATE_TwoParams(FEGQuestContextNamedEvent, UEGQuestContext&, FName);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FEGQuestContextGameplayNotify, UEGQuestContext&, const FGameplayTag&, float);

/**
 * The authority-side execution state of one running quest instance: which stage is active, what has
 * been visited, and this instance's formatted texts.
 *
 * It does not decide anything. Objective progress and the firing that leaves a stage live in
 * UEGQuestComponent, which owns the replicated snapshot that is the truth clients see.
 */
UCLASS(BlueprintType)
class UNREALEXTENDEDQUEST_API UEGQuestContext : public UEGQuestObject
{
	GENERATED_BODY()
public:
	FEGQuestContextNamedEvent OnNamedQuestEvent;

	// Fired by GameplayNotify quest events. UEGQuestComponent relays this to the game as
	// OnQuestGameplayNotify with the owning instance guid attached.
	FEGQuestContextGameplayNotify OnGameplayNotify;

	//
	// UObject Interface
	//

	UEGQuestContext(const FObjectInitializer& ObjectInitializer);

	// NOTE: Contexts are authority-only execution state and are intentionally NOT replicated.
	// Clients consume the immutable FEGQuestRuntimeSnapshot arrays replicated by UEGQuestComponent.

	//
	// Own methods
	//

	UFUNCTION(BlueprintPure, Category = "Quest|Control")
	bool HasQuestEnded() const { return bQuestEnded; }

	/** Starts from one typed entry instead of arbitrating across the graph's entry list. */
	bool StartFromEntry(UEGQuestGraph* InQuest, const UEGQuestNode_Start& Entry);

	/** Ends this context. Called by the component when a terminal node fires. */
	void MarkQuestEnded() { bQuestEnded = true; }

	/**
	 * Fires the active node's enter events. Separate from EnterNode so the caller can publish the
	 * snapshot first: these events reach game code, which will read the quest back.
	 */
	void FireActiveNodeEnterEvents();

	//
	// Active Node
	//

	UFUNCTION(BlueprintPure, Category = "Quest|ActiveNode")
	int32 GetActiveNodeIndex() const { return ActiveNodeIndex; }

	UFUNCTION(BlueprintPure, Category = "Quest|ActiveNode")
	FGuid GetActiveNodeGUID() const { return GetNodeGUIDForIndex(ActiveNodeIndex); }

	UFUNCTION(BlueprintPure, Category = "Quest|ActiveNode", DisplayName = "Get Active Node")
	UEGQuestNode* GetMutableActiveNode() const { return GetMutableNodeFromIndex(ActiveNodeIndex); }
	const UEGQuestNode* GetActiveNode() const { return GetNodeFromIndex(ActiveNodeIndex); }


	//
	// Data
	//

	UFUNCTION(BlueprintPure, Category = "Quest|Data")
	bool IsValidNodeIndex(int32 NodeIndex) const;

	UFUNCTION(BlueprintPure, Category = "Quest|Data")
	bool IsValidNodeGUID(const FGuid& NodeGUID) const;

	// Gets the GUID for the Node at NodeIndex
	UFUNCTION(BlueprintPure, Category = "Quest|Data", DisplayName = "Get Node GUID For Index")
	FGuid GetNodeGUIDForIndex(int32 NodeIndex) const;

	// Gets the corresponding Node Index for the supplied NodeGUID
	// Returns -1 (INDEX_NONE) if the Node GUID does not exist.
	UFUNCTION(BlueprintPure, Category = "Quest|Data", DisplayName = "Get Node Index For GUID")
	int32 GetNodeIndexForGUID(const FGuid& NodeGUID) const;

	// Returns the GUIDs visited inside this component-owned context.
	UFUNCTION(BlueprintPure, Category = "Quest|Context|History")
	const TSet<FGuid>& GetVisitedNodeGUIDs() const { return History.VisitedNodeGUIDs; }

	// Helper methods to get some Quest properties
	UFUNCTION(BlueprintPure, Category = "Quest|Data")
	UEGQuestGraph* GetQuest() const { return Quest; }

	UFUNCTION(BlueprintPure, Category = "Quest|Data")
	FName GetQuestName() const { check(Quest); return Quest->GetQuestFName(); }

	UFUNCTION(BlueprintPure, Category = "Quest|Data")
	FGuid GetQuestGUID() const { check(Quest); return Quest->GetGUID(); }

	//
	// Per-context formatted texts, keyed by node GUID: a node's own body text with its {arguments}
	// resolved - an objective's text, a stage's description. Runtime formatting must never be written
	// into the shared UEGQuestGraph asset: multiple contexts (one per player/instance) share one
	// asset per process.
	//

	void SetConstructedNodeText(const FGuid& NodeGUID, const FText& InText) { ConstructedNodeTexts.Add(NodeGUID, InText); }
	const FText* GetConstructedNodeText(const FGuid& NodeGUID) const { return ConstructedNodeTexts.Find(NodeGUID); }
	void SetRoleContext(UEGQuestComponent* InComponent, FGuid InRunId) { RoleComponent = InComponent; RoleRunId = InRunId; }
	FText ResolveRoleText(FName RoleName) const;


	// Makes NodeIndex the active node, records the visit and fires its enter events. Nothing is
	// checked here: the caller decides that this node fires.
	bool EnterNode(int32 NodeIndex);

	// Adds the node as visited in the current quest memory
	virtual void SetNodeVisited(const FGuid& NodeGUID);

	// Gets the Node at the NodeIndex index
	UFUNCTION(BlueprintPure, Category = "Quest|Data", DisplayName = "Get Node From Index")
	UEGQuestNode* GetMutableNodeFromIndex(int32 NodeIndex) const;
	const UEGQuestNode* GetNodeFromIndex(int32 NodeIndex) const;

	UFUNCTION(BlueprintPure, Category = "Quest|Data", DisplayName = "Get Node From GUID")
	UEGQuestNode* GetMutableNodeFromGUID(const FGuid& NodeGUID) const;
	const UEGQuestNode* GetNodeFromGUID(const FGuid& NodeGUID) const;

	// Gets the History of this context
	const FEGQuestHistory& GetHistoryOfThisContext() const { return History; }

	// Enters the first stage any start node points at, ignoring entry priority. This is the
	// content-changed restart path; UEGQuestComponent's normal start picks an entry and calls
	// StartFromEntry instead.
	bool Start(UEGQuestGraph* InQuest) { return StartWithContext(TEXT(""), InQuest); }
	bool StartWithContext(const FString& ContextString, UEGQuestGraph* InQuest);
	bool ResumeFromNodeGUID(UEGQuestGraph* InQuest, const FGuid& StartNodeGUID, const FEGQuestHistory& StartHistory, bool bFireEnterEvents)
	{
		return StartWithContextFromNode(TEXT(""), InQuest, INDEX_NONE, StartNodeGUID, StartHistory, bFireEnterEvents);
	}
	bool StartWithContextFromNode(const FString& ContextString, UEGQuestGraph* InQuest, int32 StartNodeIndex,
		const FGuid& StartNodeGUID, const FEGQuestHistory& StartHistory, bool bFireEnterEvents);

	UFUNCTION(BlueprintPure, Category = "Quest|Context")
	FString GetContextString() const;

protected:
	void LogErrorWithContext(const FString& ErrorMessage) const;
	FString GetErrorMessageWithContext(const FString& ErrorMessage) const;

protected:
	// Current Quest used in this context at runtime.
	UPROPERTY()
	UEGQuestGraph* Quest = nullptr;

	// The index of the active node in the quests Nodes array. Always a stage on a running quest.
	int32 ActiveNodeIndex = INDEX_NONE;

	// Node indices visited in this specific Quest instance (isn't serialized)
	// History for this Context only
	FEGQuestHistory History;

	// Formatted node texts keyed by node GUID, owned by this context (see SetConstructedNodeText).
	UPROPERTY()
	TMap<FGuid, FText> ConstructedNodeTexts;

	UPROPERTY(Transient)
	TObjectPtr<UEGQuestComponent> RoleComponent = nullptr;
	FGuid RoleRunId;

	// cache the result of the last ChooseOption call
	bool bQuestEnded = false;
};
