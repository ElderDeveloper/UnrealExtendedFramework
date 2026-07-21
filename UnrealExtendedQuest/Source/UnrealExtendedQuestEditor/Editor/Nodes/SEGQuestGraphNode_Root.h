// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "Widgets/DeclarativeSyntaxSupport.h"

#include "SEGQuestGraphNode.h"

class UEGQuestGraphNode_Root;

/**
 * Widget for UEGQuestGraphNode_Root
 */
class UNREALEXTENDEDQUESTEDITOR_API SEGQuestGraphNode_Root : public SEGQuestGraphNode
{
	typedef SEGQuestGraphNode Super;
public:

	SLATE_BEGIN_ARGS(SEGQuestGraphNode_Root) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEGQuestGraphNode_Root* InNode);
private:
	// The quest root this view represents
	UEGQuestGraphNode_Root* QuestGraphNode_Root = nullptr;
};
