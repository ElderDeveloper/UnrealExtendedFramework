// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "UnrealExtendedQuest/IEGQuestEditorAccess.h"
#include "UnrealExtendedQuestEditor/EGQuestEditorUtilities.h"

/**
 * Implementation of the interface for quest graph interaction between QuestPlugin module <-> QuestPluginEditor module.
 * Set in UEGQuestEdGraph constructor for Each Quest
 */
class UNREALEXTENDEDQUESTEDITOR_API FEGQuestEditorAccess : public IEGQuestEditorAccess
{
public:
	FEGQuestEditorAccess() {}
	~FEGQuestEditorAccess() {}

	void UpdateGraphNodeEdges(UEdGraphNode* GraphNode) override;
	UEdGraph* CreateNewQuestEdGraph(UEGQuestGraph* Quest) const override;
	void CompileQuestNodesFromGraphNodes(UEGQuestGraph* Quest) const override;
	void ValidateQuest(const UEGQuestGraph* Quest, FEGQuestDiagnostics& OutDiagnostics) const override;
	void RemoveAllGraphNodes(UEGQuestGraph* Quest) const override;
	void UpdateQuestToVersion_UseOnlyOneOutputAndInputPin(UEGQuestGraph* Quest) const override;
	void SetNewOuterForObjectFromGraphNode(UObject* Object, UEdGraphNode* GraphNode) const override;
	void RefreshQuestScriptBlueprint(UEGQuestGraph* Quest) const override;

	bool AreQuestNodesInSyncWithGraphNodes(UEGQuestGraph* Quest) const override
	{
		return FEGQuestEditorUtilities::AreQuestNodesInSyncWithGraphNodes(Quest);
	}
};
