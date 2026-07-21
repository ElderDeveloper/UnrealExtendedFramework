// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "EdGraph/EdGraphSchema.h"
#include "Runtime/Launch/Resources/Version.h"

#include "UnrealExtendedQuest/Nodes/EGQuestNode.h"
#include "UnrealExtendedQuest/NYEngineVersionHelpers.h"
#include "UnrealExtendedQuestEditor/EGQuestEditorUtilities.h"
#include "EGQuestGraphConnectionDrawingPolicy.h"
#include "SchemaActions/EGQuestNewComment_GraphSchemaAction.h"

#include "EGQuestEdGraphSchema.generated.h"

class UEGQuestGraphNode;
class UGraphNodeContextMenuContext;
class UToolMenu;
class FMenuBuilder;
class FSlateWindowElementList;
class UEdGraph;

UCLASS()
class UEGQuestEdGraphSchema : public UEdGraphSchema
{
	GENERATED_BODY()

public:
	/** Check whether connecting these pins would cause a loop */
	bool ConnectionCausesLoop(const UEdGraphPin* InputPin, const UEdGraphPin* OutputPin) const;

	//~ Begin EdGraphSchema Interface
	/**
	 * Get all actions that can be performed when right clicking on a graph or drag-releasing on a graph from a pin
	 *
	 * @param [in,out]	ContextMenuBuilder	The context (graph, dragged pin, etc...) and output menu builder.
	 */
	virtual void GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const override;

	/**
	 * Gets actions that should be added to the right-click context menu for a node or pin
	 * @param	Menu				The menu to append actions to.
	 * @param	Context				The menu's context.
	 */
#if NY_ENGINE_VERSION >= 424
	virtual void GetContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const override;
#else
	virtual void GetContextMenuActions(
		const UEdGraph* CurrentGraph,
		const UEdGraphNode* InGraphNode,
		const UEdGraphPin* InGraphPin,
		FMenuBuilder* MenuBuilder,
		bool bIsDebugging
	) const override;
#endif

	/**
	 * Populate new graph with any default nodes
	 *
	 * @param	Graph			Graph to add the default nodes to
	 */
	virtual void CreateDefaultNodesForGraph(UEdGraph& Graph) const override;


	/** Break links on this pin and create links instead on MoveToPin */
	virtual FPinConnectionResponse MovePinLinks(
		UEdGraphPin& MoveFromPin,
		UEdGraphPin& MoveToPin,
		bool bIsIntermediateMove = false,
		bool bNotifyLinkedNodes = false
	) const override;

	/** Copies pin links from one pin to another without breaking the original links */
	FPinConnectionResponse CopyPinLinks(UEdGraphPin& CopyFromPin, UEdGraphPin& CopyToPin, bool bIsIntermediateCopy = false) const override;

	/**
	 * Determine if a connection can be created between two pins.
	 *
	 * @param	PinA	The first pin.
	 * @param	PinB	The second pin.
	 *
	 * @return	An empty string if the connection is legal, otherwise a message describing why the connection would fail.
	 */
	virtual const FPinConnectionResponse CanCreateConnection(const UEdGraphPin* PinA, const UEdGraphPin* PinB) const override;

	/**
	 * Try to make a connection between two pins.
	 *
	 * @param	PinA	The first pin.
	 * @param	PinB	The second pin.
	 *
	 * @return	True if a connection was made/broken (graph was modified); false if the connection failed and had no side effects.
	 */
	virtual bool TryCreateConnection(UEdGraphPin* PinA, UEdGraphPin* PinB) const override;

	/**
	 * Cards render no input pin widget, so finishing a drag by hand means dropping the wire on the
	 * node body; these two hooks are how FDragConnection resolves such a drop to the input pin.
	 */
	virtual bool SupportsDropPinOnNode(UEdGraphNode* InTargetNode, const FEdGraphPinType& InSourcePinType, EEdGraphPinDirection InSourcePinDirection, FText& OutErrorMessage) const override;
	virtual UEdGraphPin* DropPinOnNode(UEdGraphNode* InTargetNode, const FName& InSourcePinName, const FEdGraphPinType& InSourcePinType, EEdGraphPinDirection InSourcePinDirection) const override;

	/** If we should disallow viewing and editing of the supplied pin */
	virtual bool ShouldHidePinDefaultValue(UEdGraphPin* Pin) const override;

	/**
	 * Breaks all links from/to a single node
	 *
	 * @param	TargetNode	The node to break links on
	 */
	virtual void BreakNodeLinks(UEdGraphNode& TargetNode) const override;

	/**
	 * Breaks all links from/to a single pin
	 *
	 * @param	TargetPin	The pin to break links on
	 * @param	bSendsNodeNotifcation	whether to send a notification to the node post pin connection change
	 */
	virtual void BreakPinLinks(UEdGraphPin& TargetPin, bool bSendsNodeNotifcation) const override;

	/**
	 * Breaks the link between two nodes.
	 *
	 * @param	SourcePin	The pin where the link begins.
	 * @param	TargetPin	The pin where the link ends.
	 */
	virtual void BreakSinglePinLink(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin) const override;

	/** Called when asset(s) are dropped onto the specified node */
	virtual void DroppedAssetsOnGraph(const TArray<FAssetData>& Assets, const FVector2D& GraphPosition, UEdGraph* Graph) const override;

	/** Called when asset(s) are dropped onto the specified node */
	virtual void DroppedAssetsOnNode(const TArray<FAssetData>& Assets, const FVector2D& GraphPosition, UEdGraphNode* Node) const override;

	/**
	 * Returns the currently selected graph node count
	 *
	 * @param	Graph			The active graph to find the selection count for
	 */
	virtual int32 GetNodeSelectionCount(const UEdGraph* Graph) const override { return FEGQuestEditorUtilities::GetSelectedNodes(Graph).Num(); }

	/**
	 * When a node is removed, this method determines whether we should remove it immediately or use the old (slower) code path that
	 * results in all node being recreated:
	 */
	virtual bool ShouldAlwaysPurgeOnModification() const override { return true; }

	/** Returns schema action to create comment from implemention */
	virtual TSharedPtr<FEdGraphSchemaAction> GetCreateCommentAction() const override
	{
		return TSharedPtr<FEdGraphSchemaAction>(static_cast<FEdGraphSchemaAction*>(new FEGQuestNewComment_GraphSchemaAction));
	}

	/* Returns new FConnectionDrawingPolicy from this schema */
	virtual FConnectionDrawingPolicy* CreateConnectionDrawingPolicy(
		int32 InBackLayerID,
		int32 InFrontLayerID,
		float InZoomFactor,
		const FSlateRect& InClippingRect,
		FSlateWindowElementList& InDrawElements,
		UEdGraph* InGraphObj
	) const override
	{
		return new FEGQuestGraphConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, InZoomFactor, InClippingRect, InDrawElements, InGraphObj);
	}
	//~ End EdGraphSchema Interface

	// Begin own functions
	/**
	 * Breaks all links from/to a single pin
	 *
	 * @param	FromPin		The pin to break links from
	 * @param	ToPin		The pin we are breaking links to
	 * @param	bSendsNodeNotifcation	whether to send a notification to the node post pin connection change
	 */
	void BreakLinkTo(UEdGraphPin* TargetPin, UEdGraphPin* ToPin, bool bSendsNodeNotifcation) const;

private:
	//~ Begin own functions
	/** Adds action for creating a comment */
	void GetCommentAction(FGraphActionMenuBuilder& ActionMenuBuilder, const UEdGraph* CurrentGraph = nullptr) const;

	/** Adds actions for creating every type of QuestNode */
	void GetAllQuestNodeActions(FGraphActionMenuBuilder& ActionMenuBuilder) const;

	/** Generates a list of all available UEGQuestNode classes */
	static void InitQuestNodeClasses();

public:
	// Allowed PinType.PinCategory values
	static const FName PIN_CATEGORY_Input;
	static const FName PIN_CATEGORY_Output;
	// Objective row outcome pins on a stage card. The category is what colors the wire.
	static const FName PIN_CATEGORY_Success;
	static const FName PIN_CATEGORY_Fail;

	// Categories for actions
	static const FText NODE_CATEGORY_Quest;
	static const FText NODE_CATEGORY_Graph;
	static const FText NODE_CATEGORY_Convert;

private:
	/** A list of all available UEGQuestNode classes */
	static TArray<TSubclassOf<UEGQuestNode>> QuestNodeClasses;

	/** Whether the list of UEGQuestNode classes has been populated */
	static bool bQuestNodeClassesInitialized;
};
