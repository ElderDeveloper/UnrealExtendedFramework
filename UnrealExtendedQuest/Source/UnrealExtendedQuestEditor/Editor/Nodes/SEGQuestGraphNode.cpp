// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#include "SEGQuestGraphNode.h"

#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/SToolTip.h"
#include "GraphEditorSettings.h"
#include "SCommentBubble.h"
#include "SLevelOfDetailBranchNode.h"
#include "IDocumentation.h"
#include "ScopedTransaction.h"

#include "SEGQuestNodeOverlayWidget.h"
#include "SEGQuestGraphPin.h"
#include "UnrealExtendedQuestEditor/EGQuestStyle.h"
#include "UnrealExtendedQuest/EGQuestGraph.h"

#define LOCTEXT_NAMESPACE "QuestEditor"


/////////////////////////////////////////////////////
// SEGQuestGraphNode
void SEGQuestGraphNode::Construct(const FArguments& InArgs, UEGQuestGraphNode* InNode)
{
	Super::Construct(Super::FArguments(), InNode);
	QuestGraphNode = InNode;

	SetCursor(EMouseCursor::CardinalCross);
	UpdateGraphNode();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Begin SNodePanel::SNode Interface
TArray<FOverlayWidgetInfo> SEGQuestGraphNode::GetOverlayWidgets(bool bSelected, const FNYVector2f& WidgetSize) const
{
	TArray<FOverlayWidgetInfo> Widgets;

	// The join badge: several arrows must all be satisfied before this node fires.
	if (JoinOverlayWidget.IsValid() && IsJoinDestination())
	{
		FOverlayWidgetInfo Overlay(JoinOverlayWidget);
		Overlay.OverlayOffset = FNYVector2f(-JoinOverlayWidget->GetDesiredSize().X / 1.5f, 0.0f);
		Widgets.Add(Overlay);
	}

	return Widgets;
}

// End SNodePanel::SNode Interface
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Begin SGraphNode interface
void SEGQuestGraphNode::UpdateGraphNode()
{
	Super::UpdateGraphNode();
	SetupErrorReporting();

	// The widget is rebuilt from scratch on every update: rows come and go with the objectives.
	NodeBodyWidget.Reset();
	TitleWidget.Reset();
	ObjectivePinBoxes.Empty();

	const FMargin NodePadding = 10.0f;

	static constexpr int HeightOverride = 24;

	JoinOverlayWidget = SNew(SEGQuestNodeOverlayWidget)
		.OverlayBody(
			SNew(SBox)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.HeightOverride(HeightOverride)
			.Padding(FMargin(4.f, 0.f))
			[
				SNew(STextBlock)
				.Text(LOCTEXT("JoinBadge", "ALL"))
				.ColorAndOpacity(FLinearColor::White)
				.Font(FNYAppStyle::GetFontStyle("BTEditor.Graph.BTNode.IndexText"))
			]
		)
		.ToolTipText(LOCTEXT("JoinBadgeTooltip", "This node is a join: it fires only when every arrow pointing into it is satisfied."))
		.Visibility(this, &Self::GetOverlayWidgetVisibility)
		.OnGetBackgroundColor(this, &Self::GetOverlayWidgetBackgroundColor);

	// Set Default tooltip
	if (!SWidget::GetToolTip().IsValid())
	{
		TSharedRef<SToolTip> DefaultToolTip = IDocumentation::Get()->CreateToolTip(TAttribute<FText>(this, &Super::GetNodeTooltip), nullptr,
			GraphNode->GetDocumentationLink(), GraphNode->GetDocumentationExcerptName());
		SetToolTip(DefaultToolTip);
	}

	// Setup content
	{
		ContentScale.Bind(this, &Super::GetContentScale);

		if (QuestGraphNode->IsStageNode())
		{
			// A stage card: its rows carry their own pins, so the body is the whole node.
			GetOrAddSlot(ENodeZone::Center)
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Center)
				[
					SNew(SBorder)
					.BorderImage(FNYAppStyle::GetBrush("Graph.StateNode.Body"))
					.Padding(0)
					.BorderBackgroundColor(Settings->BorderBackgroundColor)
					[
						GetNodeBodyWidget()
					]
				];
		}
		else
		{
			// Root, end pill and custom nodes: the (single) output pin fills the node so a wire can
			// be dragged from anywhere on it.
			GetOrAddSlot(ENodeZone::Center)
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Center)
				[
					SNew(SBorder)
					.BorderImage(FNYAppStyle::GetBrush("Graph.StateNode.Body"))
					.Padding(0)
					.BorderBackgroundColor(Settings->BorderBackgroundColor)
					[
						SNew(SOverlay)
						+SOverlay::Slot()
						.HAlign(HAlign_Fill)
						.VAlign(VAlign_Fill)
						[
							PinsNodeBox.ToSharedRef()
						]

						// Content/Body area
						+SOverlay::Slot()
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.Padding(NodePadding)
						[
							GetNodeBodyWidget()
						]
					]
				];
		}
		CreatePinWidgets();
	}

	// Create comment bubble
	{
		TSharedPtr<SCommentBubble> CommentBubble;
		const FSlateColor CommentColor = GetDefault<UGraphEditorSettings>()->DefaultCommentNodeTitleColor;

		SAssignNew(CommentBubble, SCommentBubble)
			.GraphNode(GraphNode)
			.Text(this, &Super::GetNodeComment)
			.OnTextCommitted(this, &Super::OnCommentTextCommitted)
			.OnToggled(this, &Super::OnCommentBubbleToggled)
			.ColorAndOpacity(CommentColor)
			.AllowPinning(true)
			.EnableTitleBarBubble(true)
			.EnableBubbleCtrls(true)
			.GraphLOD(this, &Super::GetCurrentLOD)
			.IsGraphNodeHovered(this, &Super::IsHovered);

		// Add it at the top, right above
		GetOrAddSlot(ENodeZone::TopCenter)
#if NY_ENGINE_VERSION >= 506
			.SlotOffset2f(TAttribute<FVector2f>(CommentBubble.Get(), &SCommentBubble::GetOffset2f))
			.SlotSize2f(TAttribute<FVector2f>(CommentBubble.Get(), &SCommentBubble::GetSize2f))
#else
			.SlotOffset(TAttribute<FVector2D>(CommentBubble.Get(), &SCommentBubble::GetOffset))
			.SlotSize(TAttribute<FVector2D>(CommentBubble.Get(), &SCommentBubble::GetSize))
#endif
			.AllowScaling(TAttribute<bool>(CommentBubble.Get(), &SCommentBubble::IsScalingAllowed))
			.VAlign(VAlign_Top)
			[
				CommentBubble.ToSharedRef()
			];
	}
}

void SEGQuestGraphNode::AddPin(const TSharedRef<SGraphPin>& PinToAdd)
{
	const UEdGraphPin* PinObj = PinToAdd->GetPinObj();
	if (PinObj)
	{
		if (const TSharedPtr<SHorizontalBox>* RowPinBox = ObjectivePinBoxes.Find(PinObj->PinName))
		{
			// An objective outcome pin docks at the right edge of its row.
			PinToAdd->SetOwner(SharedThis(this));
			(*RowPinBox)->AddSlot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(2.0f, 0.0f)
				[
					PinToAdd
				];
			OutputPins.Add(PinToAdd);
			return;
		}
	}

	Super::AddPin(PinToAdd);
}
// End SGraphNode interface
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Begin own functions
TSharedRef<SWidget> SEGQuestGraphNode::GetNodeBodyWidget()
{
	if (NodeBodyWidget.IsValid())
	{
		return NodeBodyWidget.ToSharedRef();
	}

	TSharedPtr<SVerticalBox> NodeVerticalBox;

	NodeBodyWidget =
		SNew(SBorder)
		.BorderImage(FNYAppStyle::GetBrush("BTEditor.Graph.BTNode.Body"))
		.BorderBackgroundColor(this, &Self::GetBackgroundColor)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Center)
		.Padding(1.0f)
		.Visibility(EVisibility::Visible)
		[
			// Main Content
			SAssignNew(NodeVerticalBox, SVerticalBox)
		];

	// Title
	NodeVerticalBox->AddSlot()
	.HAlign(HAlign_Fill)
	.VAlign(VAlign_Fill)
	.AutoHeight()
	.Padding(6.0f, 4.0f)
	[
		GetTitleWidget()
	];

	// Description preview
	NodeVerticalBox->AddSlot()
	.HAlign(HAlign_Fill)
	.VAlign(VAlign_Fill)
	.AutoHeight()
	.Padding(6.0f, 0.0f, 6.0f, 4.0f)
	[
		SNew(STextBlock)
		.Visibility_Lambda([this]() -> EVisibility
		{
			return GetDescription().IsEmpty() ? EVisibility::Collapsed : GetDescriptionVisibility();
		})
		.Text(this, &Self::GetDescription)
		.ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.55f))
		.WrapTextAt(Settings->DescriptionWrapTextAt)
		.Margin(Settings->DescriptionTextMargin)
	];

	// Enter events, shown under the journal text
	AddEnterEventRows(NodeVerticalBox.ToSharedRef());

	if (!QuestGraphNode->IsStageNode() && QuestGraphNode->GetMutableQuestNode()->EmitsRoutes())
	{
		NodeVerticalBox->AddSlot()
		.AutoHeight()
		.Padding(6.0f, 1.0f, 6.0f, 3.0f)
		[
			BuildRoutePriorityRows(QuestGraphNode->GetMutableQuestNode())
		];
	}

	// The checklist
	if (QuestGraphNode->IsStageNode())
	{
		NodeVerticalBox->AddSlot()
		.AutoHeight()
		.Padding(4.0f, 2.0f)
		[
			SNew(SSeparator)
			.Orientation(Orient_Horizontal)
		];

		for (UEGQuestNode_Objective* Objective : QuestGraphNode->GetMutableObjectives())
		{
			NodeVerticalBox->AddSlot()
			.AutoHeight()
			.Padding(6.0f, 2.0f)
			[
				BuildObjectiveRow(Objective)
			];
		}

		NodeVerticalBox->AddSlot()
		.AutoHeight()
		.HAlign(HAlign_Left)
		.Padding(6.0f, 2.0f, 6.0f, 5.0f)
		[
			SNew(SButton)
			.ButtonStyle(FNYAppStyle::Get(), "NoBorder")
			.ContentPadding(FMargin(2.0f, 0.0f))
			.OnClicked(this, &Self::OnAddObjectiveClicked)
			.ToolTipText(LOCTEXT("AddObjectiveTooltip", "Adds an objective to this stage's checklist"))
			[
				SNew(STextBlock)
				.Text(LOCTEXT("AddObjective", "+ add objective"))
				.ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.5f))
			]
		];
	}

	return NodeBodyWidget.ToSharedRef();
}

TSharedRef<SWidget> SEGQuestGraphNode::GetTitleWidget()
{
	if (TitleWidget.IsValid())
	{
		return TitleWidget.ToSharedRef();
	}

	// Title
	TSharedPtr<SNodeTitle> NodeTitleMultipleLines = SNew(SNodeTitle, GraphNode);
	TWeakPtr<SNodeTitle> WeakNodeTitle = NodeTitleMultipleLines;
	auto GetNodeTitlePlaceholderWidth = [WeakNodeTitle]() -> FOptionalSize
	{
		TSharedPtr<SNodeTitle> NodeTitlePin = WeakNodeTitle.Pin();
		const float DesiredWidth = NodeTitlePin.IsValid() ? NodeTitlePin->GetTitleSize().X : 0.0f;
		return FMath::Max(75.0f, DesiredWidth);
	};
	auto GetNodeTitlePlaceholderHeight = [WeakNodeTitle]() -> FOptionalSize
	{
		TSharedPtr<SNodeTitle> NodeTitlePin = WeakNodeTitle.Pin();
		const float DesiredHeight = NodeTitlePin.IsValid() ? NodeTitlePin->GetTitleSize().Y : 0.0f;
		return FMath::Max(22.0f, DesiredHeight);
	};

	TitleWidget = SNew(SHorizontalBox)
		// Error message
		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			ErrorReporting->AsWidget()
		]

		// Title
		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SLevelOfDetailBranchNode)
			.UseLowDetailSlot(this, &Self::UseLowDetailNodeTitles)
			.LowDetail()
			[
				SNew(SBox)
				.WidthOverride_Lambda(GetNodeTitlePlaceholderWidth)
				.HeightOverride_Lambda(GetNodeTitlePlaceholderHeight)
			]
			.HighDetail()
			[
				SNew(SVerticalBox)

				// Display the first line
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.TextStyle(FNYAppStyle::Get(), "Graph.StateNode.NodeTitle")
					.Text(NodeTitleMultipleLines.Get(), &SNodeTitle::GetHeadTitle)
				]

				// Display the rest of the lines (if there is a multi line title)
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					NodeTitleMultipleLines.ToSharedRef()
				]
			]
		];

	return TitleWidget.ToSharedRef();
}

TSharedRef<SWidget> SEGQuestGraphNode::BuildObjectiveRow(UEGQuestNode_Objective* Objective)
{
	if (!Objective)
	{
		return SNew(STextBlock).Text(LOCTEXT("EmptyObjectiveRow", "(empty objective)"));
	}

	const TWeakObjectPtr<UEGQuestNode_Objective> WeakObjective = Objective;
	const TWeakObjectPtr<UEGQuestGraphNode> WeakGraphNode = QuestGraphNode;

	// Register the row's pin box under both outcome pin names; AddPin docks the widgets here.
	TSharedPtr<SHorizontalBox> RowPinBox = SNew(SHorizontalBox);
	ObjectivePinBoxes.Add(UEGQuestGraphNode::MakeObjectivePinName(*Objective, EEGQuestArrowOutcome::Success), RowPinBox);
	ObjectivePinBoxes.Add(UEGQuestGraphNode::MakeObjectivePinName(*Objective, EEGQuestArrowOutcome::Fail), RowPinBox);

	return SNew(SHorizontalBox)

		// The checklist circle and text
		+SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)

			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text_Lambda([WeakObjective]() -> FText
				{
					if (const UEGQuestNode_Objective* Row = WeakObjective.Get())
					{
						const FText& Text = Row->GetNodeText();
						if (!Text.IsEmpty())
						{
							return FText::Format(LOCTEXT("ObjectiveRowText", "○ {0}"), Text);
						}
					}
					return LOCTEXT("ObjectiveRowNoText", "○ (objective)");
				})
				.WrapTextAt(Settings->DescriptionWrapTextAt)
			]

			// Compact evaluation summary: what this row needs, at a glance.
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(14.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text_Lambda([WeakObjective, WeakGraphNode]() -> FText
				{
					UEGQuestNode_Objective* Row = WeakObjective.Get();
					const UEGQuestGraphNode* Node = WeakGraphNode.Get();
					if (!Row || !Node)
					{
						return FText::GetEmpty();
					}
					return FText::FromString(Row->GetEditorDisplayString(Node->GetQuest()));
				})
				.ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.45f))
				.Font(FNYAppStyle::GetFontStyle("Graph.StateNode.NodeTitleExtraLines"))
				.WrapTextAt(Settings->DescriptionWrapTextAt)
			]

			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(14.0f, 1.0f, 0.0f, 0.0f)
			[
				BuildRoutePriorityRows(Objective)
			]
		]

		// The row's outcome pins
		+SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(4.0f, 0.0f, 0.0f, 0.0f)
		[
			RowPinBox.ToSharedRef()
		];
}

TSharedRef<SWidget> SEGQuestGraphNode::BuildRoutePriorityRows(UEGQuestNode* SourceNode)
{
	TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
	if (!SourceNode)
	{
		return Rows;
	}

	const TWeakObjectPtr<UEGQuestNode> WeakSource = SourceNode;
	const TWeakObjectPtr<UEGQuestGraphNode> WeakGraphNode = QuestGraphNode;
	for (const FEGQuestRoutePriority& Route : SourceNode->GetRoutePriorities())
	{
		const FGuid DestinationGuid = Route.DestinationGuid;
		const EEGQuestArrowOutcome Outcome = Route.Outcome;
		const FString OutcomeLabel = Outcome == EEGQuestArrowOutcome::Fail ? TEXT("Fail") : TEXT("Success");
		FString DestinationLabel = DestinationGuid.ToString(EGuidFormats::Short);
		if (const UEGQuestGraph* Quest = SourceNode->GetQuest())
		{
			const int32 DestinationIndex = Quest->GetNodeIndexForGUID(DestinationGuid);
			if (Quest->GetNodes().IsValidIndex(DestinationIndex) && Quest->GetNodes()[DestinationIndex])
			{
				const UEGQuestNode* Destination = Quest->GetNodes()[DestinationIndex];
				const FString Text = Destination->GetNodeText().ToString();
				DestinationLabel = Text.IsEmpty() ? Destination->GetClass()->GetDisplayNameText().ToString() : Text;
			}
		}

		Rows->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 1.0f)
		[
			SNew(SHorizontalBox)

			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::Format(LOCTEXT("RoutePriorityLabel", "{0} -> {1}"),
					FText::FromString(OutcomeLabel), FText::FromString(DestinationLabel)))
				.ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.4f))
				.Font(FNYAppStyle::GetFontStyle("Graph.StateNode.NodeTitleExtraLines"))
			]

			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4.0f, 0.0f)
			[
				SNew(SSpinBox<int32>)
				.MinValue(0)
				.MinSliderValue(0)
				.MaxSliderValue(32)
				.Delta(1)
				.Value_Lambda([WeakSource, DestinationGuid, Outcome]()
				{
					const UEGQuestNode* Source = WeakSource.Get();
					return Source ? FMath::Max(0, Source->FindRoutePriority(DestinationGuid, Outcome)) : 0;
				})
				.OnValueCommitted_Lambda([WeakSource, WeakGraphNode, DestinationGuid, Outcome](int32 NewValue, ETextCommit::Type)
				{
					UEGQuestNode* Source = WeakSource.Get();
					UEGQuestGraphNode* GraphNode = WeakGraphNode.Get();
					if (!Source || !GraphNode || Source->FindRoutePriority(DestinationGuid, Outcome) == NewValue)
					{
						return;
					}

					const FScopedTransaction Transaction(LOCTEXT("SetRoutePriority", "Quest Editor: Set Route Priority"));
					UEGQuestGraph* Quest = Source->GetQuest();
					Quest->Modify();
					Source->Modify();
					if (Source->SetRoutePriority(DestinationGuid, Outcome, NewValue))
					{
						Quest->CompileQuestNodesFromGraphNodes();
						Quest->MarkPackageDirty();
						if (UEdGraph* Graph = GraphNode->GetGraph())
						{
							Graph->NotifyGraphChanged();
						}
					}
				})
				.ToolTipText(LOCTEXT("RoutePriorityTooltip", "Lower values win when several destinations are eligible. Canvas layout is cosmetic."))
			]
		];
	}
	return Rows;
}

void SEGQuestGraphNode::AddEnterEventRows(const TSharedRef<SVerticalBox>& NodeVerticalBox)
{
	// Events read as quiet rows of the card - a small icon and the event's own display string -
	// rather than the old bordered boxes bolted under the body.
	const TArray<TObjectPtr<UEGQuestEventCustom>>& EnterEvents = QuestGraphNode->GetQuestNode().GetNodeEnterEvents();
	const TWeakObjectPtr<UEGQuestGraphNode> WeakGraphNode = QuestGraphNode;

	for (int32 EventIndex = 0; EventIndex < EnterEvents.Num(); ++EventIndex)
	{
		NodeVerticalBox->AddSlot()
		.AutoHeight()
		.Padding(8.0f, 0.0f, 6.0f, 2.0f)
		[
			SNew(SHorizontalBox)

			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 4.0f, 0.0f)
			[
				SNew(SBox)
				.WidthOverride(12.0f)
				.HeightOverride(12.0f)
				[
					SNew(SImage)
					.Image(FEGQuestStyle::Get()->GetBrush(FEGQuestStyle::PROPERTY_EventIcon))
					.ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.6f))
				]
			]

			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text_Lambda([WeakGraphNode, EventIndex]() -> FText
				{
					const UEGQuestGraphNode* Node = WeakGraphNode.Get();
					if (!Node || !Node->IsQuestNodeSet())
					{
						return FText::GetEmpty();
					}
					const TArray<TObjectPtr<UEGQuestEventCustom>>& Events = Node->GetQuestNode().GetNodeEnterEvents();
					if (!Events.IsValidIndex(EventIndex) || !Events[EventIndex])
					{
						return NSLOCTEXT("QuestEditor", "InvalidEventRow", "(invalid event)");
					}
					return FText::FromString(Events[EventIndex]->GetEditorDisplayString(Node->GetQuest()));
				})
				.ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.5f))
				.Font(FNYAppStyle::GetFontStyle("Graph.StateNode.NodeTitleExtraLines"))
				.WrapTextAt(Settings->DescriptionWrapTextAt)
				.ToolTipText(NSLOCTEXT("QuestEditor", "EnterEventRowTooltip", "Enter event: fires when this node is entered."))
			]
		];
	}
}

FText SEGQuestGraphNode::GetDescription() const
{
	if (!QuestGraphNode || !QuestGraphNode->IsQuestNodeSet())
	{
		return FText::GetEmpty();
	}

	// A stage previews its journal body; an end says nothing beyond its pill; custom nodes keep
	// showing their node text.
	if (const UEGQuestNode_Stage* Stage = Cast<UEGQuestNode_Stage>(&QuestGraphNode->GetQuestNode()))
	{
		return Stage->GetDescription();
	}
	if (QuestGraphNode->IsEndNode())
	{
		return FText::GetEmpty();
	}

	return QuestGraphNode->GetQuestNode().GetNodeText();
}

EVisibility SEGQuestGraphNode::GetOverlayWidgetVisibility() const
{
	// always hide the overlays on the root node
	if (QuestGraphNode->IsRootNode())
	{
		return EVisibility::Hidden;
	}

	// LOD this out once things get too small
	TSharedPtr<SGraphPanel> MyOwnerPanel = GetOwnerPanel();
	return !MyOwnerPanel.IsValid() || MyOwnerPanel->GetCurrentLOD() > EGraphRenderingLOD::LowDetail ? EVisibility::Visible : EVisibility::Collapsed;
}

FReply SEGQuestGraphNode::OnAddObjectiveClicked()
{
	if (QuestGraphNode)
	{
		QuestGraphNode->AddNewObjectiveInteractive();
	}
	return FReply::Handled();
}

bool SEGQuestGraphNode::IsJoinDestination() const
{
	return QuestGraphNode && !QuestGraphNode->IsRootNode() && QuestGraphNode->HasInputPin() &&
		QuestGraphNode->GetInputPin()->LinkedTo.Num() > 1;
}
// End own functions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
