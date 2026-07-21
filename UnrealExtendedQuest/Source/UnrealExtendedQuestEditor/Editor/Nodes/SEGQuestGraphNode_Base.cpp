// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#include "SEGQuestGraphNode_Base.h"

#include "Widgets/Layout/SBox.h"
#include "EGQuestGraphNode_Root.h"
#include "IDocumentation.h"
#include "GraphEditorDragDropAction.h"

#include "SEGQuestGraphPin.h"
#include "UnrealExtendedQuestEditor/Editor/Graph/EGQuestEdGraphSchema.h"

#define LOCTEXT_NAMESPACE "QuestEditor"


/////////////////////////////////////////////////////
// SEGQuestGraphNode_Base
void SEGQuestGraphNode_Base::Construct(const FArguments& InArgs, UEGQuestGraphNode_Base* InNode)
{
	GraphNode = Cast<UEdGraphNode>(InNode);
	QuestGraphNode_Base = InNode;
	Settings = GetDefault<UEGQuestPluginSettings>();
}

void SEGQuestGraphNode_Base::CreatePinWidgets()
{
	// Output pins draw as widgets; wires end on the destination card's body, which the connection
	// drawing policy resolves without an input pin widget. Input pins still get a hidden widget:
	// SGraphPanel resolves a hovered wire's endpoints (alt-click break, ctrl-click move, hover
	// highlight) only through registered pin widgets, never through the pin objects.
	for (UEdGraphPin* Pin : QuestGraphNode_Base->Pins)
	{
		if (Pin && IsValidPin(Pin))
		{
			CreateStandardPinWidget(Pin);
		}
	}
}

void SEGQuestGraphNode_Base::CreateStandardPinWidget(UEdGraphPin* Pin)
{
	Super::CreateStandardPinWidget(Pin);
}

void SEGQuestGraphNode_Base::UpdateGraphNode()
{
	// Reset variables that are going to be exposed, in case we are refreshing an already setup node.
	InputPins.Empty();
	OutputPins.Empty();
	RightNodeBox.Reset();
	LeftNodeBox.Reset();
	PinsNodeBox.Reset();
	SAssignNew(PinsNodeBox, SVerticalBox);

	// This Node visibility
	SetVisibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &Self::GetNodeVisibility)));
}

void SEGQuestGraphNode_Base::SetOwner(const TSharedRef<SGraphPanel>& OwnerPanel)
{
	check(!OwnerGraphPanelPtr.IsValid());
	SetParentPanel(OwnerPanel);
	OwnerGraphPanelPtr = OwnerPanel;
	GraphNode->DEPRECATED_NodeWidget = SharedThis(this);

	// Once we have an owner, and if hide Unused pins is enabled, we need to remake our pins to drop the hidden ones
	if (OwnerGraphPanelPtr.Pin()->GetPinVisibility() != SGraphEditor::Pin_Show && PinsNodeBox.IsValid())
	{
		PinsNodeBox->ClearChildren();
		CreatePinWidgets();
	}
}

FReply SEGQuestGraphNode_Base::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// There is no input pin widget to alt-click, so the node body stands in for it: alt-click
	// breaks every arrow arriving at this node. Output/outcome pins still handle their own alt-click
	// before the event ever reaches the body.
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && MouseEvent.IsAltDown() && IsNodeEditable())
	{
		if (QuestGraphNode_Base->HasInputPin())
		{
			UEdGraphPin* InputPin = QuestGraphNode_Base->GetInputPin();
			if (InputPin->LinkedTo.Num() > 0)
			{
				const UEGQuestEdGraphSchema* Schema = CastChecked<UEGQuestEdGraphSchema>(QuestGraphNode_Base->GetSchema());
				Schema->BreakPinLinks(*InputPin, true);
			}
		}
		return FReply::Handled();
	}

	return Super::OnMouseButtonDown(MyGeometry, MouseEvent);
}

TSharedPtr<SGraphPin> SEGQuestGraphNode_Base::CreatePinWidget(UEdGraphPin* Pin) const
{
	// Called by CreateStandardPinWidget
	return SNew(SEGQuestGraphPin, Pin);
}

void SEGQuestGraphNode_Base::AddPin(const TSharedRef<SGraphPin>& PinToAdd)
{
	PinToAdd->SetOwner(SharedThis(this));

	if (PinToAdd->GetDirection() == EGPD_Input)
	{
		// Registered but never laid out: the card body is the visual/drop target for arrows, the
		// widget exists only so wire endpoint lookups (FGraphPinHandle::FindInGraphPanel) succeed.
		PinToAdd->SetVisibility(EVisibility::Collapsed);
		InputPins.Add(PinToAdd);
		return;
	}

	PinsNodeBox->AddSlot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		.FillHeight(1.0f)
		[
			PinToAdd
		];

	OutputPins.Add(PinToAdd);
}
// End SGraphNode interface
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
