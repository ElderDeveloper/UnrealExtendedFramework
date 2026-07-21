// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "SGraphNode.h"
#include "SGraphPanel.h"

#include "EGQuestGraphNode.h"
#include "UnrealExtendedQuest/EGQuestPluginSettings.h"

class SVerticalBox;

/**
 * Widget for UEGQuestGraphNode_Base
 */
class UNREALEXTENDEDQUESTEDITOR_API SEGQuestGraphNode_Base : public SGraphNode
{
	typedef SGraphNode Super;
	typedef SEGQuestGraphNode_Base Self;

public:
	SLATE_BEGIN_ARGS(Self) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEGQuestGraphNode_Base* InNode);

	// Begin SGraphNode Interface
	/** Create the widgets for pins on the node */
	virtual void CreatePinWidgets() override;

	/** Create a single pin widget */
	virtual void CreateStandardPinWidget(UEdGraphPin* Pin) override;

	/** Update this GraphNode to match the data that it is observing */
	virtual void UpdateGraphNode() override;

	/** @param OwnerPanel  The GraphPanel that this node belongs to */
	virtual void SetOwner(const TSharedRef<SGraphPanel>& OwnerPanel) override;

	/**
	 * Cards render no input pin widget, so alt-clicking the node body is how the user breaks the
	 * arrows arriving at this node.
	 */
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	// End SGraphNode Interface

	// Begin own methods

	/** Is the current node visible? */
	virtual EVisibility GetNodeVisibility() const { return EVisibility::Visible; }

protected:
	// SGraphNode Interface
	/**
	 * Add a new pin to this graph node. The pin must be newly created.
	 *
	 * @param PinToAdd   A new pin to add to this GraphNode.
	 */
	virtual void AddPin(const TSharedRef<SGraphPin>& PinToAdd) override;

	/** Hook that allows derived classes to supply their own SGraphPin derivatives for any pin. Used by CreateStandardPinWidget. */
	virtual TSharedPtr<SGraphPin> CreatePinWidget(UEdGraphPin* Pin) const override;

private:
	/** Tells us if the provided pin is valid.  */
	bool IsValidPin(UEdGraphPin* Pin)
	{
		return ensureMsgf(Pin->GetOuter() == GraphNode,
			TEXT("Graph node ('%s' - %s) has an invalid %s pin: '%s'; (with a bad %s outer: '%s'); skiping creation of a widget for this pin."),
			*GraphNode->GetNodeTitle(ENodeTitleType::ListView).ToString(),
			*GraphNode->GetPathName(),
			Pin->Direction == EEdGraphPinDirection::EGPD_Input ? TEXT("input") : TEXT("output"),
			Pin->PinFriendlyName.IsEmpty() ? *Pin->PinName.ToString() : *Pin->PinFriendlyName.ToString(),
			Pin->GetOuter() ? *Pin->GetOuter()->GetClass()->GetName() : TEXT("UNKNOWN"),
			Pin->GetOuter() ? *Pin->GetOuter()->GetPathName() : TEXT("NULL"));
	}

protected:
	// The Base quest node this widget represents
	UEGQuestGraphNode_Base* QuestGraphNode_Base = nullptr;

	// Cache the Quest settings
	const UEGQuestPluginSettings* Settings = nullptr;

	/** The area where output/input pins reside */
	TSharedPtr<SVerticalBox> PinsNodeBox;
};
