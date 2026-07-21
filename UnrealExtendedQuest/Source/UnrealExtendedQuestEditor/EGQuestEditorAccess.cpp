// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#include "EGQuestEditorAccess.h"

#include "Engine/Blueprint.h"
#include "Kismet2/KismetEditorUtilities.h"

#include "UnrealExtendedQuest/EGQuestGraph.h"
#include "Editor/Graph/EGQuestEdGraph.h"
#include "Editor/Graph/EGQuestEdGraphSchema.h"
#include "UnrealExtendedQuestEditor/EGQuestEditorUtilities.h"
#include "Editor/Nodes/EGQuestGraphNode.h"
#include "Editor/EGQuestCompiler.h"
#include "UnrealExtendedQuest/Nodes/EGQuestNode.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FEGQuestEditorAccess
void FEGQuestEditorAccess::UpdateGraphNodeEdges(UEdGraphNode* GraphNode)
{
	// The runtime edges are compiler output now; the graph node never mirrors them live.
}

UEdGraph* FEGQuestEditorAccess::CreateNewQuestEdGraph(UEGQuestGraph* Quest) const
{
	UEGQuestEdGraph* QuestEdGraph = CastChecked<UEGQuestEdGraph>(FEGQuestEditorUtilities::CreateNewGraph(Quest, NAME_None,
										UEGQuestEdGraph::StaticClass(), UEGQuestEdGraphSchema::StaticClass()));
	QuestEdGraph->bAllowDeletion = false;

	return CastChecked<UEdGraph>(QuestEdGraph);
}

/** Compiles the quest nodes from the graph nodes. Meaning it transforms the graph data -> (into) quest data. */
void FEGQuestEditorAccess::CompileQuestNodesFromGraphNodes(UEGQuestGraph* Quest) const
{
	FCompilerResultsLog MessageLog;
	const UEGQuestPluginSettings* Settings = GetDefault<UEGQuestPluginSettings>();
	FEGQuestCompilerContext CompilerContext(Quest, Settings, MessageLog);
	CompilerContext.Compile();
}

void FEGQuestEditorAccess::ValidateQuest(const UEGQuestGraph* Quest, FEGQuestDiagnostics& OutDiagnostics) const
{
	OutDiagnostics.Reset();
	if (Quest)
	{
		FEGQuestCompilerContext::ValidateQuest(*Quest, OutDiagnostics);
	}
}

/** Removes all nodes from the graph. */
void FEGQuestEditorAccess::RemoveAllGraphNodes(UEGQuestGraph* Quest) const
{
	CastChecked<UEGQuestEdGraph>(Quest->GetGraph())->RemoveAllNodes();

	// Clear the references from the Quest Nodes
	for (UEGQuestNode* Node : Quest->GetMutableStartNodes())
	{
		Node->ClearGraphNode();
	}
	for (UEGQuestNode* Node : Quest->GetNodes())
	{
		Node->ClearGraphNode();
	}
}

/** Updates the Quest to match the version UseOnlyOneOutputAndInputPin */
void FEGQuestEditorAccess::UpdateQuestToVersion_UseOnlyOneOutputAndInputPin(UEGQuestGraph* Quest) const
{
	// Ancient-version migration. The stage-model editor rebuilds its whole graph from the runtime
	// arrays anyway, so the honest migration is simply a rebuild.
	RemoveAllGraphNodes(Quest);
	UEGQuestEdGraph* QuestEdGraph = CastChecked<UEGQuestEdGraph>(Quest->GetGraph());
	QuestEdGraph->CreateGraphNodesFromQuest();
	QuestEdGraph->LinkGraphNodesFromQuest();
	QuestEdGraph->AutoPositionGraphNodes();
	Quest->MarkPackageDirty();
}


void FEGQuestEditorAccess::RefreshQuestScriptBlueprint(UEGQuestGraph* Quest) const
{
	UBlueprint* ScriptBlueprint = Quest ? Quest->GetQuestScriptBlueprint() : nullptr;
	if (!ScriptBlueprint)
	{
		return;
	}

	// Stale after an edit (dirty) or after the asset was duplicated: duplication copies the
	// blueprint (it lives inside the asset) but not its generated class, which still belongs to the
	// source package until a compile mints one here.
	const bool bClassBelongsElsewhere =
		!ScriptBlueprint->GeneratedClass || ScriptBlueprint->GeneratedClass->GetOutermost() != Quest->GetOutermost();
	if (bClassBelongsElsewhere || ScriptBlueprint->Status == BS_Dirty)
	{
		FKismetEditorUtilities::CompileBlueprint(ScriptBlueprint, EBlueprintCompileOptions::SkipGarbageCollection);
	}

	Quest->SetQuestScriptClass(Cast<UClass>(ScriptBlueprint->GeneratedClass));
}

void FEGQuestEditorAccess::SetNewOuterForObjectFromGraphNode(UObject* Object, UEdGraphNode* GraphNode) const
{
	if (!Object || !GraphNode)
	{
		return;
	}

	UEGQuestNode* ClosestNode = FEGQuestEditorUtilities::GetClosestNodeFromGraphNode(GraphNode);
	if (!ClosestNode)
	{
		return;
	}

	Object->Rename(nullptr, ClosestNode, REN_DontCreateRedirectors);
}
