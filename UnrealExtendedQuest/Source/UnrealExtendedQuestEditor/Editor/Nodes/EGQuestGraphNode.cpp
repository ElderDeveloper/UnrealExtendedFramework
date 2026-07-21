// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#include "EGQuestGraphNode.h"

#include "Editor/EditorEngine.h"
#include "Framework/Commands/GenericCommands.h"
#include "EdGraph/EdGraphNode.h"
#include "Engine/Font.h"
#include "Logging/TokenizedMessage.h"
#include "ScopedTransaction.h"
#include "Runtime/Launch/Resources/Version.h"

#if NY_ENGINE_VERSION >= 424
#include "ToolMenu.h"
#endif

#include "UnrealExtendedQuestEditor/EGQuestPluginEditorModule.h"
#include "UnrealExtendedQuest/EGQuestDiagnostics.h"
#include "UnrealExtendedQuest/EGQuestGraph.h"
#include "UnrealExtendedQuest/EGQuestPluginSettings.h"
#include "UnrealExtendedQuest/Nodes/EGQuestNode_Custom.h"

#define LOCTEXT_NAMESPACE "QuestGraphNode"

namespace
{
	// Objective pin names: outcome prefix + the objective's GUID, so a pin survives row reorders and
	// node rebuilds for as long as its objective exists.
	const TCHAR* SuccessPinPrefix = TEXT("Success_");
	const TCHAR* FailPinPrefix = TEXT("Fail_");

	// Rules this node raises that EGQuestDiagnosticRule does not name yet. Spelt exactly as they
	// would be there so they can be lifted into that namespace verbatim; a rule id is stable API, so
	// naming one here that the canonical list will contradict later is the thing to avoid.
	const TCHAR* InvalidTextArgumentRule = TEXT("Quest.Node.InvalidTextArgument");
	const TCHAR* UnsatisfiableJoinRule = TEXT("Quest.Node.UnsatisfiableJoin");
	const TCHAR* ContradictoryJoinRule = TEXT("Quest.Node.ContradictoryJoin");

	// Everything below is a Warning because everything below has always been one. Escalating any of
	// them to Error would fail IsDataValid - and therefore CI - on assets that ship green today, which
	// is a call for whoever wants the escalation to make deliberately, not a side effect of this
	// conversion.
	constexpr EEGQuestDiagnosticSeverity NodeSeverity = EEGQuestDiagnosticSeverity::Warning;

	EMessageSeverity::Type ToMessageSeverity(EEGQuestDiagnosticSeverity Severity)
	{
		switch (Severity)
		{
			case EEGQuestDiagnosticSeverity::Error: return EMessageSeverity::Error;
			case EEGQuestDiagnosticSeverity::Warning: return EMessageSeverity::Warning;
			default: return EMessageSeverity::Info;
		}
	}

	void CollectEventDiagnostics(const TArray<TObjectPtr<UEGQuestEventCustom>>& Events, const FGuid& ElementGuid, FEGQuestDiagnostics& Out)
	{
		for (int32 Index = 0; Index < Events.Num(); ++Index)
		{
			if (!IsValid(Events[Index]))
			{
				Out.Add(
					FName(EGQuestDiagnosticRule::InvalidEnterEvent), NodeSeverity, ElementGuid,
					FText::Format(LOCTEXT("EventNoObject", "Event {0} has no event object"), Index),
					LOCTEXT("EventNoObjectFix", "Pick an event class on the row, or remove the row."));
				continue;
			}

			if (FString EventError; !Events[Index]->ValidateForCompile(EventError))
			{
				Out.Add(
					FName(EGQuestDiagnosticRule::InvalidEnterEvent), NodeSeverity, ElementGuid,
					FText::Format(LOCTEXT("EventInvalid", "Event {0} {1}"), Index, FText::FromString(EventError)));
			}
		}
	}

	// The check is on the CustomTextArgument pointer, which is why this stays a function over the
	// owner's array rather than a virtual on the argument: there is no object to call a virtual on.
	void CollectTextArgumentDiagnostics(
		const TArray<FEGQuestTextArgument>& Arguments, const FGuid& ElementGuid, const FText& Prefix, FEGQuestDiagnostics& Out)
	{
		for (int32 Index = 0; Index < Arguments.Num(); ++Index)
		{
			if (IsValid(Arguments[Index].CustomTextArgument))
			{
				continue;
			}

			Out.Add(
				FName(InvalidTextArgumentRule), NodeSeverity, ElementGuid,
				FText::Format(LOCTEXT("ArgumentNoObject", "{0}Text argument {1} has no custom argument object"), Prefix, Index),
				LOCTEXT("ArgumentNoObjectFix", "Pick a custom text argument class, or remove the argument row."));
		}
	}

	void CollectObjectiveDiagnostics(const UEGQuestGraphNode& Node, FEGQuestDiagnostics& Out)
	{
		const TArray<TObjectPtr<UEGQuestNode_Objective>>& Objectives = Node.GetObjectives();
		for (int32 Row = 0; Row < Objectives.Num(); ++Row)
		{
			const UEGQuestNode_Objective* Objective = Objectives[Row];
			if (!IsValid(Objective))
			{
				// Anchored to the card: an empty row has no objective, so it has no GUID of its own.
				Out.Add(
					FName(EGQuestDiagnosticRule::InvalidObjective), NodeSeverity, Node.GetQuestNode().GetGUID(),
					FText::Format(LOCTEXT("ObjectiveEmpty", "Objective row {0} is empty"), Row + 1),
					LOCTEXT("ObjectiveEmptyFix", "Pick an objective class on the row, or remove the row."));
				continue;
			}

			const FGuid ObjectiveGuid = Objective->GetGUID();
			const FText RowPrefix = FText::Format(LOCTEXT("ObjectiveRowPrefix", "Objective row {0}: "), Row + 1);

			CollectTextArgumentDiagnostics(Objective->GetTextArguments(), ObjectiveGuid, RowPrefix, Out);

			if (FString ObjectiveError; !Objective->ValidateForCompile(ObjectiveError))
			{
				Out.Add(
					FName(EGQuestDiagnosticRule::InvalidObjective), NodeSeverity, ObjectiveGuid,
					FText::Format(LOCTEXT("ObjectiveInvalid", "{0}{1}"), RowPrefix, FText::FromString(ObjectiveError)));
			}

			if (!Objective->CanEverFail())
			{
				continue;
			}

			// A failed objective with nowhere to route can never succeed either, so every destination
			// waiting on its success is unreachable and the quest hangs silently.
			const UEdGraphPin* FailPin = Node.FindObjectivePin(*Objective, EEGQuestArrowOutcome::Fail);
			if (!FailPin || FailPin->LinkedTo.Num() == 0)
			{
				Out.Add(
					FName(EGQuestDiagnosticRule::FailWithoutRoute), NodeSeverity, ObjectiveGuid,
					FText::Format(
						LOCTEXT("ObjectiveFailWithoutRoute", "{0}can fail but its fail pin goes nowhere, so failing it would hang the quest with no way out of the stage"),
						RowPrefix),
					LOCTEXT("ObjectiveFailWithoutRouteFix", "Wire the fail pin somewhere, or remove the objective's failure tracker."));
			}
		}
	}

	// A destination is a join: it fires only when every arrow into it is satisfied, and an arrow is
	// only ever satisfied by an objective of the stage that is active.
	void CollectJoinDiagnostics(const UEGQuestGraphNode& Node, FEGQuestDiagnostics& Out)
	{
		const FGuid NodeGuid = Node.GetQuestNode().GetGUID();
		const UEdGraphPin* InputPin = Node.GetInputPin();

		const UEGQuestGraphNode* CommonSourceStage = nullptr;
		for (const UEdGraphPin* IncomingPin : InputPin->LinkedTo)
		{
			const UEGQuestGraphNode* SourceNode = Cast<UEGQuestGraphNode>(IncomingPin->GetOwningNodeUnchecked());
			if (!SourceNode || SourceNode->IsRootNode() || !SourceNode->IsStageNode())
			{
				continue;
			}

			if (CommonSourceStage != nullptr && SourceNode != CommonSourceStage)
			{
				// Reported once: naming every further stage would not tell the author anything the
				// first pair does not, and the fix is the same wire either way.
				Out.Add(
					FName(UnsatisfiableJoinRule), NodeSeverity, NodeGuid,
					LOCTEXT("JoinAcrossStages", "Incoming arrows come from objectives of different stages, so they can never all be satisfied and this node can never fire"),
					LOCTEXT("JoinAcrossStagesFix", "Route through one stage at a time: break the arrows that come from the other stage."));
				return;
			}
			CommonSourceStage = SourceNode;
		}

		if (!CommonSourceStage)
		{
			return;
		}

		// One objective reaches one outcome, so needing both of its outcomes is unsatisfiable.
		for (const UEGQuestNode_Objective* Objective : CommonSourceStage->GetObjectives())
		{
			if (!IsValid(Objective))
			{
				continue;
			}

			const UEdGraphPin* SuccessPin = CommonSourceStage->FindObjectivePin(*Objective, EEGQuestArrowOutcome::Success);
			const UEdGraphPin* FailPin = CommonSourceStage->FindObjectivePin(*Objective, EEGQuestArrowOutcome::Fail);
			const bool bSuccessHere = SuccessPin && SuccessPin->LinkedTo.Contains(InputPin);
			const bool bFailHere = FailPin && FailPin->LinkedTo.Contains(InputPin);
			if (bSuccessHere && bFailHere)
			{
				Out.Add(
					FName(ContradictoryJoinRule), NodeSeverity, NodeGuid,
					LOCTEXT("JoinBothOutcomes", "Both the Success and Fail arrow of one objective point here, so this node waits for that objective to have both outcomes and can never fire"),
					LOCTEXT("JoinBothOutcomesFix", "Break one of the two arrows."));
			}
		}
	}

	void CollectNodeDiagnostics(const UEGQuestGraphNode& Node, FEGQuestDiagnostics& Out)
	{
		const UEGQuestNode& QuestNode = Node.GetQuestNode();
		const FGuid NodeGuid = QuestNode.GetGUID();

		CollectEventDiagnostics(QuestNode.GetNodeEnterEvents(), NodeGuid, Out);
		CollectTextArgumentDiagnostics(QuestNode.GetTextArguments(), NodeGuid, FText::GetEmpty(), Out);

		// Only provable breakage is reported. Quest design is the designer's job: the system provides
		// rules, not guard rails - so an odd-looking but workable graph compiles clean.
		if (Node.IsStageNode())
		{
			CollectObjectiveDiagnostics(Node, Out);
		}

		if (Node.HasInputPin() && (Node.IsStageNode() || Node.IsEndNode()))
		{
			CollectJoinDiagnostics(Node, Out);
		}

		if (Node.HasInputPin() && Node.GetInputPin()->LinkedTo.Num() == 0 && !Node.CanBeOrphan())
		{
			Out.Add(
				FName(EGQuestDiagnosticRule::OrphanNode), NodeSeverity, NodeGuid,
				LOCTEXT("Orphan", "Node has no input connections (orphan). It will not be accessible from anywhere"),
				LOCTEXT("OrphanFix", "Wire something into its input pin, or delete it."));
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Begin UObject interface
void UEGQuestGraphNode::PostLoad()
{
	Super::PostLoad();

	// Fixup back pointers and ownership. Assets touched by editor builds that predated the
	// details-panel outer fix can have rows saved under this card instead of the quest; the rename
	// flags keep the heal from dirtying packages or transacting mid-load.
	ResetQuestNodeOwner(REN_DoNotDirty | REN_NonTransactional);
}

void UEGQuestGraphNode::PostEditImport()
{
	RegisterListeners();

	// Make sure this QuestNode is owned by the Quest it's being pasted into.
	// The paste can come from another Quest
	ResetQuestNodeOwner();

	// A pasted card is a new stage with new objectives: duplicate GUIDs would collide with the
	// originals in history, snapshots and pin names.
	if (QuestNode)
	{
		QuestNode->RegenerateGUID();
	}
	for (UEGQuestNode_Objective* Objective : Objectives)
	{
		if (!Objective)
		{
			continue;
		}

		// Rename the copied outcome pins in place to the fresh GUID: pin identity is the name, so
		// this is what lets wires between two cards pasted together survive the rebuild below.
		UEdGraphPin* CopiedSuccessPin = FindObjectivePin(*Objective, EEGQuestArrowOutcome::Success);
		UEdGraphPin* CopiedFailPin = FindObjectivePin(*Objective, EEGQuestArrowOutcome::Fail);
		Objective->RegenerateGUID();
		if (CopiedSuccessPin)
		{
			CopiedSuccessPin->PinName = MakeObjectivePinName(*Objective, EEGQuestArrowOutcome::Success);
		}
		if (CopiedFailPin)
		{
			CopiedFailPin->PinName = MakeObjectivePinName(*Objective, EEGQuestArrowOutcome::Fail);
		}
	}
	ReconstructNode();
}

void UEGQuestGraphNode::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (!PropertyChangedEvent.Property)
	{
		return;
	}

	// Objective rows changed (added/removed/reordered through the details panel): the pins mirror
	// the rows, so rebuild them and recompile.
	if (PropertyChangedEvent.GetPropertyName() == GetMemberNameObjectives() ||
		(PropertyChangedEvent.MemberProperty && PropertyChangedEvent.MemberProperty->GetFName() == GetMemberNameObjectives()))
	{
		for (UEGQuestNode_Objective* Objective : Objectives)
		{
			if (Objective && !Objective->HasGUID())
			{
				Objective->RegenerateGUID();
			}
		}
		// The details panel outers a newly added/duplicated row to this card, not to the quest;
		// hand it over before compiling or GetQuest() on the row asserts.
		ResetQuestNodeOwner();
		RebuildPinsAndCompile();
		return;
	}

	CheckAll();
	ApplyCompilerWarnings();
}

void UEGQuestGraphNode::PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);

	// A row's fail routing appearing or disappearing (its failure tracker) adds or removes its fail pin.
	if (PropertyChangedEvent.PropertyChain.GetActiveMemberNode() &&
		PropertyChangedEvent.PropertyChain.GetActiveMemberNode()->GetValue() &&
		PropertyChangedEvent.PropertyChain.GetActiveMemberNode()->GetValue()->GetFName() == GetMemberNameObjectives())
	{
		RebuildPinsAndCompile();
	}
}

bool UEGQuestGraphNode::Modify(bool bAlwaysMarkDirty)
{
	if (!CanModify())
	{
		return false;
	}

	bool bWasModified = Super::Modify(bAlwaysMarkDirty);

	if (QuestNode)
	{
		bWasModified = bWasModified && QuestNode->Modify(bAlwaysMarkDirty);
	}
	for (UEGQuestNode_Objective* Objective : Objectives)
	{
		if (Objective)
		{
			bWasModified = bWasModified && Objective->Modify(bAlwaysMarkDirty);
		}
	}

	return bWasModified;
}
// End UObject interface
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Begin UEdGraphNode interface
FText UEGQuestGraphNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (NodeIndex == INDEX_NONE)
	{
		return Super::GetNodeTitle(TitleType);
	}

	// An end pill is stamped with its result - that is all an end is.
	if (const UEGQuestNode_End* AsEnd = Cast<const UEGQuestNode_End>(QuestNode))
	{
		switch (AsEnd->GetQuestResult())
		{
			case EEGQuestResult::Completed: return LOCTEXT("EndCompleted", "Completed");
			case EEGQuestResult::Failed: return LOCTEXT("EndFailed", "Failed");
			case EEGQuestResult::Abandoned: return LOCTEXT("EndAbandoned", "Abandoned");
			default: break;
		}
	}

	// A stage card is titled by its journal headline.
	if (const UEGQuestNode_Stage* AsStage = Cast<const UEGQuestNode_Stage>(QuestNode))
	{
		if (!AsStage->GetTitle().IsEmpty())
		{
			return AsStage->GetTitle();
		}
	}

	if (const UEGQuestNode_Custom* AsCustom = Cast<const UEGQuestNode_Custom>(QuestNode))
	{
		FString TitleOverride;
		if (AsCustom->GetNodeTitleOverride(TitleOverride))
		{
			return FText::FromString(TitleOverride);
		}
	}

	return FText::FromString(QuestNode->GetNodeTypeString());
}

void UEGQuestGraphNode::PrepareForCopying()
{
	Super::PrepareForCopying();

	// Temporarily take ownership of the QuestNode and the objectives, so that they are not deleted when cutting
	if (QuestNode)
	{
		QuestNode->Rename(nullptr, this, REN_DontCreateRedirectors);
	}
	for (UEGQuestNode_Objective* Objective : Objectives)
	{
		if (Objective)
		{
			Objective->Rename(nullptr, this, REN_DontCreateRedirectors);
		}
	}
}

void UEGQuestGraphNode::PostCopyNode()
{
	Super::PostCopyNode();
	// Make sure the QuestNode goes back to being owned by the Quest after copying.
	ResetQuestNodeOwner();
}

FString UEGQuestGraphNode::GetDocumentationExcerptName() const
{
	return "";
}

FText UEGQuestGraphNode::GetTooltipText() const
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("NodeType"), QuestNode ? FText::FromString(QuestNode->GetNodeTypeString()) : FText::GetEmpty());
	Args.Add(TEXT("Description"), QuestNode ? FText::FromString(QuestNode->GetDesc()) : FText::GetEmpty());
	return FText::Format(LOCTEXT("GraphNodeTooltip", "Type = {NodeType}\n{Description}"), Args);
}

void UEGQuestGraphNode::AllocateDefaultPins()
{
	check(Pins.Num() == 0);

	// The root only leads out; an end only receives. A stage receives on the card and leads out
	// through its objective rows. Custom nodes keep the generic in/out pair.
	if (IsRootNode())
	{
		CreateOutputPin();
		return;
	}

	CreateInputPin();

	if (QuestNode == nullptr || IsCustomNode())
	{
		CreateOutputPin();
		return;
	}

	if (IsStageNode())
	{
		for (const UEGQuestNode_Objective* Objective : Objectives)
		{
			if (Objective)
			{
				CreateObjectivePins(*Objective);
			}
		}
	}

	// End nodes: input only.
}

void UEGQuestGraphNode::CreateObjectivePins(const UEGQuestNode_Objective& Objective)
{
	CreatePin(EGPD_Output, UEGQuestEdGraphSchema::PIN_CATEGORY_Success, MakeObjectivePinName(Objective, EEGQuestArrowOutcome::Success));
	if (Objective.CanEverFail())
	{
		CreatePin(EGPD_Output, UEGQuestEdGraphSchema::PIN_CATEGORY_Fail, MakeObjectivePinName(Objective, EEGQuestArrowOutcome::Fail));
	}
}

#if NY_ENGINE_VERSION >= 424
void UEGQuestGraphNode::GetNodeContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const
{
	// These actions (commands) are handled and registered in the FEGQuestEditor class
	if (Context->Node && !Context->bIsDebugging && !IsRootNode())
	{
		// Menu for right clicking on node
		FToolMenuSection& Section = Menu->AddSection("QuestGraphNode_BaseNodeEditCRUD");
		Section.AddMenuEntry(FGenericCommands::Get().Delete);
		Section.AddMenuEntry(FGenericCommands::Get().Cut);
		Section.AddMenuEntry(FGenericCommands::Get().Copy);
		Section.AddMenuEntry(FGenericCommands::Get().Paste);
		Section.AddMenuEntry(FGenericCommands::Get().Duplicate);
	}
}

#else

void UEGQuestGraphNode::GetContextMenuActions(const FGraphNodeContextMenuBuilder& Context) const
{
	// These actions (commands) are handled and registered in the FEGQuestEditor class
	if (Context.Node && !Context.bIsDebugging && !IsRootNode())
	{
		// Menu for right clicking on node
		Context.MenuBuilder->BeginSection("QuestGraphNode_BaseNodeEditCRUD");
		{
			Context.MenuBuilder->AddMenuEntry(FGenericCommands::Get().Delete);
			Context.MenuBuilder->AddMenuEntry(FGenericCommands::Get().Cut);
			Context.MenuBuilder->AddMenuEntry(FGenericCommands::Get().Copy);
			Context.MenuBuilder->AddMenuEntry(FGenericCommands::Get().Paste);
			Context.MenuBuilder->AddMenuEntry(FGenericCommands::Get().Duplicate);
		}
		Context.MenuBuilder->EndSection();
	}
}
#endif // NY_ENGINE_VERSION >= 424

void UEGQuestGraphNode::AutowireNewNode(UEdGraphPin* FromPin)
{
	// No context given, simply return
	if (FromPin == nullptr)
	{
		return;
	}

	// FromPin should not belong to this node but to the node that spawned this node.
	check(FromPin->GetOwningNode() != this);
	check(FromPin->Direction == EGPD_Output);

	if (!HasInputPin())
	{
		return;
	}

	UEdGraphPin* InputPin = GetInputPin();
	const UEGQuestEdGraphSchema* Schema = GetQuestEdGraphSchema();

	// auto-connect from dragged pin to first compatible pin on the new node
	if (Schema->TryCreateConnection(FromPin, InputPin))
	{
		FromPin->GetOwningNode()->NodeConnectionListChanged();
	}
}

bool UEGQuestGraphNode::CanHaveInputConnections() const
{
	if (const UEGQuestNode_Custom* AsCustom = Cast<const UEGQuestNode_Custom>(QuestNode))
	{
		return AsCustom->CanHaveInputConnections();
	}

	return NodeIndex != INDEX_NONE && !IsRootNode();
}

bool UEGQuestGraphNode::CanHaveOutputConnections() const
{
	if (const UEGQuestNode_Custom* AsCustom = Cast<const UEGQuestNode_Custom>(QuestNode))
	{
		return AsCustom->CanHaveOutputConnections();
	}

	return !IsEndNode();
}

// End UEdGraphNode interface
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Begin UEGQuestGraphNode_Base interface
FLinearColor UEGQuestGraphNode::GetNodeBackgroundColor() const
{
	if (NodeIndex == INDEX_NONE)
	{
		return FLinearColor::Black;
	}

	if (const UEGQuestNode_Custom* AsCustom = Cast<const UEGQuestNode_Custom>(QuestNode))
	{
		return AsCustom->GetNodeColor();
	}

	if (IsStageNode())
	{
		return GetDefault<UEGQuestPluginSettings>()->StageNodeColor;
	}

	// An end pill wears its result.
	if (const UEGQuestNode_End* AsEnd = Cast<const UEGQuestNode_End>(QuestNode))
	{
		switch (AsEnd->GetQuestResult())
		{
			case EEGQuestResult::Completed: return FLinearColor(0.03f, 0.33f, 0.18f);
			case EEGQuestResult::Failed: return FLinearColor(0.45f, 0.07f, 0.07f);
			case EEGQuestResult::Abandoned: return FLinearColor(0.25f, 0.25f, 0.25f);
			default: break;
		}
	}

	return FLinearColor::Black;
}

void UEGQuestGraphNode::RegisterListeners()
{
	Super::RegisterListeners();
	QuestNode->OnQuestNodePropertyChanged.AddUObject(this, &UEGQuestGraphNode::OnQuestNodePropertyChanged);
}
// End UEGQuestGraphNode_Base interface
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Begin own functions
bool UEGQuestGraphNode::CanBeOrphan() const
{
	if (IsRootNode())
	{
		return true;
	}

	if (const UEGQuestNode_Custom* AsCustom = Cast<const UEGQuestNode_Custom>(QuestNode))
	{
		return AsCustom->CanBeOrphan();
	}

	return false;
}

void UEGQuestGraphNode::SetQuestNodeDataChecked(int32 InIndex, UEGQuestNode* InNode)
{
	const UEGQuestGraph* Quest = GetQuest();
	checkf(Quest->GetNodes()[InIndex] == InNode, TEXT("The selected index = %d and with the Node provided does not match the Node from the Quest"), InIndex);

	SetQuestNodeIndex(InIndex);
	SetQuestNode(InNode);
}

void UEGQuestGraphNode::SetObjectives(const TArray<UEGQuestNode_Objective*>& InObjectives)
{
	Objectives.Empty(InObjectives.Num());
	for (UEGQuestNode_Objective* Objective : InObjectives)
	{
		Objectives.Add(Objective);
		if (Objective)
		{
			Objective->SetGraphNode(this);
			Objective->SetFlags(RF_Transactional);
		}
	}
}

UEGQuestNode_Objective* UEGQuestGraphNode::AddNewObjectiveInteractive()
{
	if (!IsStageNode())
	{
		return nullptr;
	}

	const FScopedTransaction Transaction(LOCTEXT("AddObjective", "Quest Editor: Add Objective"));
	UEGQuestGraph* Quest = GetQuest();
	Quest->Modify();
	Modify();

	UEGQuestNode_Objective* Objective = NewObject<UEGQuestNode_Objective>(Quest, UEGQuestNode_Objective::StaticClass(), NAME_None, RF_Transactional);
	Objective->RegenerateGUID();
	Objective->SetGraphNode(this);
	Objectives.Add(Objective);

	RebuildPinsAndCompile();
	return Objective;
}

void UEGQuestGraphNode::RemoveObjectiveInteractive(UEGQuestNode_Objective* Objective)
{
	if (!Objective || !Objectives.Contains(Objective))
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("RemoveObjective", "Quest Editor: Remove Objective"));
	GetQuest()->Modify();
	Modify();

	// Break the row's wires first so the destinations get notified.
	for (EEGQuestArrowOutcome Outcome : {EEGQuestArrowOutcome::Success, EEGQuestArrowOutcome::Fail})
	{
		if (UEdGraphPin* Pin = FindObjectivePin(*Objective, Outcome))
		{
			Pin->BreakAllPinLinks();
		}
	}
	Objectives.Remove(Objective);

	RebuildPinsAndCompile();
}

void UEGQuestGraphNode::RebuildPinsAndCompile()
{
	ReconstructNode();
	UEGQuestGraph* Quest = GetQuest();
	Quest->CompileQuestNodesFromGraphNodes();
	Quest->MarkPackageDirty();
	GetGraph()->NotifyGraphChanged();
}

FName UEGQuestGraphNode::MakeObjectivePinName(const UEGQuestNode_Objective& Objective, EEGQuestArrowOutcome Outcome)
{
	const TCHAR* Prefix = Outcome == EEGQuestArrowOutcome::Fail ? FailPinPrefix : SuccessPinPrefix;
	return FName(*(FString(Prefix) + Objective.GetGUID().ToString()));
}

UEdGraphPin* UEGQuestGraphNode::FindObjectivePin(const UEGQuestNode_Objective& Objective, EEGQuestArrowOutcome Outcome) const
{
	const FName PinName = MakeObjectivePinName(Objective, Outcome);
	for (UEdGraphPin* Pin : Pins)
	{
		if (Pin && Pin->Direction == EGPD_Output && Pin->PinName == PinName)
		{
			return Pin;
		}
	}
	return nullptr;
}

UEGQuestNode_Objective* UEGQuestGraphNode::FindObjectiveForPin(const UEdGraphPin& Pin, EEGQuestArrowOutcome& OutOutcome) const
{
	for (UEGQuestNode_Objective* Objective : Objectives)
	{
		if (!Objective)
		{
			continue;
		}
		if (Pin.PinName == MakeObjectivePinName(*Objective, EEGQuestArrowOutcome::Success))
		{
			OutOutcome = EEGQuestArrowOutcome::Success;
			return Objective;
		}
		if (Pin.PinName == MakeObjectivePinName(*Objective, EEGQuestArrowOutcome::Fail))
		{
			OutOutcome = EEGQuestArrowOutcome::Fail;
			return Objective;
		}
	}
	return nullptr;
}

void UEGQuestGraphNode::ApplyCompilerWarnings()
{
	FEGQuestDiagnostics NodeDiagnostics;
	CollectDiagnostics(NodeDiagnostics);
	ApplyDiagnostics(NodeDiagnostics);
}

void UEGQuestGraphNode::CollectDiagnostics(FEGQuestDiagnostics& OutDiagnostics) const
{
	CollectNodeDiagnostics(*this, OutDiagnostics);
}

void UEGQuestGraphNode::ApplyDiagnostics(const FEGQuestDiagnostics& InDiagnostics)
{
	ClearCompilerMessage();

	TSet<FGuid> OwnedGuids;
	OwnedGuids.Add(GetQuestNode().GetGUID());
	for (const UEGQuestNode_Objective* Objective : Objectives)
	{
		if (Objective)
		{
			OwnedGuids.Add(Objective->GetGUID());
		}
	}

	FEGQuestDiagnostics NodeDiagnostics;
	for (const FEGQuestDiagnostic& Diagnostic : InDiagnostics.Items)
	{
		if (OwnedGuids.Contains(Diagnostic.ElementGuid))
		{
			NodeDiagnostics.Items.Add(Diagnostic);
		}
	}
	if (NodeDiagnostics.Num() == 0)
	{
		return;
	}

	// Every finding, not the first: this used to stop at the first problem, so a card with three of
	// them took three compiles to clean - and the author had no way to know how many were left.
	TArray<FString> Lines;
	Lines.Reserve(NodeDiagnostics.Num());
	EMessageSeverity::Type BadgeSeverity = EMessageSeverity::Info;
	for (const FEGQuestDiagnostic& Diagnostic : NodeDiagnostics.Items)
	{
		FString Line = Diagnostic.Message.ToString();
		if (!Diagnostic.FixHint.IsEmpty())
		{
			Line += TEXT(" ") + Diagnostic.FixHint.ToString();
		}
		Lines.Add(MoveTemp(Line));

		// EMessageSeverity counts down from CriticalError, so the worst finding is the smallest one.
		BadgeSeverity = static_cast<EMessageSeverity::Type>(
			FMath::Min<int32>(BadgeSeverity, ToMessageSeverity(Diagnostic.Severity)));
	}

	SetCompilerWarningMessage(FString::Join(Lines, TEXT("\n")));

	// SetCompilerWarningMessage stamps Warning unconditionally - its signature carries no severity -
	// so the badge is corrected here rather than lying about an Error.
	ErrorType = BadgeSeverity;
}

int32 UEGQuestGraphNode::EstimateNodeWidth() const
{
	constexpr int32 EstimatedCharWidth = 6;
	// Check which is bigger, and use that
	const FString NodeTitle = GetNodeTitle(ENodeTitleType::FullTitle).ToString();
	const FString NodeText = QuestNode->GetNodeText().ToString();

	int32 Result;
	FString ResultFromString;
	if (NodeTitle.Len() > NodeText.Len())
	{
		Result = NodeTitle.Len() * EstimatedCharWidth;
		ResultFromString = NodeTitle;
	}
	else
	{
		Result = NodeText.Len() * EstimatedCharWidth;
		ResultFromString = NodeText;
	}

	if (const UFont* Font = GetDefault<UEditorEngine>()->EditorFont)
	{
		Result = Font->GetStringSize(*ResultFromString);
	}

	return Result;
}

void UEGQuestGraphNode::CheckQuestNodeIndexMatchesNode() const
{
#if DO_CHECK
	if (!IsRootNode())
	{
		const UEGQuestGraph* Quest = GetQuest();
		checkf(Quest->GetNodes()[NodeIndex] == QuestNode, TEXT("The NodeIndex = %d with QuestNode does not match the Node from the Quest at the same index"), NodeIndex);
	}
#endif
}

TArray<UEGQuestGraphNode*> UEGQuestGraphNode::GetParentNodes() const
{
	TArray<UEGQuestGraphNode*> ParentNodes;
	if (!HasInputPin())
	{
		return ParentNodes;
	}

	for (const UEdGraphPin* SourceOutputPin : GetInputPin()->LinkedTo)
	{
		if (UEGQuestGraphNode* ParentNode = Cast<UEGQuestGraphNode>(SourceOutputPin->GetOwningNodeUnchecked()))
		{
			ParentNodes.AddUnique(ParentNode);
		}
	}

	return ParentNodes;
}

TArray<UEGQuestGraphNode*> UEGQuestGraphNode::GetChildNodes() const
{
	TArray<UEGQuestGraphNode*> ChildNodes;
	for (const UEdGraphPin* OutputPin : GetOutputPins())
	{
		for (const UEdGraphPin* TargetInputPin : OutputPin->LinkedTo)
		{
			if (UEGQuestGraphNode* ChildNode = Cast<UEGQuestGraphNode>(TargetInputPin->GetOwningNodeUnchecked()))
			{
				ChildNodes.AddUnique(ChildNode);
			}
		}
	}

	return ChildNodes;
}

void UEGQuestGraphNode::OnQuestNodePropertyChanged(const FPropertyChangedEvent& PropertyChangedEvent, int32 EdgeIndexChanged)
{
	if (!PropertyChangedEvent.Property)
	{
		return;
	}

	// The runtime edges are compiler output now; a quest node property change only needs a redraw
	// (titles, descriptions and end results paint on the card).
	if (UEdGraph* Graph = GetGraph())
	{
		Graph->NotifyGraphChanged();
	}
}

void UEGQuestGraphNode::ResetQuestNodeOwner(ERenameFlags ExtraRenameFlags)
{
	UEGQuestGraph* Quest = GetQuest();

	auto EnsureOwnedByQuest = [Quest, ExtraRenameFlags, this](UEGQuestNode* Node)
	{
		if (!Node)
		{
			return;
		}
		if (Node->GetOuter() != Quest)
		{
			Node->Rename(nullptr, Quest, REN_DontCreateRedirectors | ExtraRenameFlags);
		}
		Node->SetGraphNode(this);
		Node->SetFlags(RF_Transactional);
	};

	EnsureOwnedByQuest(QuestNode);
	for (UEGQuestNode_Objective* Objective : Objectives)
	{
		EnsureOwnedByQuest(Objective);
	}
}
// End own functions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
