#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "UnrealExtendedQuest/EGQuestContext.h"
#include "UnrealExtendedQuest/Events/EGQuestGraphEvents.h"
#include "UnrealExtendedQuest/EGQuestGraph.h"
#include "UnrealExtendedQuest/Nodes/EGQuestNode_End.h"
#include "UnrealExtendedQuest/Nodes/EGQuestNode_Objective.h"
#include "UnrealExtendedQuest/Nodes/EGQuestNode_Stage.h"
#include "UnrealExtendedQuest/Nodes/EGQuestNode_Start.h"
#include "UnrealExtendedQuest/EGQuestComponent.h"
#include "UnrealExtendedQuest/EGQuestFactsSubsystem.h"
#include "UnrealExtendedQuest/EGQuestPluginSettings.h"
#include "UnrealExtendedQuest/EGQuestSimulator.h"
#include "UnrealExtendedQuest/EGQuestRole.h"
#include "UnrealExtendedQuest/EGQuestTargetRegistry.h"
#include "UnrealExtendedQuest/EGQuestTracker.h"
#include "UnrealExtendedQuest/Tests/EGQuestIOTesterTypes.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Tests/AutomationEditorCommon.h"

namespace
{
	UEGQuestEvent_NamedEvent* NamedEvent(UObject& Owner, const FName Name)
	{
		UEGQuestEvent_NamedEvent* Event = NewObject<UEGQuestEvent_NamedEvent>(&Owner);
		Event->SetEventName(Name);
		return Event;
	}

	// An objective that succeeds after RequiredCount matching gameplay events, authored the way the
	// editor authors it: an EventCount tracker in the success slot. An objective with no tracker is
	// the manual archetype: only authority code completes it.
	void SetEventCount(UEGQuestNode_Objective& Objective, const TCHAR* Tag, const int32 RequiredCount)
	{
		UEGQuestTracker_EventCount* Counting = NewObject<UEGQuestTracker_EventCount>(&Objective, NAME_None, RF_Transactional);
		Counting->ExactEventTag = FGameplayTag::RequestGameplayTag(FName(Tag), false);
		Counting->RequiredCount = RequiredCount;
		Objective.SetTracker(Counting);
	}

	// A failure tracker's presence alone is what makes an objective failable.
	void SetFailEventCount(UEGQuestNode_Objective& Objective, const TCHAR* Tag, const int32 RequiredCount)
	{
		UEGQuestTracker_EventCount* Counting = NewObject<UEGQuestTracker_EventCount>(&Objective, NAME_None, RF_Transactional);
		Counting->ExactEventTag = FGameplayTag::RequestGameplayTag(FName(Tag), false);
		Counting->RequiredCount = RequiredCount;
		Counting->OutcomeOnSatisfied = EEGQuestObjectiveOutcome::Failed;
		Objective.SetFailureTracker(Counting);
	}

	FEGQuestGameplayEvent TagEvent(const TCHAR* Tag)
	{
		FEGQuestGameplayEvent Event;
		Event.EventTag = FGameplayTag::RequestGameplayTag(FName(Tag), false);
		return Event;
	}

	FEGQuestEdge Arrow(const int32 TargetIndex, const EEGQuestArrowOutcome Outcome = EEGQuestArrowOutcome::Success)
	{
		FEGQuestEdge Edge(TargetIndex);
		Edge.Outcome = Outcome;
		return Edge;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGQuestEventAutomationTest,
	"QuestPlugin.Runtime.NamedEventBroadcast",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGQuestEventAutomationTest::RunTest(const FString& Parameters)
{
	UEGQuestContext* Context = NewObject<UEGQuestContext>();
	TestNotNull(TEXT("Context created"), Context);
	if (!Context) return false;

	int32 NamedEventCount = 0;
	Context->OnNamedQuestEvent.AddLambda([&NamedEventCount](UEGQuestContext&, FName Name)
	{
		if (Name == TEXT("ObjectiveUpdated")) ++NamedEventCount;
	});
	NamedEvent(*Context, TEXT("ObjectiveUpdated"))->EnterEvent(Context);
	TestEqual(TEXT("Named quest event broadcasts once"), NamedEventCount, 1);

	NamedEvent(*Context, TEXT("SomethingElse"))->EnterEvent(Context);
	TestEqual(TEXT("Unrelated event name does not reach the listener"), NamedEventCount, 1);
	return true;
}

namespace
{
	/**
	 * Start -> Stage "Reach the altar" (one manual objective) -> Stage "Light it" (one) -> End.
	 * Nodes: [Stage0, Obj0, Stage1, Obj1, End]
	 */
	UEGQuestGraph* BuildTwoStageGraph()
	{
		UEGQuestGraph* Graph = NewObject<UEGQuestGraph>();
		Graph->RegenerateGUID();
		UEGQuestNode_Start* Start = Graph->ConstructQuestNode<UEGQuestNode_Start>();
		UEGQuestNode_Stage* Stage0 = Graph->ConstructQuestNode<UEGQuestNode_Stage>();
		UEGQuestNode_Objective* Obj0 = Graph->ConstructQuestNode<UEGQuestNode_Objective>();
		UEGQuestNode_Stage* Stage1 = Graph->ConstructQuestNode<UEGQuestNode_Stage>();
		UEGQuestNode_Objective* Obj1 = Graph->ConstructQuestNode<UEGQuestNode_Objective>();
		UEGQuestNode_End* End = Graph->ConstructQuestNode<UEGQuestNode_End>();
		for (UEGQuestNode* Node : TArray<UEGQuestNode*>{Start, Stage0, Obj0, Stage1, Obj1, End})
		{
			Node->RegenerateGUID();
		}

		Stage0->SetTitle(FText::FromString(TEXT("Reach the altar")));
		Stage0->SetDescription(FText::FromString(TEXT("The altar lies beneath the chapel.")));
		Stage0->SetNodeEnterEvents({NamedEvent(*Graph, TEXT("Stage0Enter"))});
		// No tags: manual objectives, completed by the authority.
		Obj0->SetNodeText(FText::FromString(TEXT("Find the altar")));

		Stage1->SetTitle(FText::FromString(TEXT("Light it")));
		Stage1->SetNodeEnterEvents({NamedEvent(*Graph, TEXT("Stage1Enter"))});
		Obj1->SetNodeText(FText::FromString(TEXT("Light the candles")));

		End->SetNodeEnterEvents({NamedEvent(*Graph, TEXT("EndEnter"))});

		Graph->SetNodes({Stage0, Obj0, Stage1, Obj1, End});
		Graph->SetStartNodes({Start});
		Start->SetNodeChildren({Arrow(0)});
		Stage0->SetNodeChildren({Arrow(1)});   // ownership
		Obj0->SetNodeChildren({Arrow(2)});     // success -> Stage1
		Stage1->SetNodeChildren({Arrow(3)});   // ownership
		Obj1->SetNodeChildren({Arrow(4)});     // success -> End
		return Graph;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGQuestStageJournalAutomationTest,
	"QuestPlugin.Runtime.StageJournalAndTraversal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGQuestStageJournalAutomationTest::RunTest(const FString& Parameters)
{
	UEGQuestGraph* Graph = BuildTwoStageGraph();
	UEGQuestNode_Stage* Stage0 = Cast<UEGQuestNode_Stage>(Graph->GetNodes()[0]);
	UEGQuestNode_Objective* Obj0 = Cast<UEGQuestNode_Objective>(Graph->GetNodes()[1]);
	UEGQuestNode_Stage* Stage1 = Cast<UEGQuestNode_Stage>(Graph->GetNodes()[2]);
	UEGQuestNode_Objective* Obj1 = Cast<UEGQuestNode_Objective>(Graph->GetNodes()[3]);

	AGameStateBase* GameState = NewObject<AGameStateBase>();
	UEGQuestComponent* Component = NewObject<UEGQuestComponent>(GameState);
	const FGuid Instance = Component->StartSharedQuest(Graph);
	TestTrue(TEXT("Quest starts"), Instance.IsValid());

	FEGQuestRuntimeSnapshot Snapshot;
	TestTrue(TEXT("Snapshot queryable"), Component->FindQuestSnapshot(Instance, Snapshot));
	TestEqual(TEXT("The active node is the first stage, not an objective"), Snapshot.ActiveNodeGuid, Stage0->GetGUID());
	TestEqual(TEXT("Journal carries the stage title"), Snapshot.ActiveStageTitle.ToString(), FString(TEXT("Reach the altar")));
	TestEqual(TEXT("Journal carries the stage description"), Snapshot.ActiveStageDescription.ToString(),
		FString(TEXT("The altar lies beneath the chapel.")));
	TestEqual(TEXT("Checklist has the stage's one objective"), Snapshot.ActiveObjectives.Num(), 1);
	if (Snapshot.ActiveObjectives.Num() != 1) return false;
	TestEqual(TEXT("Checklist line is the objective"), Snapshot.ActiveObjectives[0].Guid, Obj0->GetGUID());
	TestEqual(TEXT("Checklist line carries the objective text"), Snapshot.ActiveObjectives[0].Text.ToString(), FString(TEXT("Find the altar")));
	TestEqual(TEXT("Checklist line starts pending"), Snapshot.ActiveObjectives[0].Outcome, EEGQuestObjectiveOutcome::Pending);

	// An objective of the active stage completes; its success arrow is the only one into Stage1, so
	// Stage1 fires immediately.
	TestFalse(TEXT("An objective that is not in the active stage is rejected"),
		Component->CompleteActiveObjective(Instance, Obj1->GetGUID()));
	TestTrue(TEXT("Completing the active stage's objective is accepted"),
		Component->CompleteActiveObjective(Instance, Obj0->GetGUID()));

	TestTrue(TEXT("Snapshot queryable after firing"), Component->FindQuestSnapshot(Instance, Snapshot));
	TestEqual(TEXT("The destination stage fired and is now active"), Snapshot.ActiveNodeGuid, Stage1->GetGUID());
	TestEqual(TEXT("Journal shows the new stage"), Snapshot.ActiveStageTitle.ToString(), FString(TEXT("Light it")));
	TestEqual(TEXT("Checklist was rebuilt for the new stage"), Snapshot.ActiveObjectives.Num(), 1);
	TestEqual(TEXT("Checklist line is the new stage's objective"), Snapshot.ActiveObjectives[0].Guid, Obj1->GetGUID());
	TestFalse(TEXT("Quest is not terminal mid-run"), Snapshot.IsTerminal());

	TestTrue(TEXT("Completing the last objective is accepted"), Component->CompleteActiveObjective(Instance, Obj1->GetGUID()));
	TestTrue(TEXT("Snapshot queryable after the end fires"), Component->FindQuestSnapshot(Instance, Snapshot));
	TestTrue(TEXT("Reaching the end node makes the quest terminal"), Snapshot.IsTerminal());
	TestEqual(TEXT("The end node's result is stamped on the quest"), Snapshot.LifecycleState, EEGQuestLifecycleState::Completed);

	// A finished quest must not keep advertising the last stage's journal entry as if it were still
	// being worked on.
	TestEqual(TEXT("A finished quest has an empty checklist"), Snapshot.ActiveObjectives.Num(), 0);
	TestTrue(TEXT("A finished quest has no stage title"), Snapshot.ActiveStageTitle.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGQuestStageEnterEventsAutomationTest,
	"QuestPlugin.Runtime.StageEnterEventsAndHistory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGQuestStageEnterEventsAutomationTest::RunTest(const FString& Parameters)
{
	UEGQuestGraph* Graph = BuildTwoStageGraph();
	UEGQuestNode_Stage* Stage0 = Cast<UEGQuestNode_Stage>(Graph->GetNodes()[0]);
	UEGQuestNode_Objective* Obj0 = Cast<UEGQuestNode_Objective>(Graph->GetNodes()[1]);
	UEGQuestNode_Objective* Obj1 = Cast<UEGQuestNode_Objective>(Graph->GetNodes()[3]);

	UEGQuestContext* Context = NewObject<UEGQuestContext>();
	TArray<FName> EventOrder;
	Context->OnNamedQuestEvent.AddLambda([&EventOrder](UEGQuestContext&, const FName Name) { EventOrder.Add(Name); });
	TestTrue(TEXT("Context starts on the first stage"), Context->Start(Graph));
	TestEqual(TEXT("Start lands on the stage"), Context->GetActiveNodeGUID(), Stage0->GetGUID());
	TestTrue(TEXT("The stage is recorded in local history"), Context->GetVisitedNodeGUIDs().Contains(Stage0->GetGUID()));

	// Entering does not fire enter events: the caller does, once it has published the stage. That
	// ordering is what lets a handler read the quest back and see the stage it was told about.
	TestEqual(TEXT("Entering a stage does not fire its enter events by itself"), EventOrder.Num(), 0);
	Context->FireActiveNodeEnterEvents();
	TestTrue(TEXT("The caller fires the active stage's enter events"), EventOrder == TArray<FName>({TEXT("Stage0Enter")}));

	// Objectives are never entered, so they never fire enter events - only stages and the end do.
	TestTrue(TEXT("Objectives are not visited: only the stage is"), !Context->GetVisitedNodeGUIDs().Contains(Obj0->GetGUID()));

	UEGQuestContext* Resumed = NewObject<UEGQuestContext>();
	TestTrue(TEXT("Context resumes from a stable stage GUID"),
		Resumed->ResumeFromNodeGUID(Graph, Stage0->GetGUID(), Context->GetHistoryOfThisContext(), false));
	TestEqual(TEXT("Resume restores the requested stage"), Resumed->GetActiveNodeGUID(), Stage0->GetGUID());
	return true;
}

namespace
{
	/**
	 * A stage whose two objectives both feed one destination, so the destination is a join.
	 * Nodes: [Stage, ObjA, ObjB, End]
	 */
	UEGQuestGraph* BuildJoinGraph()
	{
		UEGQuestGraph* Graph = NewObject<UEGQuestGraph>();
		Graph->RegenerateGUID();
		UEGQuestNode_Start* Start = Graph->ConstructQuestNode<UEGQuestNode_Start>();
		UEGQuestNode_Stage* Stage = Graph->ConstructQuestNode<UEGQuestNode_Stage>();
		UEGQuestNode_Objective* ObjA = Graph->ConstructQuestNode<UEGQuestNode_Objective>();
		UEGQuestNode_Objective* ObjB = Graph->ConstructQuestNode<UEGQuestNode_Objective>();
		UEGQuestNode_End* End = Graph->ConstructQuestNode<UEGQuestNode_End>();
		for (UEGQuestNode* Node : TArray<UEGQuestNode*>{Start, Stage, ObjA, ObjB, End})
		{
			Node->RegenerateGUID();
		}

		Stage->SetTitle(FText::FromString(TEXT("Prepare the rite")));
		ObjA->SetNodeText(FText::FromString(TEXT("Light the candles")));
		SetEventCount(*ObjA, TEXT("DOP.Quest.Castle.GateOpened"), 1);
		ObjB->SetNodeText(FText::FromString(TEXT("Open the gate")));
		SetEventCount(*ObjB, TEXT("DOP.Quest.Castle.BookAcquired"), 1);

		Graph->SetNodes({Stage, ObjA, ObjB, End});
		Graph->SetStartNodes({Start});
		Start->SetNodeChildren({Arrow(0)});
		Stage->SetNodeChildren({Arrow(1), Arrow(2)});  // ownership of both objectives
		ObjA->SetNodeChildren({Arrow(3)});             // success -> End
		ObjB->SetNodeChildren({Arrow(3)});             // success -> End
		return Graph;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGQuestDestinationJoinAutomationTest,
	"QuestPlugin.Runtime.DestinationJoinWaitsForEveryArrow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGQuestDestinationJoinAutomationTest::RunTest(const FString& Parameters)
{
	UEGQuestGraph* Graph = BuildJoinGraph();
	AGameStateBase* GameState = NewObject<AGameStateBase>();
	UEGQuestComponent* Component = NewObject<UEGQuestComponent>(GameState);
	const FGuid Instance = Component->StartSharedQuest(Graph);
	TestTrue(TEXT("Quest starts"), Instance.IsValid());

	FEGQuestRuntimeSnapshot Snapshot;
	TestTrue(TEXT("Snapshot queryable"), Component->FindQuestSnapshot(Instance, Snapshot));
	TestEqual(TEXT("Both objectives are on the checklist at once"), Snapshot.ActiveObjectives.Num(), 2);

	// The end is fed by two arrows, so one satisfied arrow is not enough: convergence is the AND.
	Component->NotifyGameplayEvent(TagEvent(TEXT("DOP.Quest.Castle.GateOpened")));
	TestTrue(TEXT("Snapshot queryable after the first event"), Component->FindQuestSnapshot(Instance, Snapshot));
	TestFalse(TEXT("One satisfied arrow does not fire a destination joined by two"), Snapshot.IsTerminal());
	TestEqual(TEXT("The resolved objective is marked succeeded"), Snapshot.ActiveObjectives[0].Outcome, EEGQuestObjectiveOutcome::Succeeded);
	TestEqual(TEXT("The other objective is still pending"), Snapshot.ActiveObjectives[1].Outcome, EEGQuestObjectiveOutcome::Pending);

	Component->NotifyGameplayEvent(TagEvent(TEXT("DOP.Quest.Castle.BookAcquired")));
	TestTrue(TEXT("Snapshot queryable after the second event"), Component->FindQuestSnapshot(Instance, Snapshot));
	TestTrue(TEXT("The destination fires once every arrow into it is satisfied"), Snapshot.IsTerminal());
	return true;
}

namespace
{
	/**
	 * One stage, one objective that can fail, routing success and failure to different ends.
	 * Nodes: [Stage, Obj, GoodEnd, BadEnd]
	 */
	UEGQuestGraph* BuildFailRoutingGraph()
	{
		UEGQuestGraph* Graph = NewObject<UEGQuestGraph>();
		Graph->RegenerateGUID();
		UEGQuestNode_Start* Start = Graph->ConstructQuestNode<UEGQuestNode_Start>();
		UEGQuestNode_Stage* Stage = Graph->ConstructQuestNode<UEGQuestNode_Stage>();
		UEGQuestNode_Objective* Obj = Graph->ConstructQuestNode<UEGQuestNode_Objective>();
		UEGQuestNode_End* GoodEnd = Graph->ConstructQuestNode<UEGQuestNode_End>();
		UEGQuestNode_End* BadEnd = Graph->ConstructQuestNode<UEGQuestNode_End>();
		for (UEGQuestNode* Node : TArray<UEGQuestNode*>{Start, Stage, Obj, GoodEnd, BadEnd})
		{
			Node->RegenerateGUID();
		}
		GoodEnd->SetQuestResult(EEGQuestResult::Completed);
		BadEnd->SetQuestResult(EEGQuestResult::Failed);

		Stage->SetTitle(FText::FromString(TEXT("Keep the patient alive")));
		Obj->SetNodeText(FText::FromString(TEXT("Deliver the patient")));
		SetEventCount(*Obj, TEXT("DOP.Quest.Dungeon.PatientPlaced"), 1);
		SetFailEventCount(*Obj, TEXT("DOP.Quest.Dungeon.PatientInjected"), 1);

		Graph->SetNodes({Stage, Obj, GoodEnd, BadEnd});
		Graph->SetStartNodes({Start});
		Start->SetNodeChildren({Arrow(0)});
		Stage->SetNodeChildren({Arrow(1)});
		Obj->SetNodeChildren({Arrow(2, EEGQuestArrowOutcome::Success), Arrow(3, EEGQuestArrowOutcome::Fail)});
		return Graph;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGQuestFailRoutingAutomationTest,
	"QuestPlugin.Runtime.FailConditionRoutesDownTheFailArrow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGQuestFailRoutingAutomationTest::RunTest(const FString& Parameters)
{
	UEGQuestGraph* Graph = BuildFailRoutingGraph();
	AGameStateBase* GameState = NewObject<AGameStateBase>();
	UEGQuestComponent* Component = NewObject<UEGQuestComponent>(GameState);

	// Failing the objective takes the Fail arrow, not the Success one.
	const FGuid Failing = Component->StartSharedQuest(Graph);
	Component->NotifyGameplayEvent(TagEvent(TEXT("DOP.Quest.Dungeon.PatientInjected")));
	FEGQuestRuntimeSnapshot Snapshot;
	TestTrue(TEXT("Snapshot queryable"), Component->FindQuestSnapshot(Failing, Snapshot));
	TestTrue(TEXT("The fail arrow fires its destination"), Snapshot.IsTerminal());
	TestEqual(TEXT("Failure routes to the failed end"), Snapshot.LifecycleState, EEGQuestLifecycleState::Failed);

	// The same graph, succeeded instead: same objective, other arrow.
	const FGuid Succeeding = Component->StartSharedQuest(Graph);
	Component->NotifyGameplayEvent(TagEvent(TEXT("DOP.Quest.Dungeon.PatientPlaced")));
	TestTrue(TEXT("Second snapshot queryable"), Component->FindQuestSnapshot(Succeeding, Snapshot));
	TestTrue(TEXT("The success arrow fires its destination"), Snapshot.IsTerminal());
	TestEqual(TEXT("Success routes to the completed end"), Snapshot.LifecycleState, EEGQuestLifecycleState::Completed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGQuestCancellationAutomationTest,
	"QuestPlugin.Runtime.LeavingAStageCancelsItsPendingObjectives",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGQuestCancellationAutomationTest::RunTest(const FString& Parameters)
{
	// A stage with two objectives racing down separate paths: each is the whole incoming arrow set
	// of its own destination, so whichever resolves first fires and the other is cancelled.
	// Nodes: [Stage, ObjA, ObjB, Later, LaterObj, OtherEnd]
	UEGQuestGraph* Graph = NewObject<UEGQuestGraph>();
	Graph->RegenerateGUID();
	UEGQuestNode_Start* Start = Graph->ConstructQuestNode<UEGQuestNode_Start>();
	UEGQuestNode_Stage* Stage = Graph->ConstructQuestNode<UEGQuestNode_Stage>();
	UEGQuestNode_Objective* ObjA = Graph->ConstructQuestNode<UEGQuestNode_Objective>();
	UEGQuestNode_Objective* ObjB = Graph->ConstructQuestNode<UEGQuestNode_Objective>();
	UEGQuestNode_Stage* Later = Graph->ConstructQuestNode<UEGQuestNode_Stage>();
	UEGQuestNode_Objective* LaterObj = Graph->ConstructQuestNode<UEGQuestNode_Objective>();
	UEGQuestNode_End* OtherEnd = Graph->ConstructQuestNode<UEGQuestNode_End>();
	for (UEGQuestNode* Node : TArray<UEGQuestNode*>{Start, Stage, ObjA, ObjB, Later, LaterObj, OtherEnd})
	{
		Node->RegenerateGUID();
	}
	Stage->SetTitle(FText::FromString(TEXT("Race")));
	ObjA->SetNodeText(FText::FromString(TEXT("Left path")));
	SetEventCount(*ObjA, TEXT("DOP.Quest.Castle.GateOpened"), 1);
	ObjB->SetNodeText(FText::FromString(TEXT("Right path")));
	SetEventCount(*ObjB, TEXT("DOP.Quest.Castle.BookAcquired"), 1);
	Later->SetTitle(FText::FromString(TEXT("Afterwards")));
	// Manual: only authority code completes it.
	LaterObj->SetNodeText(FText::FromString(TEXT("Leave")));

	Graph->SetNodes({Stage, ObjA, ObjB, Later, LaterObj, OtherEnd});
	Graph->SetStartNodes({Start});
	Start->SetNodeChildren({Arrow(0)});
	Stage->SetNodeChildren({Arrow(1), Arrow(2)});
	ObjA->SetNodeChildren({Arrow(3)});   // the only arrow into "Afterwards"
	ObjB->SetNodeChildren({Arrow(5)});   // the only arrow into the other end
	Later->SetNodeChildren({Arrow(4)});

	AGameStateBase* GameState = NewObject<AGameStateBase>();
	UEGQuestComponent* Component = NewObject<UEGQuestComponent>(GameState);
	const FGuid Instance = Component->StartSharedQuest(Graph);

	FEGQuestRuntimeSnapshot Snapshot;
	Component->FindQuestSnapshot(Instance, Snapshot);
	TestEqual(TEXT("Both objectives start on the checklist"), Snapshot.ActiveObjectives.Num(), 2);

	// ObjA is the whole incoming arrow set for "Afterwards", so resolving it fires that stage.
	Component->NotifyGameplayEvent(TagEvent(TEXT("DOP.Quest.Castle.GateOpened")));
	TestTrue(TEXT("Snapshot queryable"), Component->FindQuestSnapshot(Instance, Snapshot));
	TestEqual(TEXT("The stage was left for the destination"), Snapshot.ActiveNodeGuid, Later->GetGUID());
	TestEqual(TEXT("The old stage's objectives are gone: the checklist is the new stage's"),
		Snapshot.ActiveObjectives.Num(), 1);
	TestEqual(TEXT("The surviving line belongs to the new stage"), Snapshot.ActiveObjectives[0].Guid, LaterObj->GetGUID());

	// The cancelled objective's event must not resurrect it or move anything.
	const int32 RevisionAfterFiring = Snapshot.Revision;
	Component->NotifyGameplayEvent(TagEvent(TEXT("DOP.Quest.Castle.BookAcquired")));
	TestTrue(TEXT("Snapshot queryable after the cancelled objective's event"), Component->FindQuestSnapshot(Instance, Snapshot));
	TestEqual(TEXT("An event for a cancelled objective changes nothing"), Snapshot.Revision, RevisionAfterFiring);
	TestEqual(TEXT("The new stage is still active"), Snapshot.ActiveNodeGuid, Later->GetGUID());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGQuestLevelQuestAssetsAutomationTest,
	"QuestPlugin.Assets.LevelQuestGraphsLoadAndValidate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGQuestLevelQuestAssetsAutomationTest::RunTest(const FString& Parameters)
{
	struct FExpectedQuest
	{
		const TCHAR* ObjectPath;
		int32 StepCount;
		TArray<const TCHAR*> EventTags;
	};

	const TArray<FExpectedQuest> ExpectedQuests = {
		{TEXT("/Game/Quests/Q_Castle.Q_Castle"), 4,
			{TEXT("DOP.Quest.Castle.GateOpened"), TEXT("DOP.Quest.Castle.BookAcquired"),
			 TEXT("DOP.Quest.Castle.BookPlaced"), TEXT("DOP.Quest.Ritual.Completed")}},
		{TEXT("/Game/Quests/Q_Dungeon.Q_Dungeon"), 5,
			{TEXT("DOP.Quest.Dungeon.KeyTaken"), TEXT("DOP.Quest.Dungeon.PatientPlaced"),
			 TEXT("DOP.Quest.Dungeon.PatientInjected"), TEXT("DOP.Quest.Dungeon.FountainItemsPlaced"),
			 TEXT("DOP.Quest.Dungeon.SyringeFilled")}},
		{TEXT("/Game/Quests/Q_Egypt.Q_Egypt"), 6,
			{TEXT("DOP.Quest.Egypt.MaatFeatherPlaced"), TEXT("DOP.Quest.Egypt.CanopicsPlaced"),
			 TEXT("DOP.Quest.Egypt.HeartAcquired"), TEXT("DOP.Quest.Egypt.HeartPurified"),
			 TEXT("DOP.Quest.Egypt.HeartPlaced"), TEXT("DOP.Quest.Egypt.HeartMummified")}}
	};

	for (const FExpectedQuest& Expected : ExpectedQuests)
	{
		UEGQuestGraph* Graph = LoadObject<UEGQuestGraph>(nullptr, Expected.ObjectPath);
		TestNotNull(FString::Printf(TEXT("%s loads"), Expected.ObjectPath), Graph);
		if (!Graph) continue;
		TestTrue(TEXT("Graph GUID is stable and valid"), Graph->GetGUID().IsValid());
		TestTrue(TEXT("Graph has a QuestGraph primary asset id"), Graph->GetPrimaryAssetId().IsValid());
		TestEqual(TEXT("Graph has exactly one start node"), Graph->GetStartNodes().Num(), 1);

		// Each step is a stage plus its objective, and the whole chain ends in one end node.
		TArray<const UEGQuestNode_Stage*> Stages;
		TArray<const UEGQuestNode_Objective*> Objectives;
		TArray<const UEGQuestNode_End*> Ends;
		for (UEGQuestNode* Node : Graph->GetNodes())
		{
			TestNotNull(TEXT("Compiled runtime node exists"), Node);
			if (!Node) continue;
			TestTrue(TEXT("Compiled runtime node GUID is valid"), Node->GetGUID().IsValid());
			if (const UEGQuestNode_Stage* Stage = Cast<UEGQuestNode_Stage>(Node)) Stages.Add(Stage);
			else if (const UEGQuestNode_Objective* Objective = Cast<UEGQuestNode_Objective>(Node)) Objectives.Add(Objective);
			else if (const UEGQuestNode_End* End = Cast<UEGQuestNode_End>(Node)) Ends.Add(End);
		}
		TestEqual(TEXT("Graph has one stage per step"), Stages.Num(), Expected.StepCount);
		TestEqual(TEXT("Graph has one objective per step"), Objectives.Num(), Expected.StepCount);
		TestEqual(TEXT("Graph has exactly one end node"), Ends.Num(), 1);
		if (Ends.Num() == 1) TestEqual(TEXT("Level quest completes"), Ends[0]->GetQuestResult(), EEGQuestResult::Completed);

		for (const UEGQuestNode_Stage* Stage : Stages)
		{
			TestFalse(TEXT("Every stage carries a journal title"), Stage->GetTitle().IsEmpty());
		}
		for (int32 Index = 0; Index < Objectives.Num() && Index < Expected.EventTags.Num(); ++Index)
		{
			// Legacy assets migrate their fields into trackers in PostLoad and new assets author
			// trackers directly, so the presented accessor reads the tag either way.
			TestTrue(TEXT("Objective counts a gameplay event"), Objectives[Index]->GetPresentedEventTag().IsValid());
			TestEqual(TEXT("Objective completion tag is in authored order"),
				Objectives[Index]->GetPresentedEventTag().ToString(), FString(Expected.EventTags[Index]));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGQuestComponentAuthorityAutomationTest,
	"QuestPlugin.Runtime.AuthorityScopeEventsPersistenceAndStaleRequests",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGQuestComponentAuthorityAutomationTest::RunTest(const FString& Parameters)
{
	UEGQuestGraph* Castle = LoadObject<UEGQuestGraph>(nullptr, TEXT("/Game/Quests/Q_Castle.Q_Castle"));
	TestNotNull(TEXT("Castle graph loads for component test"), Castle);
	if (!Castle) return false;

	// The Castle ritual objective is authored to need several completions. Pin every objective to a
	// single completion so one event per tag finishes the quest whichever asset version is on disk.
	// Post-load every counting objective holds an EventCount tracker (authored or migrated).
	for (UEGQuestNode* Node : Castle->GetNodes())
	{
		if (UEGQuestNode_Objective* Objective = Cast<UEGQuestNode_Objective>(Node))
		{
			if (UEGQuestTracker_EventCount* Counting = Cast<UEGQuestTracker_EventCount>(Objective->GetTracker()))
			{
				Counting->RequiredCount = 1;
			}
		}
	}

	AGameStateBase* GameState = NewObject<AGameStateBase>();
	UEGQuestComponent* SharedComponent = NewObject<UEGQuestComponent>(GameState);
	TestNotNull(TEXT("Shared component created"), SharedComponent);
	if (!SharedComponent) return false;
	const FGuid SharedInstance = SharedComponent->StartSharedQuest(Castle);
	TestTrue(TEXT("GameState can start a shared quest on authority"), SharedInstance.IsValid());
	TestFalse(TEXT("GameState cannot start a private quest"), SharedComponent->StartPrivateQuest(Castle).IsValid());
	TestEqual(TEXT("Only shared snapshot collection is populated"), SharedComponent->GetSharedQuestSnapshots().Num(), 1);
	TestEqual(TEXT("Private snapshot collection remains empty"), SharedComponent->GetPrivateQuestSnapshots().Num(), 0);

	FEGQuestRuntimeSnapshot Snapshot;
	TestTrue(TEXT("Started quest snapshot is queryable"), SharedComponent->FindQuestSnapshot(SharedInstance, Snapshot));
	const FGuid FirstStage = Snapshot.ActiveNodeGuid;
	TestFalse(TEXT("Completion of an unknown objective is rejected"),
		SharedComponent->CompleteActiveObjective(SharedInstance, FGuid::NewGuid()));

	const TArray<const TCHAR*> CastleEvents = {
		TEXT("DOP.Quest.Castle.GateOpened"), TEXT("DOP.Quest.Castle.BookAcquired"),
		TEXT("DOP.Quest.Castle.BookPlaced"), TEXT("DOP.Quest.Ritual.Completed")
	};
	int32 PreviousRevision = Snapshot.Revision;
	for (const TCHAR* TagName : CastleEvents)
	{
		TestTrue(TEXT("Authoritative gameplay event is accepted"), SharedComponent->NotifyGameplayEvent(TagEvent(TagName)));
		TestTrue(TEXT("Snapshot remains queryable after event"), SharedComponent->FindQuestSnapshot(SharedInstance, Snapshot));
		TestTrue(TEXT("Snapshot revision increases monotonically"), Snapshot.Revision > PreviousRevision);
		PreviousRevision = Snapshot.Revision;
	}
	TestTrue(TEXT("Castle quest reaches a terminal snapshot"), Snapshot.IsTerminal());
	TestEqual(TEXT("Castle quest completes after four accepted events"), Snapshot.LifecycleState, EEGQuestLifecycleState::Completed);
	TestFalse(TEXT("Completed quest is not resumable"), [&]() { FEGQuestSaveEnvelope Data; return SharedComponent->BuildQuestSaveData(SharedInstance, Data) && SharedComponent->ResumeQuest(Data).IsValid(); }());

	APlayerState* PlayerState = NewObject<APlayerState>();
	UEGQuestComponent* PrivateComponent = NewObject<UEGQuestComponent>(PlayerState);
	const FGuid PrivateInstance = PrivateComponent->StartPrivateQuest(Castle);
	TestTrue(TEXT("PlayerState can start a private quest on authority"), PrivateInstance.IsValid());
	TestFalse(TEXT("PlayerState cannot start a shared quest"), PrivateComponent->StartSharedQuest(Castle).IsValid());
	TestEqual(TEXT("Private quest only populates owner-only collection"), PrivateComponent->GetPrivateQuestSnapshots().Num(), 1);
	TestEqual(TEXT("PlayerState shared collection remains empty"), PrivateComponent->GetSharedQuestSnapshots().Num(), 0);

	FEGQuestSaveEnvelope SaveData;
	TestTrue(TEXT("Active private quest builds explicit save data"), PrivateComponent->BuildQuestSaveData(PrivateInstance, SaveData));
	TestTrue(TEXT("Save payload carries the stage's checklist"), SaveData.RunRecord.ActiveObjectives.Num() > 0);
	if (SaveData.RunRecord.ActiveObjectives.Num() == 0) return false;
	// Partial progress on the first objective must survive the round trip.
	SaveData.RunRecord.ActiveObjectives[0].Count = 1;
	SaveData.RunRecord.ActiveObjectives[0].RequiredCount = 5;
	APlayerState* RestoredPlayerState = NewObject<APlayerState>();
	UEGQuestComponent* RestoredComponent = NewObject<UEGQuestComponent>(RestoredPlayerState);
	const FGuid ResumedInstance = RestoredComponent->ResumeQuest(SaveData);
	TestEqual(TEXT("Resume preserves the persistent run id"), ResumedInstance, PrivateInstance);
	FEGQuestRuntimeSnapshot ResumedSnapshot;
	TestTrue(TEXT("Resumed snapshot is queryable"), RestoredComponent->FindQuestSnapshot(ResumedInstance, ResumedSnapshot));
	TestEqual(TEXT("Resume preserves the active stage"), ResumedSnapshot.ActiveNodeGuid, FirstStage);
	TestEqual(TEXT("Resume preserves the checklist"), ResumedSnapshot.ActiveObjectives.Num(), SaveData.RunRecord.ActiveObjectives.Num());
	if (ResumedSnapshot.ActiveObjectives.Num() > 0)
	{
		TestEqual(TEXT("Resume preserves partial objective progress"), ResumedSnapshot.ActiveObjectives[0].Count, 1);
		TestFalse(TEXT("Resume restores the objective's text"), ResumedSnapshot.ActiveObjectives[0].Text.IsEmpty());
	}
	return true;
}

namespace
{
	// Start -> Stage(description "Place {Books} books", custom per-context arg) -> Objective -> End
	UEGQuestGraph* BuildFormattedStageGraph(UEGQuestNode_Stage*& OutStage)
	{
		UEGQuestGraph* Graph = NewObject<UEGQuestGraph>();
		Graph->RegenerateGUID();
		UEGQuestNode_Start* Start = Graph->ConstructQuestNode<UEGQuestNode_Start>();
		OutStage = Graph->ConstructQuestNode<UEGQuestNode_Stage>();
		UEGQuestNode_Objective* Objective = Graph->ConstructQuestNode<UEGQuestNode_Objective>();
		UEGQuestNode_End* End = Graph->ConstructQuestNode<UEGQuestNode_End>();
		for (UEGQuestNode* Node : TArray<UEGQuestNode*>{Start, OutStage, Objective, End})
		{
			Node->RegenerateGUID();
		}
		OutStage->SetTitle(FText::FromString(TEXT("The library")));
		OutStage->SetDescription(FText::FromString(TEXT("Place {Books} books")));
		if (OutStage->GetMutableTextArguments().Num() == 1)
		{
			OutStage->GetMutableTextArguments()[0].CustomTextArgument = NewObject<UEGQuestTestContextTextArgument>(OutStage);
		}
		// Manual: only authority code completes it.
		Objective->SetNodeText(FText::FromString(TEXT("Shelve them")));

		Graph->SetNodes({OutStage, Objective, End});
		Graph->SetStartNodes({Start});
		Start->SetNodeChildren({Arrow(0)});
		OutStage->SetNodeChildren({Arrow(1)});
		Objective->SetNodeChildren({Arrow(2)});
		return Graph;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGQuestMultiContextTextIsolationAutomationTest,
	"QuestPlugin.Runtime.MultiContextTextIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGQuestMultiContextTextIsolationAutomationTest::RunTest(const FString& Parameters)
{
	UEGQuestNode_Stage* Stage = nullptr;
	UEGQuestGraph* Graph = BuildFormattedStageGraph(Stage);
	TestEqual(TEXT("Stage format argument discovered"), Stage->GetTextArguments().Num(), 1);

	UEGQuestContext* ContextA = NewObject<UEGQuestContext>();
	UEGQuestContext* ContextB = NewObject<UEGQuestContext>();
	UEGQuestTestContextTextArgument::ContextValues.Reset();
	UEGQuestTestContextTextArgument::ContextValues.Add(ContextA, FText::AsNumber(2));
	UEGQuestTestContextTextArgument::ContextValues.Add(ContextB, FText::AsNumber(7));

	auto FormattedDescription = [&Stage](const UEGQuestContext* Context)
	{
		const FText* Constructed = Context->GetConstructedNodeText(Stage->GetGUID());
		return Constructed ? Constructed->ToString() : FString();
	};

	TestTrue(TEXT("First context starts"), ContextA->Start(Graph));
	TestEqual(TEXT("First context formats its own value"), FormattedDescription(ContextA), FString(TEXT("Place 2 books")));

	TestTrue(TEXT("Second context starts on the SAME graph asset"), ContextB->Start(Graph));
	TestEqual(TEXT("Second context formats its own value"), FormattedDescription(ContextB), FString(TEXT("Place 7 books")));

	// The regression this guards: formatted text used to be stored on the shared node object,
	// so the second context's value bled into the first context's display.
	TestEqual(TEXT("First context text is NOT contaminated by the second context"),
		FormattedDescription(ContextA), FString(TEXT("Place 2 books")));
	TestEqual(TEXT("Graph asset node keeps only the raw authored text"),
		Stage->GetDescription().ToString(), FString(TEXT("Place {Books} books")));
	return true;
}

namespace
{
	// Start -> Stage -> Objective(GameplayEvent: RitualCompleted, authored count 3) -> End
	UEGQuestGraph* BuildCountingObjectiveGraph(UEGQuestNode_Objective*& OutObjective)
	{
		UEGQuestGraph* Graph = NewObject<UEGQuestGraph>();
		Graph->RegenerateGUID();
		UEGQuestNode_Start* Start = Graph->ConstructQuestNode<UEGQuestNode_Start>();
		UEGQuestNode_Stage* Stage = Graph->ConstructQuestNode<UEGQuestNode_Stage>();
		OutObjective = Graph->ConstructQuestNode<UEGQuestNode_Objective>();
		UEGQuestNode_End* End = Graph->ConstructQuestNode<UEGQuestNode_End>();
		for (UEGQuestNode* Node : TArray<UEGQuestNode*>{Start, Stage, OutObjective, End})
		{
			Node->RegenerateGUID();
		}
		Stage->SetTitle(FText::FromString(TEXT("The rites")));
		OutObjective->SetNodeText(FText::FromString(TEXT("Complete the rituals")));
		SetEventCount(*OutObjective, TEXT("DOP.Quest.Ritual.Completed"), 3);

		Graph->SetNodes({Stage, OutObjective, End});
		Graph->SetStartNodes({Start});
		Start->SetNodeChildren({Arrow(0)});
		Stage->SetNodeChildren({Arrow(1)});
		OutObjective->SetNodeChildren({Arrow(2)});
		return Graph;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGQuestRequiredCountAutomationTest,
	"QuestPlugin.Runtime.ObjectiveCompletesAtAuthoredCount",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGQuestRequiredCountAutomationTest::RunTest(const FString& Parameters)
{
	UEGQuestNode_Objective* Objective = nullptr;
	UEGQuestGraph* Graph = BuildCountingObjectiveGraph(Objective);

	AGameStateBase* GameState = NewObject<AGameStateBase>();
	UEGQuestComponent* Component = NewObject<UEGQuestComponent>(GameState);
	const FGuid Instance = Component->StartSharedQuest(Graph);
	TestTrue(TEXT("Instance started"), Instance.IsValid());

	const FEGQuestGameplayEvent Event = TagEvent(TEXT("DOP.Quest.Ritual.Completed"));
	TestTrue(TEXT("Completion tag is registered"), Event.EventTag.IsValid());

	FEGQuestRuntimeSnapshot Snapshot;
	Component->NotifyGameplayEvent(Event);
	Component->NotifyGameplayEvent(Event);
	TestTrue(TEXT("Snapshot queryable"), Component->FindQuestSnapshot(Instance, Snapshot));
	TestFalse(TEXT("Objective still counting below the authored count"), Snapshot.IsTerminal());
	TestEqual(TEXT("Checklist still has the counting objective"), Snapshot.ActiveObjectives.Num(), 1);
	TestEqual(TEXT("Progress reflects both events"), Snapshot.ActiveObjectives[0].Count, 2);
	TestEqual(TEXT("The checklist line reports the authored target"), Snapshot.ActiveObjectives[0].RequiredCount, 3);

	Component->NotifyGameplayEvent(Event);
	TestTrue(TEXT("Snapshot queryable after third event"), Component->FindQuestSnapshot(Instance, Snapshot));
	TestTrue(TEXT("Objective completes at the authored count"), Snapshot.IsTerminal());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGQuestRequiredCountOverrideAutomationTest,
	"QuestPlugin.Runtime.ObjectiveRequiredCountOverride",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGQuestRequiredCountOverrideAutomationTest::RunTest(const FString& Parameters)
{
	UEGQuestNode_Objective* Objective = nullptr;
	UEGQuestGraph* Graph = BuildCountingObjectiveGraph(Objective);

	AGameStateBase* GameState = NewObject<AGameStateBase>();
	UEGQuestComponent* Component = NewObject<UEGQuestComponent>(GameState);
	// Two instances of the same graph: one keeps the authored count (3), the other overrides to 2.
	// Every accepted event advances both active instances.
	const FGuid AuthoredInstance = Component->StartSharedQuest(Graph);
	const FGuid OverriddenInstance = Component->StartSharedQuest(Graph);
	TestTrue(TEXT("Authored instance started"), AuthoredInstance.IsValid());
	TestTrue(TEXT("Overridden instance started"), OverriddenInstance.IsValid());
	TestTrue(TEXT("Override applied"), Component->SetObjectiveRequiredCount(OverriddenInstance, Objective->GetGUID(), 2));

	const FEGQuestGameplayEvent Event = TagEvent(TEXT("DOP.Quest.Ritual.Completed"));
	FEGQuestRuntimeSnapshot Snapshot;
	Component->NotifyGameplayEvent(Event);
	Component->NotifyGameplayEvent(Event);
	TestTrue(TEXT("Overridden snapshot queryable"), Component->FindQuestSnapshot(OverriddenInstance, Snapshot));
	TestTrue(TEXT("Overridden instance completes at the override count"), Snapshot.IsTerminal());
	TestTrue(TEXT("Authored snapshot queryable"), Component->FindQuestSnapshot(AuthoredInstance, Snapshot));
	TestFalse(TEXT("Instance without an override is unaffected"), Snapshot.IsTerminal());

	Component->NotifyGameplayEvent(Event);
	TestTrue(TEXT("Authored snapshot queryable after third event"), Component->FindQuestSnapshot(AuthoredInstance, Snapshot));
	TestTrue(TEXT("Authored instance completes at its own count"), Snapshot.IsTerminal());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGQuestPerPublisherCursorAutomationTest,
	"QuestPlugin.Runtime.PerPublisherCursorPersistsAcrossResume",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGQuestPerPublisherCursorAutomationTest::RunTest(const FString& Parameters)
{
	UEGQuestNode_Objective* Objective = nullptr;
	UEGQuestGraph* Graph = BuildCountingObjectiveGraph(Objective);
	CastChecked<UEGQuestTracker_EventCount>(Objective->GetTracker())->RequiredCount = 10;

	AGameStateBase* FirstGameState = NewObject<AGameStateBase>();
	UEGQuestComponent* FirstComponent = NewObject<UEGQuestComponent>(FirstGameState);
	const FEGQuestOperationResult StartResult = FirstComponent->StartSharedQuest(Graph);
	const FGuid RunId = StartResult.RunId;
	TestEqual(TEXT("Start reports an applied operation"), StartResult.Status, EEGQuestOperationStatus::Applied);
	TestTrue(TEXT("Started run has a persistent id"), RunId.IsValid());

	FEGQuestGameplayEvent PublisherA = TagEvent(TEXT("DOP.Quest.Ritual.Completed"));
	PublisherA.PublisherId = TEXT("Tests.PublisherA");
	PublisherA.Sequence = 100;
	FEGQuestGameplayEvent PublisherB = PublisherA;
	PublisherB.PublisherId = TEXT("Tests.PublisherB");
	PublisherB.Sequence = 1;

	TestEqual(TEXT("Publisher A event applies"), FirstComponent->NotifyGameplayEvent(PublisherA).Status,
		EEGQuestOperationStatus::Applied);
	TestEqual(TEXT("Publisher B starts its independent cursor at one"), FirstComponent->NotifyGameplayEvent(PublisherB).Status,
		EEGQuestOperationStatus::Applied);

	FEGQuestRuntimeSnapshot Snapshot;
	TestTrue(TEXT("Snapshot is queryable after both publishers"), FirstComponent->FindQuestSnapshot(RunId, Snapshot));
	TestEqual(TEXT("Independent publishers both advance progress"), Snapshot.ActiveObjectives[0].Count, 2);
	const int32 RevisionBeforeDuplicate = Snapshot.Revision;
	const FEGQuestOperationResult DuplicateResult = FirstComponent->NotifyGameplayEvent(PublisherA);
	TestEqual(TEXT("Same-publisher duplicate is an accepted no-op"), DuplicateResult.Status,
		EEGQuestOperationStatus::AcceptedNoChange);
	FirstComponent->FindQuestSnapshot(RunId, Snapshot);
	TestEqual(TEXT("Duplicate input commits zero revisions"), Snapshot.Revision, RevisionBeforeDuplicate);
	TestEqual(TEXT("Duplicate input does not advance progress"), Snapshot.ActiveObjectives[0].Count, 2);

	FEGQuestSaveEnvelope SaveEnvelope;
	TestTrue(TEXT("Run builds the Phase 1 save envelope"), FirstComponent->BuildQuestSaveData(RunId, SaveEnvelope));
	TestEqual(TEXT("Save envelope schema is explicit"), SaveEnvelope.SchemaVersion, 1);

	AGameStateBase* RestoredGameState = NewObject<AGameStateBase>();
	UEGQuestComponent* RestoredComponent = NewObject<UEGQuestComponent>(RestoredGameState);
	const FEGQuestOperationResult ResumeResult = RestoredComponent->ResumeQuest(SaveEnvelope);
	TestEqual(TEXT("Resume preserves logical run identity"), ResumeResult.RunId, RunId);
	TestEqual(TEXT("Resume reports an applied operation"), ResumeResult.Status, EEGQuestOperationStatus::Applied);

	RestoredComponent->FindQuestSnapshot(RunId, Snapshot);
	const int32 RestoredRevision = Snapshot.Revision;
	TestEqual(TEXT("Persisted publisher cursor still dedupes after resume"),
		RestoredComponent->NotifyGameplayEvent(PublisherB).Status, EEGQuestOperationStatus::AcceptedNoChange);
	RestoredComponent->FindQuestSnapshot(RunId, Snapshot);
	TestEqual(TEXT("Post-resume duplicate commits zero revisions"), Snapshot.Revision, RestoredRevision);

	PublisherB.Sequence = 2;
	TestEqual(TEXT("Next sequence from restored publisher applies"),
		RestoredComponent->NotifyGameplayEvent(PublisherB).Status, EEGQuestOperationStatus::Applied);
	RestoredComponent->FindQuestSnapshot(RunId, Snapshot);
	TestEqual(TEXT("Restored run continues from saved progress"), Snapshot.ActiveObjectives[0].Count, 3);
	TestEqual(TEXT("Accepted event consumes exactly one revision"), Snapshot.Revision, RestoredRevision + 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGQuestTerminalShapeAutomationTest,
	"QuestPlugin.Runtime.TerminalCommandsShareCanonicalShape",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGQuestTerminalShapeAutomationTest::RunTest(const FString& Parameters)
{
	UEGQuestNode_Objective* Objective = nullptr;
	UEGQuestGraph* Graph = BuildCountingObjectiveGraph(Objective);
	AGameStateBase* GameState = NewObject<AGameStateBase>();
	UEGQuestComponent* Component = NewObject<UEGQuestComponent>(GameState);
	const FGuid EndNodeRun = Component->StartSharedQuest(Graph);
	const FGuid DirectRun = Component->StartSharedQuest(Graph);

	FEGQuestRuntimeSnapshot Before;
	Component->FindQuestSnapshot(EndNodeRun, Before);
	const FEGQuestOperationResult CompleteResult = Component->CompleteActiveObjective(EndNodeRun, Objective->GetGUID());
	TestEqual(TEXT("Objective command reports one committed revision"), CompleteResult.AfterRevision,
		CompleteResult.BeforeRevision + 1);
	const FEGQuestOperationResult AbandonResult = Component->AbandonQuest(DirectRun);
	TestEqual(TEXT("Direct terminal command reports one committed revision"), AbandonResult.AfterRevision,
		AbandonResult.BeforeRevision + 1);

	FEGQuestRuntimeSnapshot EndNodeSnapshot;
	FEGQuestRuntimeSnapshot DirectSnapshot;
	TestTrue(TEXT("End-node terminal snapshot is queryable"), Component->FindQuestSnapshot(EndNodeRun, EndNodeSnapshot));
	TestTrue(TEXT("Direct terminal snapshot is queryable"), Component->FindQuestSnapshot(DirectRun, DirectSnapshot));
	TestTrue(TEXT("End-node completion is terminal"), EndNodeSnapshot.IsTerminal());
	TestTrue(TEXT("Direct abandon is terminal"), DirectSnapshot.IsTerminal());
	for (const FEGQuestRuntimeSnapshot* Snapshot : {&EndNodeSnapshot, &DirectSnapshot})
	{
		TestFalse(TEXT("Terminal projection has no active node"), Snapshot->ActiveNodeGuid.IsValid());
		TestTrue(TEXT("Terminal projection has no stage title"), Snapshot->ActiveStageTitle.IsEmpty());
		TestTrue(TEXT("Terminal projection has no stage description"), Snapshot->ActiveStageDescription.IsEmpty());
		TestEqual(TEXT("Terminal projection has no active objectives"), Snapshot->ActiveObjectives.Num(), 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGQuestRequiredCountByTagAutomationTest,
	"QuestPlugin.Runtime.ObjectiveRequiredCountByEventTag",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGQuestRequiredCountByTagAutomationTest::RunTest(const FString& Parameters)
{
	UEGQuestNode_Objective* Objective = nullptr;
	UEGQuestGraph* Graph = BuildCountingObjectiveGraph(Objective);

	AGameStateBase* GameState = NewObject<AGameStateBase>();
	UEGQuestComponent* Component = NewObject<UEGQuestComponent>(GameState);
	const FGuid Instance = Component->StartSharedQuest(Graph);

	// This is how the game scales its ritual target: by tag, at quest start, without knowing where
	// the counting objective sits in the graph.
	const TArray<FGuid> Scaled = Component->SetObjectiveRequiredCountByEventTag(
		Instance, FGameplayTag::RequestGameplayTag(TEXT("DOP.Quest.Ritual.Completed"), false), 2);
	TestEqual(TEXT("The counting objective is found by its tag"), Scaled.Num(), 1);
	if (Scaled.Num() == 1) TestEqual(TEXT("The right objective was scaled"), Scaled[0], Objective->GetGUID());
	TestEqual(TEXT("The override is readable back"), Component->GetObjectiveRequiredCountOverride(Instance, Objective->GetGUID()), 2);

	const TArray<FGuid> Unmatched = Component->SetObjectiveRequiredCountByEventTag(
		Instance, FGameplayTag::RequestGameplayTag(TEXT("DOP.Quest.Castle.GateOpened"), false), 2);
	TestEqual(TEXT("A tag no objective counts scales nothing"), Unmatched.Num(), 0);

	const FEGQuestGameplayEvent Event = TagEvent(TEXT("DOP.Quest.Ritual.Completed"));
	FEGQuestRuntimeSnapshot Snapshot;
	Component->NotifyGameplayEvent(Event);
	TestTrue(TEXT("Snapshot queryable"), Component->FindQuestSnapshot(Instance, Snapshot));
	TestEqual(TEXT("The checklist reports the overridden target"), Snapshot.ActiveObjectives[0].RequiredCount, 2);
	TestFalse(TEXT("Not yet complete at one of two"), Snapshot.IsTerminal());

	Component->NotifyGameplayEvent(Event);
	TestTrue(TEXT("Snapshot queryable after the second event"), Component->FindQuestSnapshot(Instance, Snapshot));
	TestTrue(TEXT("Completes at the overridden count, not the authored three"), Snapshot.IsTerminal());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGQuestNegativeMagnitudeAutomationTest,
	"QuestPlugin.Runtime.NegativeMagnitudeRemovesProgress",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGQuestNegativeMagnitudeAutomationTest::RunTest(const FString& Parameters)
{
	UEGQuestNode_Objective* Objective = nullptr;
	UEGQuestGraph* Graph = BuildCountingObjectiveGraph(Objective);   // needs 3
	AGameStateBase* GameState = NewObject<AGameStateBase>();
	UEGQuestComponent* Component = NewObject<UEGQuestComponent>(GameState);
	const FGuid Instance = Component->StartSharedQuest(Graph);

	FEGQuestGameplayEvent Add = TagEvent(TEXT("DOP.Quest.Ritual.Completed"));
	Add.Magnitude = 2.0f;
	Component->NotifyGameplayEvent(Add);

	FEGQuestRuntimeSnapshot Snapshot;
	Component->FindQuestSnapshot(Instance, Snapshot);
	TestEqual(TEXT("A magnitude of 2 counts twice"), Snapshot.ActiveObjectives[0].Count, 2);

	// A magnitude that rounds to zero from below must not be treated as the default +1 tick.
	FEGQuestGameplayEvent SmallNegative = TagEvent(TEXT("DOP.Quest.Ritual.Completed"));
	SmallNegative.Magnitude = -0.4f;
	Component->NotifyGameplayEvent(SmallNegative);
	Component->FindQuestSnapshot(Instance, Snapshot);
	TestEqual(TEXT("A magnitude rounding to 0 from below adds nothing"), Snapshot.ActiveObjectives[0].Count, 2);
	TestFalse(TEXT("...and certainly does not complete the objective"), Snapshot.IsTerminal());

	FEGQuestGameplayEvent Remove = TagEvent(TEXT("DOP.Quest.Ritual.Completed"));
	Remove.Magnitude = -1.0f;
	Component->NotifyGameplayEvent(Remove);
	Component->FindQuestSnapshot(Instance, Snapshot);
	TestEqual(TEXT("A negative magnitude removes progress"), Snapshot.ActiveObjectives[0].Count, 1);

	// Progress never drops below zero, and a clamped no-op must not churn the snapshot.
	Remove.Magnitude = -5.0f;
	Component->NotifyGameplayEvent(Remove);
	Component->FindQuestSnapshot(Instance, Snapshot);
	TestEqual(TEXT("Progress never goes negative"), Snapshot.ActiveObjectives[0].Count, 0);
	const int32 RevisionAtZero = Snapshot.Revision;
	Component->NotifyGameplayEvent(Remove);
	Component->FindQuestSnapshot(Instance, Snapshot);
	TestEqual(TEXT("Removing progress that is already zero changes nothing"), Snapshot.Revision, RevisionAtZero);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGQuestDownscaleResolvesAutomationTest,
	"QuestPlugin.Runtime.LoweringTheTargetBelowProgressResolvesTheObjective",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGQuestDownscaleResolvesAutomationTest::RunTest(const FString& Parameters)
{
	// A party shrinks mid-run and the target drops below what is already counted. Waiting for one
	// more event would hang the quest when no further event can happen.
	UEGQuestNode_Objective* Objective = nullptr;
	UEGQuestGraph* Graph = BuildCountingObjectiveGraph(Objective);   // authored count 3
	AGameStateBase* GameState = NewObject<AGameStateBase>();
	UEGQuestComponent* Component = NewObject<UEGQuestComponent>(GameState);
	const FGuid Instance = Component->StartSharedQuest(Graph);

	const FEGQuestGameplayEvent Event = TagEvent(TEXT("DOP.Quest.Ritual.Completed"));
	Component->NotifyGameplayEvent(Event);
	Component->NotifyGameplayEvent(Event);

	FEGQuestRuntimeSnapshot Snapshot;
	Component->FindQuestSnapshot(Instance, Snapshot);
	TestFalse(TEXT("Two of three is not done"), Snapshot.IsTerminal());

	TestTrue(TEXT("The target is lowered to one"), Component->SetObjectiveRequiredCount(Instance, Objective->GetGUID(), 1));
	Component->FindQuestSnapshot(Instance, Snapshot);
	TestTrue(TEXT("Lowering the target under the count resolves the objective there and then"), Snapshot.IsTerminal());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGQuestGameplayNotifyAutomationTest,
	"QuestPlugin.Runtime.GameplayNotifyRelaysThroughComponent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGQuestGameplayNotifyAutomationTest::RunTest(const FString& Parameters)
{
	UEGQuestNode_Objective* Objective = nullptr;
	UEGQuestGraph* Graph = BuildCountingObjectiveGraph(Objective);
	UEGQuestNode_Stage* Stage = Cast<UEGQuestNode_Stage>(Graph->GetNodes()[0]);
	const FGameplayTag NotifyTag = FGameplayTag::RequestGameplayTag(TEXT("DOP.Quest.Ritual.Completed"), false);
	UEGQuestEvent_GameplayNotify* Notify = NewObject<UEGQuestEvent_GameplayNotify>(Graph);
	Notify->SetNotifyTag(NotifyTag);
	Notify->SetNotifyMagnitude(75.0f);
	// No tag: must not broadcast.
	UEGQuestEvent_GameplayNotify* InvalidNotify = NewObject<UEGQuestEvent_GameplayNotify>(Graph);
	// Enter events live on the stage: it is the only thing that is ever entered.
	Stage->SetNodeEnterEvents({Notify, InvalidNotify});

	AGameStateBase* GameState = NewObject<AGameStateBase>();
	UEGQuestComponent* Component = NewObject<UEGQuestComponent>(GameState);
	UEGQuestTestGameplayNotifySink* Sink = NewObject<UEGQuestTestGameplayNotifySink>();
	Component->OnQuestGameplayNotify.AddDynamic(Sink, &UEGQuestTestGameplayNotifySink::HandleNotify);

	// The stage is entered during Start, so its enter-event notify must already be relayed
	// (stamped with the instance guid the call returns).
	const FGuid Instance = Component->StartSharedQuest(Graph);
	TestTrue(TEXT("Quest started"), Instance.IsValid());
	TestEqual(TEXT("Exactly one notify relayed (invalid-tag notify dropped)"), Sink->ReceivedInstances.Num(), 1);
	if (Sink->ReceivedInstances.Num() == 1)
	{
		TestEqual(TEXT("Notify carries the owning instance guid"), Sink->ReceivedInstances[0], Instance);
		TestEqual(TEXT("Notify carries the authored tag"), Sink->ReceivedTags[0], NotifyTag);
		TestEqual(TEXT("Notify carries the authored magnitude"), Sink->ReceivedMagnitudes[0], 75.0f);
	}
	return true;
}

namespace
{
	class FMutableQuestClock final : public IEGQuestClock
	{
	public:
		double Now = 0.0;
		double GetServerTime() const override { return Now; }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGQuestInjectedClockAutomationTest,
	"QuestPlugin.Runtime.InjectedClockOwnsRunTimestamps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGQuestInjectedClockAutomationTest::RunTest(const FString& Parameters)
{
	UEGQuestGraph* Graph = BuildTwoStageGraph();
	UEGQuestNode_Objective* FirstObjective = CastChecked<UEGQuestNode_Objective>(Graph->GetNodes()[1]);
	UEGQuestNode_Objective* SecondObjective = CastChecked<UEGQuestNode_Objective>(Graph->GetNodes()[3]);
	AGameStateBase* GameState = NewObject<AGameStateBase>();
	UEGQuestComponent* Component = NewObject<UEGQuestComponent>(GameState);

	const TSharedPtr<FMutableQuestClock> Clock = MakeShared<FMutableQuestClock>();
	Clock->Now = 12.5;
	Component->SetQuestClock(Clock);
	const FGuid Instance = Component->StartSharedQuest(Graph);

	FEGQuestRuntimeSnapshot Snapshot;
	TestTrue(TEXT("Clocked quest starts"), Instance.IsValid());
	TestTrue(TEXT("Clocked quest snapshot exists"), Component->FindQuestSnapshot(Instance, Snapshot));
	TestEqual(TEXT("Start timestamp comes from the injected clock"), Snapshot.StartServerTime, 12.5);

	Clock->Now = 77.25;
	Component->CompleteActiveObjective(Instance, FirstObjective->GetGUID());
	Component->CompleteActiveObjective(Instance, SecondObjective->GetGUID());
	Component->FindQuestSnapshot(Instance, Snapshot);
	TestTrue(TEXT("Clocked quest reaches a terminal state"), Snapshot.IsTerminal());
	TestEqual(TEXT("Terminal timestamp comes from the same injected clock"), Snapshot.CompletionServerTime, 77.25);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGQuestPostCommitReentryAutomationTest,
	"QuestPlugin.Runtime.PostCommitCallbackReentryQueuesNewInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGQuestPostCommitReentryAutomationTest::RunTest(const FString& Parameters)
{
	UEGQuestGraph* Graph = BuildTwoStageGraph();
	UEGQuestNode_Objective* Objective = CastChecked<UEGQuestNode_Objective>(Graph->GetNodes()[1]);
	AGameStateBase* GameState = NewObject<AGameStateBase>();
	UEGQuestComponent* Component = NewObject<UEGQuestComponent>(GameState);
	const FGuid RunId = Component->StartSharedQuest(Graph);

	FEGQuestViewSnapshot Before;
	TestTrue(TEXT("Run starts"), RunId.IsValid());
	TestTrue(TEXT("Initial projection exists"), Component->FindQuestSnapshot(RunId, Before));

	UEGQuestTestReentrySink* Sink = NewObject<UEGQuestTestReentrySink>();
	Sink->Configure(Component, RunId, Objective->GetGUID());
	Component->OnGameplayEventAccepted.AddDynamic(Sink, &UEGQuestTestReentrySink::HandleAcceptedEvent);

	FEGQuestGameplayEvent Event = TagEvent(TEXT("DOP.Quest.Ritual.Completed"));
	Event.PublisherId = TEXT("Automation.Reentry");
	const FEGQuestOperationResult EventResult = Component->NotifyGameplayEvent(Event);

	FEGQuestViewSnapshot After;
	TestTrue(TEXT("Projection remains queryable"), Component->FindQuestSnapshot(RunId, After));
	TestEqual(TEXT("Callback observes the committed projection"), Sink->ProjectionRevisionSeenByCallback, Before.Revision);
	TestEqual(TEXT("Mutation requested by callback reports deferred"), Sink->CallbackStatus, EEGQuestOperationStatus::Deferred);
	TestEqual(TEXT("Queued callback mutation commits as a separate revision"), After.Revision, Before.Revision + 1);
	TestEqual(TEXT("Queued callback mutation updated the projected objective"), After.ActiveObjectives[0].RequiredCount, 7);
	TestTrue(TEXT("Original event was accepted without an inline mutation"), EventResult.IsSuccess());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGQuestFactsStoreAutomationTest,
	"QuestPlugin.Facts.WorldPlayerSaveAndReplicationPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGQuestFactsStoreAutomationTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	TestNotNull(TEXT("Automation world exists"), World);
	if (!World) return false;
	UEGQuestFactsSubsystem* Facts = World->GetSubsystem<UEGQuestFactsSubsystem>();
	TestNotNull(TEXT("Facts subsystem is world-owned"), Facts);
	if (!Facts) return false;

	const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TEXT("DOP.Quest.Ritual.Completed"), false);
	APlayerState* FirstPlayer = NewObject<APlayerState>(World);
	APlayerState* SecondPlayer = NewObject<APlayerState>(World);
	FirstPlayer->SetPlayerId(11);
	SecondPlayer->SetPlayerId(22);

	TestTrue(TEXT("World fact set"), Facts->SetFact(Tag, 2));
	TestTrue(TEXT("World fact increments"), Facts->AddToFact(Tag, 3));
	TestEqual(TEXT("World fact is persistent truth"), Facts->GetFact(Tag), 5);
	TestTrue(TEXT("First player fact set"), Facts->SetFact(Tag, 4, EEGQuestFactScope::Player, FirstPlayer));
	TestTrue(TEXT("Second player fact set"), Facts->SetFact(Tag, 9, EEGQuestFactScope::Player, SecondPlayer));
	TestEqual(TEXT("Player facts are isolated"), Facts->GetFact(Tag, EEGQuestFactScope::Player, FirstPlayer), 4);
	TestEqual(TEXT("Second player keeps its own value"), Facts->GetFact(Tag, EEGQuestFactScope::Player, SecondPlayer), 9);

	const FEGQuestFactsSaveData SaveData = Facts->ExtractFactsSaveData();
	TestEqual(TEXT("Save contains world plus two player entries"), SaveData.Entries.Num(), 3);
	Facts->ClearFact(Tag);
	Facts->ClearFact(Tag, EEGQuestFactScope::Player, FirstPlayer);
	Facts->ClearFact(Tag, EEGQuestFactScope::Player, SecondPlayer);
	TestFalse(TEXT("Clear removes presence"), Facts->HasFact(Tag));
	TestTrue(TEXT("Schema-one facts restore"), Facts->RestoreFactsSaveData(SaveData));
	TestEqual(TEXT("World value restored"), Facts->GetFact(Tag), 5);
	TestEqual(TEXT("Player value restored by stable id"), Facts->GetFact(Tag, EEGQuestFactScope::Player, FirstPlayer), 4);

	UEGQuestPluginSettings* Settings = GetMutableDefault<UEGQuestPluginSettings>();
	const FGameplayTagContainer PreviousPolicy = Settings->ReplicatedFactRootTags;
	Settings->ReplicatedFactRootTags.Reset();
	TestFalse(TEXT("Facts are server-only by default"), Facts->ShouldReplicateFact(Tag));
	Settings->ReplicatedFactRootTags.AddTag(Tag);
	TestTrue(TEXT("Allowlisted fact opts into relay replication"), Facts->ShouldReplicateFact(Tag));
	Settings->ReplicatedFactRootTags = PreviousPolicy;
	return true;
}

namespace
{
	struct FTrackerGraphFixture
	{
		UEGQuestGraph* Graph = nullptr;
		UEGQuestNode_Objective* Objective = nullptr;
	};

	FTrackerGraphFixture BuildTrackerGraph(const TFunction<void(UEGQuestNode_Objective&)>& Configure)
	{
		FTrackerGraphFixture Result;
		Result.Graph = NewObject<UEGQuestGraph>();
		Result.Graph->RegenerateGUID();
		UEGQuestNode_Start* Start = Result.Graph->ConstructQuestNode<UEGQuestNode_Start>();
		UEGQuestNode_Stage* Stage = Result.Graph->ConstructQuestNode<UEGQuestNode_Stage>();
		Result.Objective = Result.Graph->ConstructQuestNode<UEGQuestNode_Objective>();
		UEGQuestNode_End* End = Result.Graph->ConstructQuestNode<UEGQuestNode_End>();
		for (UEGQuestNode* Node : TArray<UEGQuestNode*>{Start, Stage, Result.Objective, End}) Node->RegenerateGUID();
		Result.Objective->SetNodeText(FText::FromString(TEXT("Tracker objective")));
		Configure(*Result.Objective);
		Result.Graph->SetNodes({Stage, Result.Objective, End});
		Result.Graph->SetStartNodes({Start});
		Start->SetNodeChildren({Arrow(0)});
		Stage->SetNodeChildren({Arrow(1)});
		Result.Objective->SetNodeChildren({Arrow(2)});
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGQuestTrackerLibrarySimulatorAutomationTest,
	"QuestPlugin.Trackers.LibraryRestoreAndSimulator",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGQuestTrackerLibrarySimulatorAutomationTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	TestNotNull(TEXT("Tracker automation world exists"), World);
	if (!World) return false;
	UEGQuestFactsSubsystem* Facts = World->GetSubsystem<UEGQuestFactsSubsystem>();
	AGameStateBase* GameState = World->GetGameState();
	if (!GameState) GameState = World->SpawnActor<AGameStateBase>();
	TestNotNull(TEXT("Facts service exists"), Facts);
	TestNotNull(TEXT("World-owned quest host exists"), GameState);
	if (!Facts || !GameState) return false;

	const FGameplayTag Ritual = FGameplayTag::RequestGameplayTag(TEXT("DOP.Quest.Ritual.Completed"), false);
	const FGameplayTag Gate = FGameplayTag::RequestGameplayTag(TEXT("DOP.Quest.Castle.GateOpened"), false);
	const FGameplayTag Book = FGameplayTag::RequestGameplayTag(TEXT("DOP.Quest.Castle.BookAcquired"), false);

	// Legacy fields migrate to an ordinary inline EventCount tracker. The fields are load-only with
	// no setters, so write them the way serialization does: through the property system.
	UEGQuestNode_Objective* Legacy = NewObject<UEGQuestNode_Objective>();
	{
		FStructProperty* TagProperty = CastFieldChecked<FStructProperty>(
			UEGQuestNode_Objective::StaticClass()->FindPropertyByName(TEXT("EventTag")));
		*TagProperty->ContainerPtrToValuePtr<FGameplayTag>(Legacy) = Ritual;
		FIntProperty* CountProperty = CastFieldChecked<FIntProperty>(
			UEGQuestNode_Objective::StaticClass()->FindPropertyByName(TEXT("RequiredCount")));
		CountProperty->SetPropertyValue_InContainer(Legacy, 3);
	}
	Legacy->PostLoad();
	const UEGQuestTracker_EventCount* Migrated = Cast<UEGQuestTracker_EventCount>(Legacy->GetTracker());
	TestNotNull(TEXT("Legacy event fields migrate to EventCount"), Migrated);
	if (Migrated) TestEqual(TEXT("Migration preserves required count"), Migrated->RequiredCount, 3);

	// Fact is retroactive: activation observes truth that predates the quest.
	Facts->SetFact(Ritual, 2);
	FTrackerGraphFixture FactGraph = BuildTrackerGraph([&](UEGQuestNode_Objective& Objective)
	{
		UEGQuestTracker_Fact* Tracker = NewObject<UEGQuestTracker_Fact>(&Objective);
		Tracker->FactTag = Ritual;
		Tracker->Value = 2;
		Objective.SetTracker(Tracker);
	});
	UEGQuestComponent* FactComponent = NewObject<UEGQuestComponent>(GameState);
	FEGQuestSimulator FactSim(*FactComponent, *Facts, 10.0);
	TestTrue(TEXT("Fact tracker starts"), FactSim.Start(*FactGraph.Graph).IsSuccess());
	FString Failure;
	TestTrue(TEXT("Retroactive fact completes through production settle"), FactSim.AssertResult(EEGQuestLifecycleState::Completed, Failure));

	// EventCount consumes a tag query and magnitude predicate, not a privileged exact-tag cast.
	FTrackerGraphFixture EventGraph = BuildTrackerGraph([&](UEGQuestNode_Objective& Objective)
	{
		UEGQuestTracker_EventCount* Tracker = NewObject<UEGQuestTracker_EventCount>(&Objective);
		Tracker->EventQuery = FGameplayTagQuery::MakeQuery_MatchTag(Gate);
		Tracker->RequiredCount = 2;
		Tracker->bUseMagnitudePredicate = true;
		Tracker->MagnitudeValue = 1.0f;
		Objective.SetTracker(Tracker);
	});
	UEGQuestComponent* EventComponent = NewObject<UEGQuestComponent>(GameState);
	FEGQuestSimulator EventSim(*EventComponent, *Facts);
	EventSim.Start(*EventGraph.Graph);
	EventSim.Drive(Gate, 0.5f);
	FEGQuestViewSnapshot Snapshot;
	EventSim.GetSnapshot(Snapshot);
	TestEqual(TEXT("Magnitude predicate rejects a small event"), Snapshot.ActiveObjectives[0].Count, 0);
	EventSim.Drive(Gate);
	EventSim.Drive(Gate);
	TestTrue(TEXT("EventCount query reaches its target"), EventSim.AssertResult(EEGQuestLifecycleState::Completed, Failure));

	// Sequence persists its cursor and reset policy.
	FTrackerGraphFixture SequenceGraph = BuildTrackerGraph([&](UEGQuestNode_Objective& Objective)
	{
		UEGQuestTracker_Sequence* Tracker = NewObject<UEGQuestTracker_Sequence>(&Objective);
		Tracker->OrderedTags = {Gate, Book};
		Objective.SetTracker(Tracker);
	});
	UEGQuestComponent* SequenceComponent = NewObject<UEGQuestComponent>(GameState);
	FEGQuestSimulator SequenceSim(*SequenceComponent, *Facts);
	SequenceSim.Start(*SequenceGraph.Graph);
	SequenceSim.Drive(Book);
	SequenceSim.Drive(Gate);
	SequenceSim.GetSnapshot(Snapshot);
	TestEqual(TEXT("Sequence cursor is projected from run state"), Snapshot.ActiveObjectives[0].SequenceIndex, 1);
	SequenceSim.Drive(Book);
	TestTrue(TEXT("Ordered sequence completes"), SequenceSim.AssertResult(EEGQuestLifecycleState::Completed, Failure));

	// Distinct ignores a repeated context key.
	FTrackerGraphFixture DistinctGraph = BuildTrackerGraph([&](UEGQuestNode_Objective& Objective)
	{
		UEGQuestTracker_Distinct* Tracker = NewObject<UEGQuestTracker_Distinct>(&Objective);
		Tracker->ExactEventTag = Gate;
		Tracker->RequiredDistinctCount = 2;
		Objective.SetTracker(Tracker);
	});
	UEGQuestComponent* DistinctComponent = NewObject<UEGQuestComponent>(GameState);
	FEGQuestSimulator DistinctSim(*DistinctComponent, *Facts);
	DistinctSim.Start(*DistinctGraph.Graph);
	DistinctSim.Drive(Gate, 1.0f, TEXT("Brazier.A"));
	DistinctSim.Drive(Gate, 1.0f, TEXT("Brazier.A"));
	DistinctSim.GetSnapshot(Snapshot);
	TestEqual(TEXT("Distinct key is counted once"), Snapshot.ActiveObjectives[0].DistinctKeys.Num(), 1);
	DistinctSim.Drive(Gate, 1.0f, TEXT("Brazier.B"));
	TestTrue(TEXT("Second unique key completes distinct tracker"), DistinctSim.AssertResult(EEGQuestLifecycleState::Completed, Failure));

	// Predicate and composite use the same tracker contract and declared fact dependencies.
	FTrackerGraphFixture CompositeGraph = BuildTrackerGraph([&](UEGQuestNode_Objective& Objective)
	{
		UEGQuestTracker_Composite* Composite = NewObject<UEGQuestTracker_Composite>(&Objective);
		UEGQuestTracker_Predicate* PredicateTracker = NewObject<UEGQuestTracker_Predicate>(Composite);
		UEGQuestPredicate_Fact* Predicate = NewObject<UEGQuestPredicate_Fact>(PredicateTracker);
		Predicate->FactTag = Ritual;
		Predicate->Value = 2;
		PredicateTracker->Predicate = Predicate;
		UEGQuestTracker_Fact* FactTracker = NewObject<UEGQuestTracker_Fact>(Composite);
		FactTracker->FactTag = Ritual;
		FactTracker->Value = 1;
		Composite->Children = {PredicateTracker, FactTracker};
		Objective.SetTracker(Composite);
	});
	UEGQuestComponent* CompositeComponent = NewObject<UEGQuestComponent>(GameState);
	FEGQuestSimulator CompositeSim(*CompositeComponent, *Facts);
	CompositeSim.Start(*CompositeGraph.Graph);
	TestTrue(TEXT("Composite ALL over predicate/fact children completes"), CompositeSim.AssertResult(EEGQuestLifecycleState::Completed, Failure));

	// Timer state is an absolute injected-clock deadline and restore must not reset it.
	FTrackerGraphFixture TimerGraph = BuildTrackerGraph([&](UEGQuestNode_Objective& Objective)
	{
		UEGQuestTracker_Timer* Tracker = NewObject<UEGQuestTracker_Timer>(&Objective);
		Tracker->DurationSeconds = 5.0;
		Objective.SetTracker(Tracker);
	});
	UEGQuestComponent* TimerComponent = NewObject<UEGQuestComponent>(GameState);
	FEGQuestSimulator TimerSim(*TimerComponent, *Facts, 100.0);
	TimerSim.Start(*TimerGraph.Graph);
	TimerSim.Advance(2.0);
	FEGQuestSaveEnvelope TimerSave;
	TestTrue(TEXT("Timer run saves"), TimerComponent->BuildQuestSaveData(TimerSim.GetRunId(), TimerSave));
	TestEqual(TEXT("Timer save stores remaining duration"), TimerSave.RunRecord.ActiveObjectives[0].TrackerEndServerTime, 3.0);
	UEGQuestComponent* RestoredComponent = NewObject<UEGQuestComponent>(GameState);
	FEGQuestSimulator RestoredSim(*RestoredComponent, *Facts, 102.0);
	TestTrue(TEXT("Timer run resumes"), RestoredSim.Resume(TimerSave).IsSuccess());
	RestoredSim.Advance(2.0);
	TestTrue(TEXT("Restored activation does not restart timer"), RestoredSim.AssertResult(EEGQuestLifecycleState::Active, Failure));
	RestoredSim.Advance(1.0);
	TestTrue(TEXT("Restored timer expires at original deadline"), RestoredSim.AssertResult(EEGQuestLifecycleState::Completed, Failure));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGQuestTrackerPulseDriverAutomationTest,
	"QuestPlugin.Trackers.PulseDriverExpiresTimerWithoutManualPulse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGQuestTrackerPulseDriverAutomationTest::RunTest(const FString& Parameters)
{
	// The component never ticks, so a timer tracker can only fire if something calls
	// PulseActiveTrackers. Every other timer test drives that by hand through the simulator, which
	// would pass even with no driver at all. This one never pulses: the only thing that advances the
	// quest is the component's own timer, ticked through the world's timer manager.
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	TestNotNull(TEXT("Pulse driver world exists"), World);
	if (!World) return false;
	AGameStateBase* GameState = World->GetGameState();
	if (!GameState) GameState = World->SpawnActor<AGameStateBase>();
	TestNotNull(TEXT("World-owned quest host exists"), GameState);
	if (!GameState) return false;

	FTrackerGraphFixture TimerGraph = BuildTrackerGraph([](UEGQuestNode_Objective& Objective)
	{
		UEGQuestTracker_Timer* Tracker = NewObject<UEGQuestTracker_Timer>(&Objective);
		Tracker->DurationSeconds = 5.0;
		Objective.SetTracker(Tracker);
	});

	UEGQuestComponent* Component = NewObject<UEGQuestComponent>(GameState);
	const TSharedPtr<FEGQuestSimulationClock> Clock = MakeShared<FEGQuestSimulationClock>(100.0);
	Component->SetQuestClock(Clock);
	Component->TrackerPulseInterval = 0.5f;

	const FEGQuestOperationResult Started = Component->StartSharedQuest(TimerGraph.Graph);
	TestTrue(TEXT("Timer quest starts"), Started.IsSuccess());
	if (!Started.IsSuccess()) return false;
	const FGuid RunId = Started.RunId;

	FEGQuestViewSnapshot Snapshot;
	TestTrue(TEXT("Started run is visible"), Component->FindQuestSnapshot(RunId, Snapshot));
	TestEqual(TEXT("Timer objective is pending before its deadline"),
		Snapshot.LifecycleState, EEGQuestLifecycleState::Active);

	// Past the 5s deadline in quest time, but nothing has pulsed yet.
	Clock->Advance(6.0);
	TestTrue(TEXT("Passing the deadline alone does not resolve the timer"),
		Component->FindQuestSnapshot(RunId, Snapshot) && Snapshot.LifecycleState == EEGQuestLifecycleState::Active);

	// Only the driver from here: no PulseActiveTrackers call, just the world's timers.
	//
	// Two FTimerManager details make a single Tick() useless here, and both are about frames rather
	// than about the driver. Tick() early-outs on HasBeenTickedThisFrame, and a timer set on a frame
	// the manager has not yet ticked starts Pending - the next Tick only promotes it to the active
	// heap. So drive several frames: one to promote, the rest to reach the 0.5s interval.
	for (int32 Frame = 0; Frame < 4; ++Frame)
	{
		++GFrameCounter;
		World->GetTimerManager().Tick(0.6f);
	}

	TestTrue(TEXT("Driven run is still visible"), Component->FindQuestSnapshot(RunId, Snapshot));
	TestEqual(TEXT("The pulse driver alone expires the timer objective"),
		Snapshot.LifecycleState, EEGQuestLifecycleState::Completed);

	// Negative control: identical run with the driver switched off. If this one completed too, the
	// assertion above would be proving something other than the driver.
	FTrackerGraphFixture UndrivenGraph = BuildTrackerGraph([](UEGQuestNode_Objective& Objective)
	{
		UEGQuestTracker_Timer* Tracker = NewObject<UEGQuestTracker_Timer>(&Objective);
		Tracker->DurationSeconds = 5.0;
		Objective.SetTracker(Tracker);
	});
	UEGQuestComponent* Undriven = NewObject<UEGQuestComponent>(GameState);
	const TSharedPtr<FEGQuestSimulationClock> UndrivenClock = MakeShared<FEGQuestSimulationClock>(100.0);
	Undriven->SetQuestClock(UndrivenClock);
	Undriven->TrackerPulseInterval = 0.0f;

	const FEGQuestOperationResult UndrivenStarted = Undriven->StartSharedQuest(UndrivenGraph.Graph);
	TestTrue(TEXT("Undriven timer quest starts"), UndrivenStarted.IsSuccess());
	if (!UndrivenStarted.IsSuccess()) return false;
	UndrivenClock->Advance(6.0);
	for (int32 Frame = 0; Frame < 4; ++Frame)
	{
		++GFrameCounter;
		World->GetTimerManager().Tick(0.6f);
	}
	FEGQuestViewSnapshot UndrivenSnapshot;
	TestTrue(TEXT("Undriven run is visible"), Undriven->FindQuestSnapshot(UndrivenStarted.RunId, UndrivenSnapshot));
	TestEqual(TEXT("With the driver disabled the timer objective never fires"),
		UndrivenSnapshot.LifecycleState, EEGQuestLifecycleState::Active);
	return true;
}

namespace
{
	struct FMultiTrackFixture
	{
		UEGQuestGraph* Graph = nullptr;
		UEGQuestNode_Objective* MainObjective = nullptr;
		UEGQuestNode_Objective* SentinelObjective = nullptr;
	};

	FMultiTrackFixture BuildMultiTrackGraph()
	{
		FMultiTrackFixture Result;
		Result.Graph = NewObject<UEGQuestGraph>();
		Result.Graph->RegenerateGUID();

		UEGQuestNode_Start* MainStart = Result.Graph->ConstructQuestNode<UEGQuestNode_Start>();
		UEGQuestNode_Start* SentinelStart = Result.Graph->ConstructQuestNode<UEGQuestNode_Start>();
		UEGQuestNode_Stage* MainStage = Result.Graph->ConstructQuestNode<UEGQuestNode_Stage>();
		Result.MainObjective = Result.Graph->ConstructQuestNode<UEGQuestNode_Objective>();
		UEGQuestNode_Stage* SentinelStage = Result.Graph->ConstructQuestNode<UEGQuestNode_Stage>();
		Result.SentinelObjective = Result.Graph->ConstructQuestNode<UEGQuestNode_Objective>();
		UEGQuestNode_End* FailedEnd = Result.Graph->ConstructQuestNode<UEGQuestNode_End>();
		for (UEGQuestNode* Node : TArray<UEGQuestNode*>{
			MainStart, SentinelStart, MainStage, Result.MainObjective, SentinelStage,
			Result.SentinelObjective, FailedEnd})
		{
			Node->RegenerateGUID();
		}

		MainStart->SetTrackName(TEXT("Main"));
		MainStart->SetTrackType(EEGQuestTrackType::Main);
		MainStart->SetEntryPriority(0);
		SentinelStart->SetTrackName(TEXT("Watcher"));
		SentinelStart->SetTrackType(EEGQuestTrackType::Sentinel);
		SentinelStart->SetEntryPriority(1);
		MainStage->SetTitle(FText::FromString(TEXT("Main investigation")));
		SentinelStage->SetTitle(FText::FromString(TEXT("Watch the plague")));
		Result.MainObjective->SetNodeText(FText::FromString(TEXT("Find the cure")));
		Result.SentinelObjective->SetNodeText(FText::FromString(TEXT("Do not alert the horde")));
		SetFailEventCount(*Result.SentinelObjective, TEXT("DOP.Quest.Castle.GateOpened"), 1);
		FailedEnd->SetQuestResult(EEGQuestResult::Failed);

		Result.Graph->SetNodes({MainStage, Result.MainObjective, SentinelStage, Result.SentinelObjective, FailedEnd});
		Result.Graph->SetStartNodes({MainStart, SentinelStart});
		MainStart->SetNodeChildren({Arrow(0)});
		MainStage->SetNodeChildren({Arrow(1)});
		SentinelStart->SetNodeChildren({Arrow(2)});
		SentinelStage->SetNodeChildren({Arrow(3)});
		Result.SentinelObjective->SetNodeChildren({Arrow(4, EEGQuestArrowOutcome::Fail)});
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGQuestMultiTrackAutomationTest,
	"QuestPlugin.Tracks.SentinelGlobalEndAndObsolete",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGQuestMultiTrackAutomationTest::RunTest(const FString& Parameters)
{
	const FMultiTrackFixture Fixture = BuildMultiTrackGraph();
	AGameStateBase* GameState = NewObject<AGameStateBase>();
	UEGQuestComponent* Component = NewObject<UEGQuestComponent>(GameState);
	const FGuid RunId = Component->StartSharedQuest(Fixture.Graph);
	FEGQuestViewSnapshot Snapshot;
	TestTrue(TEXT("Multi-track run starts"), RunId.IsValid());
	TestTrue(TEXT("Initial projection exists"), Component->FindQuestSnapshot(RunId, Snapshot));
	TestEqual(TEXT("Projection contains main plus sentinel"), Snapshot.Tracks.Num(), 2);
	if (Snapshot.Tracks.Num() != 2) return false;
	TestEqual(TEXT("Main is projected first"), Snapshot.Tracks[0].TrackName, FName(TEXT("Main")));
	TestEqual(TEXT("Sentinel retains authored order"), Snapshot.Tracks[1].TrackName, FName(TEXT("Watcher")));
	TestEqual(TEXT("Legacy convenience fields map to main"), Snapshot.ActiveNodeGuid, Snapshot.Tracks[0].ActiveNodeGuid);
	TestEqual(TEXT("Each track has its own objective"), Snapshot.Tracks[1].ActiveObjectives.Num(), 1);

	FEGQuestSaveEnvelope Save;
	TestTrue(TEXT("Multi-track state saves"), Component->BuildQuestSaveData(RunId, Save));
	AGameStateBase* RestoredGameState = NewObject<AGameStateBase>();
	UEGQuestComponent* Restored = NewObject<UEGQuestComponent>(RestoredGameState);
	TestTrue(TEXT("Multi-track state resumes"), Restored->ResumeQuest(Save).IsSuccess());
	TestTrue(TEXT("Restored projection exists"), Restored->FindQuestSnapshot(RunId, Snapshot));
	TestEqual(TEXT("Sentinel is reconstructed on resume"), Snapshot.Tracks.Num(), 2);

	Restored->NotifyGameplayEvent(TagEvent(TEXT("DOP.Quest.Castle.GateOpened")));
	TestTrue(TEXT("Terminal projection remains queryable"), Restored->FindQuestSnapshot(RunId, Snapshot));
	TestEqual(TEXT("Sentinel end resolves the whole logical quest"), Snapshot.LifecycleState, EEGQuestLifecycleState::Failed);
	TestEqual(TEXT("Terminal run retains both track histories"), Snapshot.Tracks.Num(), 2);
	TestEqual(TEXT("Main active checklist is cleared"), Snapshot.Tracks[0].ActiveObjectives.Num(), 0);
	TestEqual(TEXT("Unfinished main work is retained as history"), Snapshot.Tracks[0].ObjectiveHistory.Num(), 1);
	if (Snapshot.Tracks[0].ObjectiveHistory.Num() == 1)
	{
		TestEqual(TEXT("Unfinished main objective becomes obsolete"),
			Snapshot.Tracks[0].ObjectiveHistory[0].Outcome, EEGQuestObjectiveOutcome::Obsolete);
	}
	TestEqual(TEXT("Sentinel failure is retained as history"), Snapshot.Tracks[1].ObjectiveHistory.Num(), 1);
	if (Snapshot.Tracks[1].ObjectiveHistory.Num() == 1)
	{
		TestEqual(TEXT("Sentinel objective retains failed outcome"),
			Snapshot.Tracks[1].ObjectiveHistory[0].Outcome, EEGQuestObjectiveOutcome::Failed);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGQuestRolesAutomationTest,
	"QuestPlugin.Roles.RegistryTextMarkersTemplateAndSave",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGQuestRolesAutomationTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	TestNotNull(TEXT("Role test world exists"), World);
	if (!World) return false;
	AGameStateBase* GameState = World->GetGameState();
	if (!GameState) GameState = World->SpawnActor<AGameStateBase>();
	UEGQuestTargetRegistrySubsystem* Registry = World->GetSubsystem<UEGQuestTargetRegistrySubsystem>();
	TestNotNull(TEXT("Role registry is world-owned"), Registry);
	if (!GameState || !Registry) return false;

	const FGameplayTag TargetTag = FGameplayTag::RequestGameplayTag(TEXT("DOP.Quest.Castle.GateOpened"), false);
	auto SpawnTarget = [World, TargetTag](const TCHAR* Id, const TCHAR* Label, const FVector& Location)
	{
		AActor* Actor = World->SpawnActor<AActor>();
		Actor->SetActorLocation(Location);
		UEGQuestTargetComponent* Target = NewObject<UEGQuestTargetComponent>(Actor);
		Target->SetStableId(FName(Id));
		Target->SetDisplayName(FText::FromString(Label));
		FGameplayTagContainer Tags;
		Tags.AddTag(TargetTag);
		Target->SetTargetTags(Tags);
		Target->RegisterComponent();
		return Target;
	};
	UEGQuestTargetComponent* Alpha = SpawnTarget(TEXT("Target.Alpha"), TEXT("Alpha"), FVector(10.0, 0.0, 0.0));
	UEGQuestTargetComponent* Beta = SpawnTarget(TEXT("Target.Beta"), TEXT("Beta"), FVector(100.0, 0.0, 0.0));

	FTrackerGraphFixture Fixture = BuildTrackerGraph([](UEGQuestNode_Objective& Objective)
	{
		Objective.SetNodeText(FText::FromString(TEXT("Find {Role.Target}")));
		SetEventCount(Objective, TEXT("DOP.Quest.Ritual.Completed"), 3);
	});
	UEGQuestRoleResolver_RegistryQuery* Resolver = NewObject<UEGQuestRoleResolver_RegistryQuery>();
	Resolver->TargetQuery = FGameplayTagQuery::MakeQuery_MatchTag(TargetTag);
	Resolver->Selection = EEGQuestRoleSelection::Nearest;
	FEGQuestRoleResolveContext ResolveContext;
	ResolveContext.World = World;
	ResolveContext.Origin.SetLocation(FVector::ZeroVector);
	ResolveContext.RunId = FGuid::NewGuid();
	TArray<FEGQuestEntityHandle> ResolvedHandles;
	TestTrue(TEXT("Registry query resolver finds a target"), Resolver->Resolve(ResolveContext, TEXT("Target"), ResolvedHandles));
	TestEqual(TEXT("Nearest selection is deterministic"), ResolvedHandles[0].StableId, FName(TEXT("Target.Alpha")));

	UEGQuestComponent* Component = NewObject<UEGQuestComponent>(GameState);
	FEGQuestTemplateParameters ResolvedTemplate;
	FEGQuestRoleBinding& ResolvedBinding = ResolvedTemplate.RoleBindings.AddDefaulted_GetRef();
	ResolvedBinding.RoleName = TEXT("Target");
	ResolvedBinding.Handles = ResolvedHandles;
	const FGuid ResolvedRun = Component->StartQuestFromTemplate(Fixture.Graph, ResolvedTemplate);
	FEGQuestViewSnapshot Snapshot;
	TestTrue(TEXT("Resolver-backed quest starts"), ResolvedRun.IsValid());
	TestTrue(TEXT("Resolver-backed projection exists"), Component->FindQuestSnapshot(ResolvedRun, Snapshot));
	TestEqual(TEXT("Nearest registry target formats journal text"), Snapshot.ActiveObjectives[0].Text.ToString(), FString(TEXT("Find Alpha")));
	TestEqual(TEXT("Resolved role emits one marker"), Snapshot.RoleMarkers.Num(), 1);
	if (Snapshot.RoleMarkers.Num() == 1)
	{
		TestEqual(TEXT("Marker uses the role handle"), Snapshot.RoleMarkers[0].Handle.StableId, FName(TEXT("Target.Alpha")));
		TestTrue(TEXT("Loaded marker carries a transform"), Snapshot.RoleMarkers[0].bResolved);
	}

	FEGQuestTemplateParameters Template;
	FEGQuestRoleBinding& Binding = Template.RoleBindings.AddDefaulted_GetRef();
	Binding.RoleName = TEXT("Target");
	Binding.Handles.Add(Beta->GetEntityHandle());
	Binding.LossPolicy = EEGQuestRoleLossPolicy::Notify;
	FEGQuestObjectiveCountOverride& Override = Template.ObjectiveCountOverrides.AddDefaulted_GetRef();
	Override.ObjectiveGuid = Fixture.Objective->GetGUID();
	Override.RequiredCount = 2;
	const FGuid TemplateRun = Component->StartQuestFromTemplate(Fixture.Graph, Template);
	TestTrue(TEXT("Typed template quest starts"), TemplateRun.IsValid());
	Component->FindQuestSnapshot(TemplateRun, Snapshot);
	TestEqual(TEXT("Template binding wins over authored resolver"), Snapshot.ActiveObjectives[0].Text.ToString(), FString(TEXT("Find Beta")));
	TestEqual(TEXT("Template count override applies before first projection"), Snapshot.ActiveObjectives[0].RequiredCount, 2);

	FEGQuestSaveEnvelope Save;
	TestTrue(TEXT("Role bindings save as stable handles"), Component->BuildQuestSaveData(TemplateRun, Save));
	TestEqual(TEXT("Save contains one template role"), Save.RunRecord.RoleBindings.Num(), 1);
	if (Save.RunRecord.RoleBindings.Num() == 1)
		TestEqual(TEXT("No live pointer is needed to identify the saved target"),
			Save.RunRecord.RoleBindings[0].Handles[0].StableId, FName(TEXT("Target.Beta")));

	UEGQuestTestPlatformSink* PlatformSink = NewObject<UEGQuestTestPlatformSink>();
	Component->OnQuestRoleLost.AddDynamic(PlatformSink, &UEGQuestTestPlatformSink::HandleRoleLost);
	Component->OnQuestTelemetry.AddDynamic(PlatformSink, &UEGQuestTestPlatformSink::HandleTelemetry);
	Beta->UnregisterComponent();
	Component->RefreshRoleBindings();
	TestEqual(TEXT("Notify loss policy emits exactly once"), PlatformSink->LostRoles.Num(), 1);
	TestEqual(TEXT("Notify loss policy identifies the role"), PlatformSink->LostRoles[0], FName(TEXT("Target")));
	TestTrue(TEXT("Role loss enters the structured telemetry funnel"),
		PlatformSink->TelemetryTypes.Contains(EEGQuestTelemetryEventType::RoleLost));
	Component->SetObjectiveRequiredCount(TemplateRun, Fixture.Objective->GetGUID(), 4);
	Component->FindQuestSnapshot(TemplateRun, Snapshot);
	TestEqual(TEXT("Unloaded target handle remains projected"), Snapshot.RoleMarkers.Num(), 1);
	if (Snapshot.RoleMarkers.Num() == 1)
	{
		TestEqual(TEXT("Unloaded marker preserves stable identity"), Snapshot.RoleMarkers[0].Handle.StableId, FName(TEXT("Target.Beta")));
		TestFalse(TEXT("Unloaded marker explicitly reports unresolved transform"), Snapshot.RoleMarkers[0].bResolved);
	}
	Alpha->UnregisterComponent();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGQuestPresentationAutomationTest,
	"QuestPlugin.Presentation.SemanticsMilestonesTrackingAndTelemetry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGQuestPresentationAutomationTest::RunTest(const FString& Parameters)
{
	UEGQuestNode_Objective* Objective = nullptr;
	UEGQuestGraph* Graph = BuildCountingObjectiveGraph(Objective);
	Graph->SetAutoTrackPolicy(EEGQuestAutoTrackPolicy::Always);
	Objective->SetNodeText(FText::FromString(TEXT("Collected {Count}/{RequiredCount}; {Remaining} left")));
	Objective->SetOptional(true);
	Objective->SetUITag(TEXT("Journal.Ritual"));
	Objective->SetSortOrder(7);

	const FGameplayTag NotifyTag = FGameplayTag::RequestGameplayTag(TEXT("DOP.Quest.Castle.BookAcquired"), false);
	UEGQuestEvent_GameplayNotify* MilestoneEvent = NewObject<UEGQuestEvent_GameplayNotify>(Objective);
	MilestoneEvent->SetNotifyTag(NotifyTag);
	MilestoneEvent->SetNotifyMagnitude(50.0f);
	FEGQuestObjectiveMilestone Milestone;
	Milestone.Threshold = 0.5f;
	Milestone.Events.Add(MilestoneEvent);
	Objective->SetMilestones({Milestone});
	UEGQuestEvent_GameplayNotify* SuccessEvent = NewObject<UEGQuestEvent_GameplayNotify>(Objective);
	SuccessEvent->SetNotifyTag(NotifyTag);
	SuccessEvent->SetNotifyMagnitude(100.0f);
	Objective->SetSuccessEvents({SuccessEvent});

	AGameStateBase* GameState = NewObject<AGameStateBase>();
	UEGQuestComponent* Component = NewObject<UEGQuestComponent>(GameState);
	UEGQuestTestGameplayNotifySink* NotifySink = NewObject<UEGQuestTestGameplayNotifySink>();
	UEGQuestTestPlatformSink* PlatformSink = NewObject<UEGQuestTestPlatformSink>();
	Component->OnQuestGameplayNotify.AddDynamic(NotifySink, &UEGQuestTestGameplayNotifySink::HandleNotify);
	Component->OnQuestTelemetry.AddDynamic(PlatformSink, &UEGQuestTestPlatformSink::HandleTelemetry);
	const FGuid RunId = Component->StartSharedQuest(Graph);
	FEGQuestViewSnapshot Snapshot;
	Component->FindQuestSnapshot(RunId, Snapshot);
	TestTrue(TEXT("Auto-track selects the new quest"), Snapshot.bTracked);
	TestEqual(TEXT("Semantic progress is count"), Snapshot.ActiveObjectives[0].Progress.Type, EEGQuestSemanticProgressType::Count);
	TestEqual(TEXT("Automatic arguments are formatted on first projection"), Snapshot.ActiveObjectives[0].Text.ToString(),
		FString(TEXT("Collected 0/3; 3 left")));
	TestTrue(TEXT("Optional flag projects"), Snapshot.ActiveObjectives[0].bOptional);
	TestEqual(TEXT("Opaque UI tag projects"), Snapshot.ActiveObjectives[0].UITag, FName(TEXT("Journal.Ritual")));
	UEGQuestNode_Objective* ReplacementObjective = nullptr;
	UEGQuestGraph* ReplacementGraph = BuildCountingObjectiveGraph(ReplacementObjective);
	ReplacementGraph->SetAutoTrackPolicy(EEGQuestAutoTrackPolicy::Always);
	const FGuid ReplacementRun = Component->StartSharedQuest(ReplacementGraph);
	FEGQuestViewSnapshot ReplacementSnapshot;
	Component->FindQuestSnapshot(RunId, Snapshot);
	Component->FindQuestSnapshot(ReplacementRun, ReplacementSnapshot);
	TestFalse(TEXT("Always policy publishes the previous quest as untracked"), Snapshot.bTracked);
	TestTrue(TEXT("Always policy leaves exactly the replacement quest tracked"), ReplacementSnapshot.bTracked);

	FEGQuestGameplayEvent Progress = TagEvent(TEXT("DOP.Quest.Ritual.Completed"));
	Progress.Magnitude = 2.0f;
	Component->NotifyGameplayEvent(Progress);
	Component->FindQuestSnapshot(RunId, Snapshot);
	TestEqual(TEXT("Progress projection updates semantically"), Snapshot.ActiveObjectives[0].Progress.Count, 2);
	TestEqual(TEXT("Automatic arguments refresh with progress"), Snapshot.ActiveObjectives[0].Text.ToString(),
		FString(TEXT("Collected 2/3; 1 left")));
	TestEqual(TEXT("Milestone emits once after commit"), NotifySink->ReceivedMagnitudes.Num(), 1);
	if (!NotifySink->ReceivedMagnitudes.IsEmpty()) TestEqual(TEXT("Milestone magnitude"), NotifySink->ReceivedMagnitudes[0], 50.0f);

	Progress.Magnitude = 1.0f;
	Component->NotifyGameplayEvent(Progress);
	TestEqual(TEXT("Success feedback emits after milestone"), NotifySink->ReceivedMagnitudes.Num(), 2);
	TestTrue(TEXT("Telemetry includes start"), PlatformSink->TelemetryTypes.Contains(EEGQuestTelemetryEventType::Started));
	TestTrue(TEXT("Telemetry includes objective progress"), PlatformSink->TelemetryTypes.Contains(EEGQuestTelemetryEventType::ObjectiveProgress));
	TestTrue(TEXT("Telemetry includes completion"), PlatformSink->TelemetryTypes.Contains(EEGQuestTelemetryEventType::Completed));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGQuestDirectivesAutomationTest,
	"QuestPlugin.Directives.ActivateAndTeardownOnTerminal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGQuestDirectivesAutomationTest::RunTest(const FString& Parameters)
{
	UEGQuestNode_Objective* Objective = nullptr;
	UEGQuestGraph* Graph = BuildCountingObjectiveGraph(Objective);
	UEGQuestNode_Stage* Stage = CastChecked<UEGQuestNode_Stage>(Graph->GetNodes()[0]);
	FEGQuestDirective Activate;
	Activate.DirectiveTag = FGameplayTag::RequestGameplayTag(TEXT("DOP.Quest.Castle.GateOpened"), false);
	Activate.RoleNames = {TEXT("Target")};
	Activate.Magnitude = 2.0f;
	FEGQuestDirective ExplicitDeactivate;
	ExplicitDeactivate.DirectiveTag = FGameplayTag::RequestGameplayTag(TEXT("DOP.Quest.Castle.BookAcquired"), false);
	Stage->SetActivateDirectives({Activate});
	Stage->SetDeactivateDirectives({ExplicitDeactivate});
	Stage->SetAutoRevertDirectives(true);

	AGameStateBase* GameState = NewObject<AGameStateBase>();
	UEGQuestComponent* Component = NewObject<UEGQuestComponent>(GameState);
	UEGQuestTestPlatformSink* Sink = NewObject<UEGQuestTestPlatformSink>();
	Component->OnQuestDirective.AddDynamic(Sink, &UEGQuestTestPlatformSink::HandleDirective);
	const FGuid RunId = Component->StartSharedQuest(Graph);
	TestEqual(TEXT("Stage activation emits after initial projection"), Sink->Directives.Num(), 1);
	if (!Sink->DirectivePhases.IsEmpty()) TestEqual(TEXT("Initial phase is activate"), Sink->DirectivePhases[0], EEGQuestDirectivePhase::Activate);
	FEGQuestSaveEnvelope Save;
	TestTrue(TEXT("Active directive quest can be saved"), Component->BuildQuestSaveData(RunId, Save));
	UEGQuestComponent* RestoredComponent = NewObject<UEGQuestComponent>(GameState);
	UEGQuestTestPlatformSink* RestoredSink = NewObject<UEGQuestTestPlatformSink>();
	RestoredComponent->OnQuestDirective.AddDynamic(RestoredSink, &UEGQuestTestPlatformSink::HandleDirective);
	TestTrue(TEXT("Directive quest resumes"), RestoredComponent->ResumeQuest(Save).IsSuccess());
	TestEqual(TEXT("Resume re-emits active-stage directives for idempotent consumers"), RestoredSink->Directives.Num(), 1);
	if (!RestoredSink->DirectivePhases.IsEmpty())
		TestEqual(TEXT("Resumed stage directive is activation"), RestoredSink->DirectivePhases[0], EEGQuestDirectivePhase::Activate);
	FEGQuestGameplayEvent Complete = TagEvent(TEXT("DOP.Quest.Ritual.Completed"));
	Complete.Magnitude = 3.0f;
	Component->NotifyGameplayEvent(Complete);
	TestEqual(TEXT("Terminal exit emits explicit teardown plus auto-revert"), Sink->Directives.Num(), 3);
	if (Sink->DirectivePhases.Num() == 3)
	{
		TestEqual(TEXT("Explicit teardown phase"), Sink->DirectivePhases[1], EEGQuestDirectivePhase::Deactivate);
		TestEqual(TEXT("Auto-revert phase"), Sink->DirectivePhases[2], EEGQuestDirectivePhase::Deactivate);
	}
	return true;
}

#endif
