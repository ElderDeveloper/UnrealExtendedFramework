#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UnrealExtendedQuest/EGQuestGraph.h"
#include "UnrealExtendedQuest/Nodes/EGQuestNode_End.h"
#include "UnrealExtendedQuest/Nodes/EGQuestNode_Objective.h"
#include "UnrealExtendedQuest/Nodes/EGQuestNode_Stage.h"
#include "UnrealExtendedQuest/Nodes/EGQuestNode_Start.h"
#include "UnrealExtendedQuest/EGQuestDiagnostics.h"
#include "UnrealExtendedQuestEditor/Editor/EGQuestCompiler.h"
#include "UnrealExtendedQuestEditor/Editor/Graph/EGQuestEdGraphSchema.h"
#include "UnrealExtendedQuestEditor/Editor/Graph/SchemaActions/EGQuestNewNode_GraphSchemaAction.h"
#include "UnrealExtendedQuestEditor/Editor/Nodes/EGQuestGraphNode.h"
#include "UnrealExtendedQuestEditor/Editor/Nodes/EGQuestGraphNode_Root.h"
#include "UnrealExtendedQuestEditor/Factories/EGQuestAssetFactory.h"
#include "UnrealExtendedQuestEditor/Commandlets/EGQuestValidateCommandlet.h"
#include "EdGraph/EdGraph.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGQuestEditorAuthoringAutomationTest,
	"QuestPlugin.Editor.AssetFactoryGraphAuthoringCompileAndDuplicate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGQuestEditorAuthoringAutomationTest::RunTest(const FString& Parameters)
{
	UEGQuestAssetFactory* Factory = NewObject<UEGQuestAssetFactory>();
	UEGQuestGraph* Quest = Cast<UEGQuestGraph>(Factory->FactoryCreateNew(
		UEGQuestGraph::StaticClass(), GetTransientPackage(), TEXT("QuestEditorAutomation"), RF_Transient | RF_Transactional, nullptr, GWarn));
	TestNotNull(TEXT("Quest factory creates an asset"), Quest);
	if (!Quest) return false;
	Quest->SetDefinitionId(TEXT("Tests.AssetFactoryAuthoring"));

	UEdGraph* Graph = Quest->GetGraph();
	TestNotNull(TEXT("Factory creates the copied editor graph"), Graph);
	const UEGQuestEdGraphSchema* Schema = Graph ? Cast<UEGQuestEdGraphSchema>(Graph->GetSchema()) : nullptr;
	TestNotNull(TEXT("Quest graph uses the adapted schema"), Schema);
	if (!Graph || !Schema) return false;
	// Build the transient graph as one authoring transaction. Intermediate shapes are deliberately
	// incomplete and their diagnostics are tested through the canonical validator below, not logs.
	Quest->DisableCompileQuest();

	UEGQuestGraphNode_Root* Root = nullptr;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (UEGQuestGraphNode_Root* Candidate = Cast<UEGQuestGraphNode_Root>(Node)) { Root = Candidate; break; }
	}
	TestNotNull(TEXT("New graph contains a root node"), Root);
	if (!Root) return false;

	UEGQuestGraphNode* Stage = FEGQuestNewNode_GraphSchemaAction::SpawnGraphNodeWithQuestNodeFromTemplate<UEGQuestGraphNode>(
		Graph, UEGQuestNode_Stage::StaticClass(), FNYVector2f(350.f, 0.f), false);
	UEGQuestGraphNode* End = FEGQuestNewNode_GraphSchemaAction::SpawnGraphNodeWithQuestNodeFromTemplate<UEGQuestGraphNode>(
		Graph, UEGQuestNode_End::StaticClass(), FNYVector2f(700.f, 0.f), false);
	TestNotNull(TEXT("Stage palette action creates a node"), Stage);
	TestNotNull(TEXT("End palette action creates a node"), End);
	if (!Stage || !End) return false;

	UEGQuestNode_Objective* Objective = Stage->AddNewObjectiveInteractive();
	TestNotNull(TEXT("Stage creates an objective row"), Objective);
	if (!Objective) return false;
	Objective->SetNodeText(FText::FromString(TEXT("Automation objective")));
	TestTrue(TEXT("Schema connects root to stage"), Schema->TryCreateConnection(Root->GetOutputPin(), Stage->GetInputPin()));
	UEdGraphPin* SuccessPin = Stage->FindObjectivePin(*Objective, EEGQuestArrowOutcome::Success);
	TestNotNull(TEXT("Objective row has a success pin"), SuccessPin);
	if (!SuccessPin) return false;
	TestTrue(TEXT("Schema connects objective row to end"), Schema->TryCreateConnection(SuccessPin, End->GetInputPin()));
	Quest->EnableCompileQuest();
	Quest->CompileQuestNodesFromGraphNodes();
	TestEqual(TEXT("Compile emits stage, objective and end runtime nodes"), Quest->GetNodes().Num(), 3);
	TestEqual(TEXT("Compile emits one start node"), Quest->GetStartNodes().Num(), 1);
	TestTrue(TEXT("Compiled stage has a stable GUID"), Quest->GetNodes()[0]->GetGUID().IsValid());

	UEGQuestGraph* Duplicate = DuplicateObject<UEGQuestGraph>(Quest, GetTransientPackage(), TEXT("QuestEditorAutomationDuplicate"));
	TestNotNull(TEXT("Quest asset duplicates with editor data"), Duplicate);
	if (Duplicate)
	{
		TestNotNull(TEXT("Duplicated asset retains editor graph"), Duplicate->GetGraph());
		TestEqual(TEXT("Duplicated asset retains compiled nodes"), Duplicate->GetNodes().Num(), Quest->GetNodes().Num());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGQuestExplicitRoutePriorityAutomationTest,
	"QuestPlugin.Editor.ExplicitRoutePriorityIgnoresCanvasLayout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGQuestExplicitRoutePriorityAutomationTest::RunTest(const FString& Parameters)
{
	UEGQuestAssetFactory* Factory = NewObject<UEGQuestAssetFactory>();
	UEGQuestGraph* Quest = Cast<UEGQuestGraph>(Factory->FactoryCreateNew(
		UEGQuestGraph::StaticClass(), GetTransientPackage(), TEXT("QuestRoutePriorityAutomation"),
		RF_Transient | RF_Transactional, nullptr, GWarn));
	TestNotNull(TEXT("Route-priority test quest exists"), Quest);
	if (!Quest) return false;
	Quest->SetDefinitionId(TEXT("Tests.RoutePriority"));

	UEdGraph* Graph = Quest->GetGraph();
	const UEGQuestEdGraphSchema* Schema = Cast<UEGQuestEdGraphSchema>(Graph->GetSchema());
	Quest->DisableCompileQuest();
	UEGQuestGraphNode_Root* Root = nullptr;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (UEGQuestGraphNode_Root* Candidate = Cast<UEGQuestGraphNode_Root>(Node))
		{
			Root = Candidate;
			break;
		}
	}
	TestNotNull(TEXT("Route-priority test has a root"), Root);
	if (!Root || !Schema) return false;

	UEGQuestGraphNode* Stage = FEGQuestNewNode_GraphSchemaAction::SpawnGraphNodeWithQuestNodeFromTemplate<UEGQuestGraphNode>(
		Graph, UEGQuestNode_Stage::StaticClass(), FNYVector2f(300.f, 0.f), false);
	UEGQuestGraphNode* EndA = FEGQuestNewNode_GraphSchemaAction::SpawnGraphNodeWithQuestNodeFromTemplate<UEGQuestGraphNode>(
		Graph, UEGQuestNode_End::StaticClass(), FNYVector2f(650.f, -100.f), false);
	UEGQuestGraphNode* EndB = FEGQuestNewNode_GraphSchemaAction::SpawnGraphNodeWithQuestNodeFromTemplate<UEGQuestGraphNode>(
		Graph, UEGQuestNode_End::StaticClass(), FNYVector2f(650.f, 100.f), false);
	TestNotNull(TEXT("Stage created"), Stage);
	TestNotNull(TEXT("First end created"), EndA);
	TestNotNull(TEXT("Second end created"), EndB);
	if (!Stage || !EndA || !EndB) return false;

	UEGQuestNode_Objective* Objective = Stage->AddNewObjectiveInteractive();
	TestNotNull(TEXT("Stage objective created"), Objective);
	if (!Objective) return false;
	Objective->SetNodeText(FText::FromString(TEXT("Choose a route")));

	TestTrue(TEXT("Root connects to the stage"), Schema->TryCreateConnection(Root->GetOutputPin(), Stage->GetInputPin()));
	UEdGraphPin* SuccessPin = Stage->FindObjectivePin(*Objective, EEGQuestArrowOutcome::Success);
	TestNotNull(TEXT("Objective success pin exists"), SuccessPin);
	if (!SuccessPin) return false;
	TestTrue(TEXT("Objective connects to first end"), Schema->TryCreateConnection(SuccessPin, EndA->GetInputPin()));
	TestTrue(TEXT("Objective connects to second end"), Schema->TryCreateConnection(SuccessPin, EndB->GetInputPin()));
	Quest->EnableCompileQuest();
	Quest->CompileQuestNodesFromGraphNodes();

	const FGuid EndAGuid = EndA->GetQuestNode().GetGUID();
	const FGuid EndBGuid = EndB->GetQuestNode().GetGUID();
	TestTrue(TEXT("First route priority is editable"),
		Objective->SetRoutePriority(EndAGuid, EEGQuestArrowOutcome::Success, 20));
	TestTrue(TEXT("Second route priority is editable"),
		Objective->SetRoutePriority(EndBGuid, EEGQuestArrowOutcome::Success, 5));
	Quest->CompileQuestNodesFromGraphNodes();

	auto AssertEndBWins = [this, Quest, Objective, EndBGuid](const TCHAR* What)
	{
		const TArray<FEGQuestEdge>& Edges = Objective->GetNodeChildren();
		TestEqual(FString::Printf(TEXT("%s emits both routes"), What), Edges.Num(), 2);
		if (Edges.Num() == 2)
		{
			TestEqual(FString::Printf(TEXT("%s keeps the lower priority first"), What),
				Edges[0].TargetIndex, Quest->GetNodeIndexForGUID(EndBGuid));
		}
	};
	AssertEndBWins(TEXT("Before moving cards"));

	// Reverse their visible ordering. Runtime arbitration must stay byte-for-byte authored.
	EndA->NodePosX = 900;
	EndA->NodePosY = 200;
	EndB->NodePosX = 450;
	EndB->NodePosY = -200;
	Quest->CompileQuestNodesFromGraphNodes();
	AssertEndBWins(TEXT("After moving cards"));

	// The same canonical rule list must catch bad authored priority and identity data without
	// compiling or mutating the asset.
	Objective->SetRoutePriority(EndAGuid, EEGQuestArrowOutcome::Success, 5);
	Quest->SetDefinitionId(TEXT("MalformedDefinitionId"));
	FEGQuestDiagnostics Diagnostics;
	FEGQuestCompilerContext::ValidateQuest(*Quest, Diagnostics);
	TestTrue(TEXT("Duplicate priorities are a structured error"), Diagnostics.Items.ContainsByPredicate([](const FEGQuestDiagnostic& Item)
	{
		return Item.RuleId == FName(EGQuestDiagnosticRule::DuplicateRoutePriority) &&
			Item.Severity == EEGQuestDiagnosticSeverity::Error;
	}));
	TestTrue(TEXT("Malformed definition ids are a structured error"), Diagnostics.Items.ContainsByPredicate([](const FEGQuestDiagnostic& Item)
	{
		return Item.RuleId == FName(EGQuestDiagnosticRule::MalformedDefinitionId) &&
			Item.Severity == EEGQuestDiagnosticSeverity::Error;
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEGQuestCoverageAutomationTest,
	"QuestPlugin.Editor.CoverageCommandletReachability",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEGQuestCoverageAutomationTest::RunTest(const FString& Parameters)
{
	UEGQuestGraph* Graph = NewObject<UEGQuestGraph>();
	UEGQuestNode_Start* Start = Graph->ConstructQuestNode<UEGQuestNode_Start>();
	UEGQuestNode_Stage* Stage = Graph->ConstructQuestNode<UEGQuestNode_Stage>();
	UEGQuestNode_Objective* Objective = Graph->ConstructQuestNode<UEGQuestNode_Objective>();
	UEGQuestNode_End* End = Graph->ConstructQuestNode<UEGQuestNode_End>();
	for (UEGQuestNode* Node : TArray<UEGQuestNode*>{Start, Stage, Objective, End}) Node->RegenerateGUID();
	Graph->SetNodes({Stage, Objective, End});
	Graph->SetStartNodes({Start});
	Start->SetNodeChildren({FEGQuestEdge(0)});
	Stage->SetNodeChildren({FEGQuestEdge(1)});
	Objective->SetNodeChildren({FEGQuestEdge(2)});
	UEGQuestCoverageCommandlet* Coverage = NewObject<UEGQuestCoverageCommandlet>();
	TArray<FString> Errors;
	TestTrue(TEXT("Reachable objective and End pass coverage"), Coverage->CheckCoverage(*Graph, Errors));
	UEGQuestNode_Objective* Orphan = Graph->ConstructQuestNode<UEGQuestNode_Objective>();
	Orphan->RegenerateGUID();
	Graph->SetNodes({Stage, Objective, End, Orphan});
	Errors.Reset();
	TestFalse(TEXT("Unreachable objective fails coverage"), Coverage->CheckCoverage(*Graph, Errors));
	TestTrue(TEXT("Coverage reports the orphan"), Errors.ContainsByPredicate([](const FString& Error)
	{
		return Error.Contains(TEXT("unreachable objective"));
	}));
	UEGQuestNode_Stage* DeadEnd = Graph->ConstructQuestNode<UEGQuestNode_Stage>();
	DeadEnd->RegenerateGUID();
	Graph->SetNodes({Stage, Objective, End, Orphan, DeadEnd});
	Stage->SetNodeChildren({FEGQuestEdge(1), FEGQuestEdge(4)});
	Errors.Reset();
	TestFalse(TEXT("A reachable branch with no route to End fails coverage"), Coverage->CheckCoverage(*Graph, Errors));
	TestTrue(TEXT("Coverage reports the reachable dead end"), Errors.ContainsByPredicate([](const FString& Error)
	{
		return Error.Contains(TEXT("reachable dead end"));
	}));
	return true;
}

#endif
