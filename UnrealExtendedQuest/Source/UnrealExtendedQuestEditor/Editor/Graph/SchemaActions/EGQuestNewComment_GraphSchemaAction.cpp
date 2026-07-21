// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#include "EGQuestNewComment_GraphSchemaAction.h"

#include "EdGraphNode_Comment.h"

#include "UnrealExtendedQuestEditor/EGQuestEditorUtilities.h"

#define LOCTEXT_NAMESPACE "NewComment_QuestEdGraphSchemaAction"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FEGQuestNewComment_GraphSchemaAction
UEdGraphNode* FEGQuestNewComment_GraphSchemaAction::PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin,
	FNYLocationVector2f Location, bool bSelectNewNode/* = true*/)
{
	// Add menu item for creating comment boxes
	UEdGraphNode_Comment* CommentTemplate = NewObject<UEdGraphNode_Comment>();

	// Wrap comment around other nodes, this makes it possible to select other nodes and press the "C" key on the keyboard.
	FNYVector2f SpawnLocation = Location;
	FSlateRect Bounds;
	if (FEGQuestEditorUtilities::GetBoundsForSelectedNodes(ParentGraph, Bounds, 50.0f))
	{
		CommentTemplate->SetBounds(Bounds);
		SpawnLocation.X = CommentTemplate->NodePosX;
		SpawnLocation.Y = CommentTemplate->NodePosY;
	}

	return FEdGraphSchemaAction_NewNode::SpawnNodeFromTemplate<UEdGraphNode_Comment>(ParentGraph, CommentTemplate, SpawnLocation, bSelectNewNode);
}

#undef LOCTEXT_NAMESPACE
