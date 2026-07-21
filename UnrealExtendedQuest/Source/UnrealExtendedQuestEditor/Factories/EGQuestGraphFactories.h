// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "EdGraphUtilities.h"

// Factory for creating Quest graph nodes
// WARNING: UEdGraphNode::CreateVisualWidget has priority over this, see FNodeFactory::CreateNodeWidget
struct UNREALEXTENDEDQUESTEDITOR_API FEGQuestGraphNodeFactory : public FGraphPanelNodeFactory
{
	TSharedPtr<class SGraphNode> CreateNode(class UEdGraphNode* InNode) const override;
};

// Factory  for creating pin widgets
// This is the highest priority creator, see FNodeFactory::CreatePinWidget
struct UNREALEXTENDEDQUESTEDITOR_API FEGQuestGraphPinFactory : public FGraphPanelPinFactory
{
public:
	TSharedPtr<class SGraphPin> CreatePin(class UEdGraphPin* Pin) const override;
};

// Factory for creating the drawing policy between nodes.
// Defined in UEGQuestEdGraphSchema::CreateConnectionDrawingPolicy which has priority over FGraphPanelPinConnectionFactory,
// see FNodeFactory::CreateConnectionPolicy
