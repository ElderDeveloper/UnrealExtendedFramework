// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"
#include "SGraphNode.h"
#include "SGraphPanel.h"

#include "EGQuestGraphNode.h"
#include "SEGQuestGraphNode_Base.h"

class SVerticalBox;
class SHorizontalBox;

/**
 * Widget for UEGQuestGraphNode.
 *
 * A stage renders as a card: title, description preview, then one checklist row per objective with
 * that row's outcome pins docked at its right edge. An end renders as a pill stamped with its
 * result. Root and custom nodes keep the plain body with a full-node output pin.
 */
class UNREALEXTENDEDQUESTEDITOR_API SEGQuestGraphNode : public SEGQuestGraphNode_Base
{
	typedef SEGQuestGraphNode_Base Super;
	typedef SEGQuestGraphNode Self;

public:
	SLATE_BEGIN_ARGS(Self) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEGQuestGraphNode* InNode);

	// Begin SNodePanel::SNode Interface

	/** Populate the widgets array with any overlay widgets to render */
	virtual TArray<FOverlayWidgetInfo> GetOverlayWidgets(bool bSelected, const FNYVector2f& WidgetSize) const override;

	// Begin SGraphNode Interface

	/** Update this GraphNode to match the data that it is observing */
	virtual void UpdateGraphNode() override;
	// End SGraphNode Interface

protected:
	//
	// SGraphNode Interface
	//

	/** Route a stage card's objective pins into their row; everything else fills the node. */
	virtual void AddPin(const TSharedRef<SGraphPin>& PinToAdd) override;

	/** Should we use low-detail node titles? Used by UpdateGraphNode() */
	virtual bool UseLowDetailNodeTitles() const override
	{
		if (const SGraphPanel* MyOwnerPanel = GetOwnerPanel().Get())
		{
			return MyOwnerPanel->GetCurrentLOD() <= EGraphRenderingLOD::LowestDetail;
		}

		return false;
	}

	/** Return the desired comment bubble color */
	virtual FSlateColor GetCommentColor() const override { return QuestGraphNode->GetNodeBackgroundColor(); }

	//
	// Begin own functions
	//

	/** The whole node content: a stage card, an end pill, or the plain body. */
	TSharedRef<SWidget> GetNodeBodyWidget();

	/** The title row: error reporting plus the node title. */
	TSharedRef<SWidget> GetTitleWidget();

	/** One checklist row: objective text, condition summary, and the row's outcome pin box. */
	TSharedRef<SWidget> BuildObjectiveRow(UEGQuestNode_Objective* Objective);

	/** Editable authored route priorities, displayed beside their destination rather than canvas position. */
	TSharedRef<SWidget> BuildRoutePriorityRows(UEGQuestNode* SourceNode);

	/** One quiet row per enter event, part of the card rather than a box bolted onto it. */
	void AddEnterEventRows(const TSharedRef<SVerticalBox>& NodeVerticalBox);

	// Gets the background color for a node
	FSlateColor GetBackgroundColor() const { return QuestGraphNode->GetNodeBackgroundColor(); }

	// The description preview: a stage's journal body, a custom node's text.
	FText GetDescription() const;

	// Gets the visibility of the Description
	EVisibility GetDescriptionVisibility() const
	{
		// LOD this out once things get too small
		TSharedPtr<SGraphPanel> MyOwnerPanel = GetOwnerPanel();
		return !MyOwnerPanel.IsValid() || MyOwnerPanel->GetCurrentLOD() > EGraphRenderingLOD::LowDetail ? EVisibility::Visible : EVisibility::Collapsed;
	}

	/** Get the visibility of the overlay widgets. */
	EVisibility GetOverlayWidgetVisibility() const;

	/** Get the background color to display for the widget overlay. */
	FSlateColor GetOverlayWidgetBackgroundColor(bool bHovered) const
	{
		return bHovered ? Settings->BorderHoveredBackgroundColor : Settings->BorderBackgroundColor;
	}

	/** The card's "+ add objective" button. */
	FReply OnAddObjectiveClicked();

	/** More than one arrow arrives here: the destination join (the AND) made visible. */
	bool IsJoinDestination() const;
	// End own functions

protected:
	// The quest this view represents
	UEGQuestGraphNode* QuestGraphNode = nullptr;

	/** The node body widget, cached here so we can determine its size when we want ot position our overlays */
	TSharedPtr<SBorder> NodeBodyWidget;

	/** The widget that holds the title section */
	TSharedPtr<SWidget> TitleWidget;

	/** The "ALL" badge shown on the input side when several arrows must all be satisfied. */
	TSharedPtr<SWidget> JoinOverlayWidget;

	/** Where each objective outcome pin widget docks, keyed by pin name. Rebuilt with the rows. */
	TMap<FName, TSharedPtr<SHorizontalBox>> ObjectivePinBoxes;
};
