// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#include "EGQuestCompiler.h"

#include "EdGraph/EdGraphPin.h"
#include "UnrealExtendedQuestEditor/EGQuestPluginEditorModule.h"
#include "UnrealExtendedQuestEditor/Editor/Graph/EGQuestEdGraph.h"
#include "UnrealExtendedQuestEditor/EGQuestEditorUtilities.h"
#include "UnrealExtendedQuestEditor/Editor/Nodes/EGQuestGraphNode_Root.h"
#include "UnrealExtendedQuest/Nodes/EGQuestNode.h"
#include "UnrealExtendedQuest/Nodes/EGQuestNode_Objective.h"
#include "UnrealExtendedQuest/Nodes/EGQuestNode_Start.h"
#include "UnrealExtendedQuest/Nodes/EGQuestNode_Stage.h"
#include "UnrealExtendedQuest/Nodes/EGQuestNode_End.h"
#include "UnrealExtendedQuest/EGQuestGraph.h"

#define LOCTEXT_NAMESPACE "QuestCompiler"

namespace
{
	FString GuidSortKey(const FGuid& Guid)
	{
		return Guid.ToString(EGuidFormats::Digits);
	}

	const UEGQuestGraphNode* GetLinkedQuestGraphNode(const UEdGraphPin* LinkedPin)
	{
		return LinkedPin ? Cast<UEGQuestGraphNode>(LinkedPin->GetOwningNodeUnchecked()) : nullptr;
	}

	FGuid GetLinkedDestinationGuid(const UEdGraphPin* LinkedPin)
	{
		if (const UEGQuestGraphNode* Node = GetLinkedQuestGraphNode(LinkedPin))
		{
			return Node->GetQuestNode().GetGUID();
		}
		return FGuid();
	}

	bool ResolveRouteSource(
		const UEGQuestGraphNode& GraphNode,
		const UEdGraphPin& Pin,
		const UEGQuestNode*& OutSource,
		EEGQuestArrowOutcome& OutOutcome)
	{
		OutOutcome = EEGQuestArrowOutcome::Success;
		if (GraphNode.IsStageNode())
		{
			OutSource = GraphNode.FindObjectiveForPin(Pin, OutOutcome);
			return OutSource != nullptr;
		}

		OutSource = &GraphNode.GetQuestNode();
		return OutSource->EmitsRoutes();
	}

	FText WithFixHint(const FEGQuestDiagnostic& Diagnostic)
	{
		return Diagnostic.FixHint.IsEmpty()
			? Diagnostic.Message
			: FText::Format(LOCTEXT("DiagnosticWithFix", "{0} ({1})"), Diagnostic.Message, Diagnostic.FixHint);
	}
}

void FEGQuestCompilerContext::Compile()
{
	check(Quest);
	Diagnostics.Reset();

	UEGQuestEdGraph* QuestEdGraph = CastChecked<UEGQuestEdGraph>(Quest->GetGraph());
	QuestGraphNodes.Empty();
	for (UEGQuestGraphNode* GraphNode : QuestEdGraph->GetAllQuestGraphNodes())
	{
		if (!GraphNode->IsRootNode())
		{
			QuestGraphNodes.Add(GraphNode);
		}
	}

	ResultQuestNodes.Empty();
	NodeToIndex.Empty();
	VisitedNodes.Empty();

	// Step 1. Entry points are authored runtime nodes too: mint their identity, migrate legacy
	// canvas ordering once, then order by the authored field forever after.
	GraphNodeRoots = QuestEdGraph->GetRootGraphNodes();
	MintRootGUIDs();
	SeedEntryPrioritiesFromLayout();
	OrderRootGraphNodes();

	TArray<UEGQuestNode*> StartNodes;
	for (UEGQuestGraphNode_Root* RootNode : GraphNodeRoots)
	{
		StartNodes.Add(RootNode->GetMutableQuestNode());
	}
	Quest->SetStartNodes(StartNodes);

	// Step 2. Breadth-first index assignment. Layout only affects otherwise-semantic-free indices.
	for (UEGQuestGraphNode_Root* RootNode : GraphNodeRoots)
	{
		VisitedNodes.Add(RootNode);
		RootNode->SetNodeDepth(0);
	}
	TArray<UEGQuestGraphNode*> Queue;
	for (UEGQuestGraphNode_Root* RootNode : GraphNodeRoots)
	{
		for (UEGQuestGraphNode* ChildNode : GetOrderedChildNodes(RootNode))
		{
			if (!VisitedNodes.Contains(ChildNode))
			{
				VisitedNodes.Add(ChildNode);
				ChildNode->SetNodeDepth(1);
				Queue.Add(ChildNode);
			}
		}
	}
	for (int32 QueueIndex = 0; QueueIndex < Queue.Num(); ++QueueIndex)
	{
		UEGQuestGraphNode* GraphNode = Queue[QueueIndex];
		AssignIndices(GraphNode);
		for (UEGQuestGraphNode* ChildNode : GetOrderedChildNodes(GraphNode))
		{
			if (!VisitedNodes.Contains(ChildNode))
			{
				VisitedNodes.Add(ChildNode);
				ChildNode->SetNodeDepth(GraphNode->GetNodeDepth() + 1);
				Queue.Add(ChildNode);
			}
		}
	}

	// Step 3. Preserve orphaned authored work and give it stable runtime identity.
	for (UEGQuestGraphNode* GraphNode : QuestGraphNodes)
	{
		if (!VisitedNodes.Contains(GraphNode))
		{
			VisitedNodes.Add(GraphNode);
			AssignIndices(GraphNode);
		}
	}

	// Step 4. Destination GUIDs now exist, so legacy route order can be captured and runtime edges
	// can be emitted in authored priority order.
	SeedRoutePrioritiesFromLayout();
	BuildRuntimeEdges();

	// Step 5. Publish compiler output.
	Quest->EmptyNodesGUIDToIndexMap();
	Quest->SetNodes(ResultQuestNodes);

	// Step 6. One canonical, read-only rule pass feeds the message log, node badges, Data Validation,
	// and the commandlet.
	FEGQuestDiagnostics Validated;
	ValidateQuest(*Quest, Validated);
	for (const FEGQuestDiagnostic& Diagnostic : Validated.Items)
	{
		AddDiagnostic(Diagnostic.RuleId, Diagnostic.Severity, Diagnostic.ElementGuid,
			Diagnostic.Message, Diagnostic.FixHint);
	}
	ApplyDiagnosticsToGraphNodes();

	Quest->PostEditChange();
	FEGQuestEditorUtilities::RefreshQuestEditorForGraph(QuestEdGraph);
}

void FEGQuestCompilerContext::MintRootGUIDs()
{
	for (UEGQuestGraphNode_Root* RootNode : GraphNodeRoots)
	{
		if (RootNode && !RootNode->GetQuestNode().HasGUID())
		{
			RootNode->GetMutableQuestNode()->RegenerateGUID();
		}
	}
}

void FEGQuestCompilerContext::SeedEntryPrioritiesFromLayout()
{
	if (!Quest->NeedsPriorityMigration())
	{
		return;
	}

	TArray<UEGQuestGraphNode_Root*> LegacyOrder = GraphNodeRoots;
	LegacyOrder.Sort([](const UEGQuestGraphNode_Root& A, const UEGQuestGraphNode_Root& B)
	{
		if (A.GetPosition().X != B.GetPosition().X)
		{
			return A.GetPosition().X < B.GetPosition().X;
		}
		return A.GetPosition().Y < B.GetPosition().Y;
	});
	for (int32 Index = 0; Index < LegacyOrder.Num(); ++Index)
	{
		CastChecked<UEGQuestNode_Start>(LegacyOrder[Index]->GetMutableQuestNode())->SetEntryPriority(Index);
	}
}

void FEGQuestCompilerContext::OrderRootGraphNodes()
{
	GraphNodeRoots.Sort([](const UEGQuestGraphNode_Root& A, const UEGQuestGraphNode_Root& B)
	{
		const UEGQuestNode_Start& StartA = *CastChecked<UEGQuestNode_Start>(&A.GetQuestNode());
		const UEGQuestNode_Start& StartB = *CastChecked<UEGQuestNode_Start>(&B.GetQuestNode());
		if (StartA.GetEntryPriority() != StartB.GetEntryPriority())
		{
			return StartA.GetEntryPriority() < StartB.GetEntryPriority();
		}
		return GuidSortKey(StartA.GetGUID()) < GuidSortKey(StartB.GetGUID());
	});
}

void FEGQuestCompilerContext::AssignIndices(UEGQuestGraphNode* GraphNode)
{
	UEGQuestNode* QuestNode = GraphNode->GetMutableQuestNode();
	if (!QuestNode->HasGUID())
	{
		QuestNode->RegenerateGUID();
	}

	const int32 NodeIndex = ResultQuestNodes.Add(QuestNode);
	NodeToIndex.Add(QuestNode, NodeIndex);
	GraphNode->SetQuestNodeIndex(NodeIndex);

	if (GraphNode->IsStageNode())
	{
		for (UEGQuestNode_Objective* Objective : GraphNode->GetObjectives())
		{
			if (!Objective)
			{
				continue;
			}
			if (!Objective->HasGUID())
			{
				Objective->RegenerateGUID();
			}
			const int32 ObjectiveIndex = ResultQuestNodes.Add(Objective);
			NodeToIndex.Add(Objective, ObjectiveIndex);
		}
	}
}

TArray<UEGQuestGraphNode*> FEGQuestCompilerContext::GetOrderedChildNodes(const UEGQuestGraphNode* GraphNode) const
{
	TArray<UEGQuestGraphNode*> ChildNodes;
	for (const UEdGraphPin* OutputPin : GraphNode->GetOutputPins())
	{
		TArray<UEdGraphPin*> SortedLinks = OutputPin->LinkedTo;
		SortedLinks.Sort([](const UEdGraphPin& A, const UEdGraphPin& B)
		{
			const UEdGraphNode* NodeA = A.GetOwningNode();
			const UEdGraphNode* NodeB = B.GetOwningNode();
			return NodeA->NodePosX != NodeB->NodePosX
				? NodeA->NodePosX < NodeB->NodePosX
				: NodeA->NodePosY < NodeB->NodePosY;
		});
		for (const UEdGraphPin* TargetInputPin : SortedLinks)
		{
			if (UEGQuestGraphNode* ChildNode = Cast<UEGQuestGraphNode>(TargetInputPin->GetOwningNodeUnchecked()))
			{
				ChildNodes.AddUnique(ChildNode);
			}
		}
	}
	return ChildNodes;
}

void FEGQuestCompilerContext::SeedRoutePrioritiesFromLayout()
{
	if (!Quest->NeedsPriorityMigration())
	{
		return;
	}

	auto SeedNode = [](UEGQuestGraphNode& GraphNode)
	{
		TMap<UEGQuestNode*, TArray<FEGQuestRoutePriority>> RoutesBySource;
		for (const UEdGraphPin* Pin : GraphNode.GetOutputPins())
		{
			const UEGQuestNode* SourceConst = nullptr;
			EEGQuestArrowOutcome Outcome = EEGQuestArrowOutcome::Success;
			if (!ResolveRouteSource(GraphNode, *Pin, SourceConst, Outcome))
			{
				continue;
			}

			TArray<UEdGraphPin*> Links = Pin->LinkedTo;
			Links.Sort([](const UEdGraphPin& A, const UEdGraphPin& B)
			{
				const UEdGraphNode* NodeA = A.GetOwningNode();
				const UEdGraphNode* NodeB = B.GetOwningNode();
				return NodeA->NodePosX != NodeB->NodePosX
					? NodeA->NodePosX < NodeB->NodePosX
					: NodeA->NodePosY < NodeB->NodePosY;
			});

			TArray<FEGQuestRoutePriority>& Routes = RoutesBySource.FindOrAdd(const_cast<UEGQuestNode*>(SourceConst));
			for (int32 Index = 0; Index < Links.Num(); ++Index)
			{
				const FGuid DestinationGuid = GetLinkedDestinationGuid(Links[Index]);
				if (!DestinationGuid.IsValid())
				{
					continue;
				}
				FEGQuestRoutePriority& Route = Routes.AddDefaulted_GetRef();
				Route.DestinationGuid = DestinationGuid;
				Route.Outcome = Outcome;
				Route.Priority = Index;
			}
		}

		for (TPair<UEGQuestNode*, TArray<FEGQuestRoutePriority>>& Pair : RoutesBySource)
		{
			Pair.Key->SetRoutePriorities(Pair.Value);
		}
	};

	for (UEGQuestGraphNode_Root* RootNode : GraphNodeRoots)
	{
		SeedNode(*RootNode);
	}
	for (UEGQuestGraphNode* GraphNode : QuestGraphNodes)
	{
		SeedNode(*GraphNode);
	}
	Quest->ConsumePriorityMigration();
}

TArray<FEGQuestRoutePriority> FEGQuestCompilerContext::ReconcileRoutePrioritiesForPin(
	const UEGQuestNode& SourceNode, const UEdGraphPin* Pin, EEGQuestArrowOutcome Outcome) const
{
	TArray<FEGQuestRoutePriority> Result;
	if (!Pin)
	{
		return Result;
	}

	int32 NextPriority = SourceNode.GetMaxRoutePriority(Outcome) + 1;
	for (const UEdGraphPin* LinkedPin : Pin->LinkedTo)
	{
		const FGuid DestinationGuid = GetLinkedDestinationGuid(LinkedPin);
		if (!DestinationGuid.IsValid())
		{
			continue;
		}

		FEGQuestRoutePriority Route;
		Route.DestinationGuid = DestinationGuid;
		Route.Outcome = Outcome;
		const int32 ExistingPriority = SourceNode.FindRoutePriority(DestinationGuid, Outcome);
		Route.Priority = ExistingPriority == INDEX_NONE ? NextPriority++ : ExistingPriority;
		Result.Add(Route);
	}
	return Result;
}

TArray<const UEdGraphPin*> FEGQuestCompilerContext::GetOrderedLinksForPin(
	const UEdGraphPin* Pin, EEGQuestArrowOutcome Outcome, const TArray<FEGQuestRoutePriority>& Group) const
{
	TArray<const UEdGraphPin*> Result;
	if (!Pin)
	{
		return Result;
	}
	for (const UEdGraphPin* LinkedPin : Pin->LinkedTo)
	{
		Result.Add(LinkedPin);
	}

	auto PriorityFor = [&Group, Outcome](const FGuid& DestinationGuid)
	{
		for (const FEGQuestRoutePriority& Route : Group)
		{
			if (Route.Outcome == Outcome && Route.DestinationGuid == DestinationGuid)
			{
				return Route.Priority;
			}
		}
		return MAX_int32;
	};

	Result.Sort([&PriorityFor](const UEdGraphPin& A, const UEdGraphPin& B)
	{
		const FGuid GuidA = GetLinkedDestinationGuid(&A);
		const FGuid GuidB = GetLinkedDestinationGuid(&B);
		const int32 PriorityA = PriorityFor(GuidA);
		const int32 PriorityB = PriorityFor(GuidB);
		return PriorityA != PriorityB ? PriorityA < PriorityB : GuidSortKey(GuidA) < GuidSortKey(GuidB);
	});
	return Result;
}

void FEGQuestCompilerContext::BuildRuntimeEdges()
{
	auto BuildEdgesForSource = [this](UEGQuestGraphNode& GraphNode, UEGQuestNode& SourceNode, const TArray<const UEdGraphPin*>& Pins)
	{
		TArray<FEGQuestRoutePriority> ReconciledRoutes;
		TArray<FEGQuestEdge> RuntimeEdges;

		for (const UEdGraphPin* Pin : Pins)
		{
			const UEGQuestNode* ResolvedSource = nullptr;
			EEGQuestArrowOutcome Outcome = EEGQuestArrowOutcome::Success;
			if (!Pin || !ResolveRouteSource(GraphNode, *Pin, ResolvedSource, Outcome) || ResolvedSource != &SourceNode)
			{
				continue;
			}

			const TArray<FEGQuestRoutePriority> Group = ReconcileRoutePrioritiesForPin(SourceNode, Pin, Outcome);
			ReconciledRoutes.Append(Group);
			for (const UEdGraphPin* TargetPin : GetOrderedLinksForPin(Pin, Outcome, Group))
			{
				const UEGQuestGraphNode* TargetNode = GetLinkedQuestGraphNode(TargetPin);
				if (!TargetNode)
				{
					continue;
				}
				const int32* TargetIndex = NodeToIndex.Find(&TargetNode->GetQuestNode());
				if (!TargetIndex)
				{
					continue;
				}

				FEGQuestEdge& Edge = RuntimeEdges.AddDefaulted_GetRef();
				Edge.TargetIndex = *TargetIndex;
				Edge.Outcome = Outcome;
			}
		}

		SourceNode.SetRoutePriorities(ReconciledRoutes);
		SourceNode.SetNodeChildren(RuntimeEdges);
	};

	for (UEGQuestGraphNode_Root* RootNode : GraphNodeRoots)
	{
		TArray<const UEdGraphPin*> Pins;
		for (const UEdGraphPin* Pin : RootNode->GetOutputPins())
		{
			Pins.Add(Pin);
		}
		BuildEdgesForSource(*RootNode, *RootNode->GetMutableQuestNode(), Pins);
	}

	for (UEGQuestGraphNode* GraphNode : QuestGraphNodes)
	{
		if (GraphNode->IsStageNode())
		{
			TArray<FEGQuestEdge> OwnershipEdges;
			for (UEGQuestNode_Objective* Objective : GraphNode->GetObjectives())
			{
				if (!Objective)
				{
					continue;
				}
				FEGQuestEdge& OwnershipEdge = OwnershipEdges.AddDefaulted_GetRef();
				OwnershipEdge.TargetIndex = NodeToIndex.FindChecked(Objective);

				TArray<const UEdGraphPin*> Pins;
				if (const UEdGraphPin* SuccessPin = GraphNode->FindObjectivePin(*Objective, EEGQuestArrowOutcome::Success))
				{
					Pins.Add(SuccessPin);
				}
				if (const UEdGraphPin* FailPin = GraphNode->FindObjectivePin(*Objective, EEGQuestArrowOutcome::Fail))
				{
					Pins.Add(FailPin);
				}
				BuildEdgesForSource(*GraphNode, *Objective, Pins);
			}
			GraphNode->GetMutableQuestNode()->SetRoutePriorities({});
			GraphNode->GetMutableQuestNode()->SetNodeChildren(OwnershipEdges);
		}
		else
		{
			TArray<const UEdGraphPin*> Pins;
			for (const UEdGraphPin* Pin : GraphNode->GetOutputPins())
			{
				Pins.Add(Pin);
			}
			BuildEdgesForSource(*GraphNode, *GraphNode->GetMutableQuestNode(), Pins);
		}
	}
}

void FEGQuestCompilerContext::ValidateQuest(const UEGQuestGraph& QuestToValidate, FEGQuestDiagnostics& OutDiagnostics)
{
	OutDiagnostics.Reset();
	auto Add = [&OutDiagnostics](const TCHAR* Rule, EEGQuestDiagnosticSeverity Severity, const FGuid& Guid,
		FText Message, FText Hint = FText::GetEmpty())
	{
		OutDiagnostics.Add(FName(Rule), Severity, Guid, MoveTemp(Message), MoveTemp(Hint));
	};

	if (QuestToValidate.GetDefinitionId().IsNone())
	{
		Add(EGQuestDiagnosticRule::MissingDefinitionId, EEGQuestDiagnosticSeverity::Error, FGuid(),
			LOCTEXT("MissingDefinitionId", "Quest has no DefinitionId."),
			LOCTEXT("MissingDefinitionIdFix", "Assign a namespaced id such as Project.QuestName."));
	}
	else if (!QuestToValidate.IsDefinitionIdWellFormed())
	{
		Add(EGQuestDiagnosticRule::MalformedDefinitionId, EEGQuestDiagnosticSeverity::Error, FGuid(),
			FText::Format(LOCTEXT("MalformedDefinitionId", "DefinitionId '{0}' is not in Namespace.Name form."),
				FText::FromName(QuestToValidate.GetDefinitionId())),
			LOCTEXT("MalformedDefinitionIdFix", "Use exactly one dot with a non-empty namespace and name."));
	}

	const UEGQuestEdGraph* EdGraph = Cast<UEGQuestEdGraph>(QuestToValidate.GetGraph());
	if (!EdGraph)
	{
		Add(EGQuestDiagnosticRule::NoStartNode, EEGQuestDiagnosticSeverity::Error, FGuid(),
			LOCTEXT("MissingEditorGraph", "Quest has no editable graph."));
		return;
	}

	const TArray<UEGQuestGraphNode_Root*> Roots = EdGraph->GetRootGraphNodes();
	if (Roots.Num() == 0)
	{
		Add(EGQuestDiagnosticRule::NoStartNode, EEGQuestDiagnosticSeverity::Error, FGuid(),
			LOCTEXT("NoStartNode", "Quest has no start node."),
			LOCTEXT("NoStartNodeFix", "Add one start node and connect it to the first stage."));
	}

	TMap<int32, FGuid> EntryPriorityOwners;
	int32 MainEntryCount = 0;
	TMap<FName, FGuid> TrackNameOwners;
	for (const UEGQuestGraphNode_Root* Root : Roots)
	{
		const UEGQuestNode& RootQuestNode = Root->GetQuestNode();
		const FGuid RootGuid = RootQuestNode.GetGUID();
		if (!RootGuid.IsValid())
		{
			Add(EGQuestDiagnosticRule::MissingNodeGUID, EEGQuestDiagnosticSeverity::Error, RootGuid,
				LOCTEXT("StartMissingGuid", "Start node has no GUID."),
				LOCTEXT("StartMissingGuidFix", "Compile the quest once to mint stable element identity."));
		}

		const UEGQuestNode_Start* Start = Cast<UEGQuestNode_Start>(&RootQuestNode);
		if (Start)
		{
			if (Start->GetTrackType() == EEGQuestTrackType::Main) ++MainEntryCount;
			const FName TrackName = Start->GetTrackName();
			if (const FGuid* ExistingTrack = TrackNameOwners.Find(TrackName))
			{
				Add(EGQuestDiagnosticRule::DuplicateTrackName, EEGQuestDiagnosticSeverity::Error, RootGuid,
					FText::Format(LOCTEXT("DuplicateTrackName", "Track name '{0}' is already owned by entry {1}."),
						FText::FromName(TrackName), FText::FromString(ExistingTrack->ToString())),
					LOCTEXT("DuplicateTrackNameFix", "Give every typed entry a unique TrackName."));
			}
			else TrackNameOwners.Add(TrackName, RootGuid);
			if (const FGuid* Existing = EntryPriorityOwners.Find(Start->GetEntryPriority()))
			{
				Add(EGQuestDiagnosticRule::DuplicateEntryPriority, EEGQuestDiagnosticSeverity::Error, RootGuid,
					FText::Format(LOCTEXT("DuplicateEntryPriority", "Entry priority {0} is also used by start node {1}."),
						Start->GetEntryPriority(), FText::FromString(Existing->ToString())),
					LOCTEXT("DuplicateEntryPriorityFix", "Give every start node a unique EntryPriority."));
			}
			else
			{
				EntryPriorityOwners.Add(Start->GetEntryPriority(), RootGuid);
			}
		}

		bool bHasChild = false;
		for (const UEdGraphPin* Pin : Root->GetOutputPins())
		{
			bHasChild |= Pin && Pin->LinkedTo.Num() > 0;
		}
		if (!bHasChild)
		{
			Add(EGQuestDiagnosticRule::StartWithoutChildren, EEGQuestDiagnosticSeverity::Error, RootGuid,
				LOCTEXT("StartWithoutChildren", "Start node has no destination."),
				LOCTEXT("StartWithoutChildrenFix", "Connect it to a stage or remove the unused entry point."));
		}
	}
	if (MainEntryCount != 1)
	{
		Add(EGQuestDiagnosticRule::MainEntryCount, EEGQuestDiagnosticSeverity::Error, FGuid(),
			FText::Format(LOCTEXT("MainEntryCount", "Quest has {0} Main entries; exactly one is required."), MainEntryCount),
			LOCTEXT("MainEntryCountFix", "Mark one entry Main and every watcher entry Sentinel."));
	}

	// Runtime-edge reachability assigns every non-End node to exactly one track. Ends are global and
	// may be targeted by any track; sharing any other node is a cross-track route.
	TMap<int32, FName> NodeTrackOwners;
	for (const UEGQuestNode* EntryNode : QuestToValidate.GetStartNodes())
	{
		const UEGQuestNode_Start* Entry = Cast<UEGQuestNode_Start>(EntryNode);
		if (!Entry) continue;
		TArray<int32> Pending;
		for (const FEGQuestEdge& Edge : Entry->GetNodeChildren()) if (Edge.IsValid()) Pending.Add(Edge.TargetIndex);
		TSet<int32> Visited;
		int32 StageCount = 0;
		while (!Pending.IsEmpty())
		{
			const int32 Index = Pending.Pop(EAllowShrinking::No);
			if (Visited.Contains(Index) || !QuestToValidate.GetNodes().IsValidIndex(Index)) continue;
			Visited.Add(Index);
			const UEGQuestNode* Node = QuestToValidate.GetNodes()[Index];
			if (!Node || Node->IsA<UEGQuestNode_End>()) continue;
			if (const FName* Existing = NodeTrackOwners.Find(Index); Existing && *Existing != Entry->GetTrackName())
			{
				Add(EGQuestDiagnosticRule::CrossTrackRoute, EEGQuestDiagnosticSeverity::Error, Node->GetGUID(),
					FText::Format(LOCTEXT("CrossTrackRoute", "Node is reachable from tracks '{0}' and '{1}'."),
						FText::FromName(*Existing), FText::FromName(Entry->GetTrackName())),
					LOCTEXT("CrossTrackRouteFix", "Remove the cross-track arrow; communicate between tracks through facts."));
				continue;
			}
			NodeTrackOwners.Add(Index, Entry->GetTrackName());
			if (Node->IsA<UEGQuestNode_Stage>()) ++StageCount;
			for (const FEGQuestEdge& Edge : Node->GetNodeChildren()) if (Edge.IsValid()) Pending.Add(Edge.TargetIndex);
		}
		if (Entry->GetTrackType() == EEGQuestTrackType::Sentinel && StageCount > 3)
		{
			Add(EGQuestDiagnosticRule::SentinelStageBudget, EEGQuestDiagnosticSeverity::Warning, Entry->GetGUID(),
				FText::Format(LOCTEXT("SentinelStageBudget", "Sentinel track '{0}' contains {1} stages."),
					FText::FromName(Entry->GetTrackName()), StageCount),
				LOCTEXT("SentinelStageBudgetFix", "Keep sentinel tracks small; use child-quest composition for parallel story arcs."));
		}
	}

	const TArray<UEGQuestGraphNode*> GraphNodes = EdGraph->GetAllQuestGraphNodes();
	for (const UEGQuestGraphNode* GraphNode : GraphNodes)
	{
		if (GraphNode->IsRootNode())
		{
			continue;
		}
		const FGuid NodeGuid = GraphNode->GetQuestNode().GetGUID();
		if (!NodeGuid.IsValid())
		{
			Add(EGQuestDiagnosticRule::MissingNodeGUID, EEGQuestDiagnosticSeverity::Error, NodeGuid,
				LOCTEXT("NodeMissingGuid", "Node has no GUID."),
				LOCTEXT("NodeMissingGuidFix", "Compile the quest once to mint stable element identity."));
		}
		for (const UEGQuestNode_Objective* Objective : GraphNode->GetObjectives())
		{
			if (Objective && !Objective->GetGUID().IsValid())
			{
				Add(EGQuestDiagnosticRule::MissingNodeGUID, EEGQuestDiagnosticSeverity::Error, Objective->GetGUID(),
					LOCTEXT("ObjectiveMissingGuid", "Objective row has no GUID."),
					LOCTEXT("ObjectiveMissingGuidFix", "Compile the quest once to mint stable element identity."));
			}
		}
		GraphNode->CollectDiagnostics(OutDiagnostics);
	}

	// Build the route set represented by the pins, grouped by the runtime source node. The authored
	// priority array must have exactly one entry for each member of that set.
	TMap<const UEGQuestNode*, TMap<EEGQuestArrowOutcome, TSet<FGuid>>> ActualRoutes;
	for (const UEGQuestGraphNode* GraphNode : GraphNodes)
	{
		for (const UEdGraphPin* Pin : GraphNode->GetOutputPins())
		{
			const UEGQuestNode* Source = nullptr;
			EEGQuestArrowOutcome Outcome = EEGQuestArrowOutcome::Success;
			if (!Pin || !ResolveRouteSource(*GraphNode, *Pin, Source, Outcome))
			{
				continue;
			}
			TSet<FGuid>& Destinations = ActualRoutes.FindOrAdd(Source).FindOrAdd(Outcome);
			for (const UEdGraphPin* LinkedPin : Pin->LinkedTo)
			{
				const FGuid DestinationGuid = GetLinkedDestinationGuid(LinkedPin);
				if (!DestinationGuid.IsValid())
				{
					Add(EGQuestDiagnosticRule::DanglingWire, EEGQuestDiagnosticSeverity::Error, Source->GetGUID(),
						LOCTEXT("DanglingWire", "An output wire points at a node that cannot be emitted."),
						LOCTEXT("DanglingWireFix", "Break and reconnect the wire."));
					continue;
				}
				Destinations.Add(DestinationGuid);
			}
		}
	}

	for (const TPair<const UEGQuestNode*, TMap<EEGQuestArrowOutcome, TSet<FGuid>>>& SourcePair : ActualRoutes)
	{
		const UEGQuestNode* Source = SourcePair.Key;
		for (const TPair<EEGQuestArrowOutcome, TSet<FGuid>>& OutcomePair : SourcePair.Value)
		{
			TMap<int32, FGuid> PriorityOwners;
			for (const FGuid& DestinationGuid : OutcomePair.Value)
			{
				const int32 Priority = Source->FindRoutePriority(DestinationGuid, OutcomePair.Key);
				if (Priority == INDEX_NONE)
				{
					Add(EGQuestDiagnosticRule::MissingRoutePriority, EEGQuestDiagnosticSeverity::Error, Source->GetGUID(),
						FText::Format(LOCTEXT("MissingRoutePriority", "Route to {0} has no authored priority."),
							FText::FromString(DestinationGuid.ToString())),
						LOCTEXT("MissingRoutePriorityFix", "Compile the quest to seed a priority, then adjust it in the route data."));
					continue;
				}
				if (const FGuid* Existing = PriorityOwners.Find(Priority))
				{
					Add(EGQuestDiagnosticRule::DuplicateRoutePriority, EEGQuestDiagnosticSeverity::Error, Source->GetGUID(),
						FText::Format(LOCTEXT("DuplicateRoutePriority", "Route priority {0} is shared by destinations {1} and {2}."),
							Priority, FText::FromString(Existing->ToString()), FText::FromString(DestinationGuid.ToString())),
						LOCTEXT("DuplicateRoutePriorityFix", "Give routes in the same outcome group unique priorities."));
				}
				else
				{
					PriorityOwners.Add(Priority, DestinationGuid);
				}
			}
		}

		for (const FEGQuestRoutePriority& Route : Source->GetRoutePriorities())
		{
			const TSet<FGuid>* Destinations = SourcePair.Value.Find(Route.Outcome);
			if (!Destinations || !Destinations->Contains(Route.DestinationGuid))
			{
				Add(EGQuestDiagnosticRule::StaleRoutePriority, EEGQuestDiagnosticSeverity::Warning, Source->GetGUID(),
					FText::Format(LOCTEXT("StaleRoutePriority", "Priority data still names disconnected destination {0}."),
						FText::FromString(Route.DestinationGuid.ToString())),
					LOCTEXT("StaleRoutePriorityFix", "Compile the quest to reconcile route data with the wires."));
			}
		}
	}
}

void FEGQuestCompilerContext::AddDiagnostic(
	FName RuleId, EEGQuestDiagnosticSeverity Severity, const FGuid& ElementGuid, FText Message, FText FixHint)
{
	FEGQuestDiagnostic Diagnostic;
	Diagnostic.RuleId = RuleId;
	Diagnostic.Severity = Severity;
	Diagnostic.ElementGuid = ElementGuid;
	Diagnostic.Message = MoveTemp(Message);
	Diagnostic.FixHint = MoveTemp(FixHint);
	const FText LogText = FText::Format(LOCTEXT("CompilerDiagnostic", "[{0}] {1}"),
		FText::FromName(RuleId), WithFixHint(Diagnostic));

	switch (Severity)
	{
		case EEGQuestDiagnosticSeverity::Error:
			MessageLog.Error(*LogText.ToString());
			break;
		case EEGQuestDiagnosticSeverity::Warning:
			MessageLog.Warning(*LogText.ToString());
			break;
		default:
			MessageLog.Note(*LogText.ToString());
			break;
	}
	Diagnostics.Items.Add(MoveTemp(Diagnostic));
}

void FEGQuestCompilerContext::ApplyDiagnosticsToGraphNodes()
{
	for (UEGQuestGraphNode_Root* RootNode : GraphNodeRoots)
	{
		RootNode->ApplyDiagnostics(Diagnostics);
	}
	for (UEGQuestGraphNode* GraphNode : QuestGraphNodes)
	{
		GraphNode->ApplyDiagnostics(Diagnostics);
	}
}

#undef LOCTEXT_NAMESPACE
