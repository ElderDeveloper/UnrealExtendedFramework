// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "UObject/ObjectMacros.h"
#include "EdGraph/EdGraphNode.h"

#include "UnrealExtendedQuestEditor/Editor/Graph/EGQuestEdGraphSchema.h"
#include "UnrealExtendedQuestEditor/Editor/Graph/EGQuestEdGraph.h"

#include "EGQuestGraphNode_Base.generated.h"

class UEdGraphPin;
class UEdGraphSchema;
class UEGQuestEdGraphSchema;

/**
 * Represents the base class representation of the quest graph nodes.
 * A node has at most one input pin and any number of output pins (a stage card exposes one or two
 * output pins per objective row); every pin can be linked to multiple nodes.
 */
UCLASS(Abstract)
class UNREALEXTENDEDQUESTEDITOR_API UEGQuestGraphNode_Base : public UEdGraphNode
{
	GENERATED_BODY()

public:
	//~ Begin UObject Interface.
	/**
	 * Do any object-specific cleanup required immediately after loading an object,
	 * and immediately after any undo/redo.
	 */
	virtual void PostLoad() override;

	/**
	 *  Called after duplication & serialization and before PostLoad. Used to e.g. make sure UStaticMesh's UModel gets copied as well.
	 *  Note: NOT called on components on actor duplication (alt-drag or copy-paste).  Use PostEditImport as well to cover that case.
	 */
	virtual void PostDuplicate(bool bDuplicateForPIE) override;

	/**
	 * Called after importing property values for this object (paste, duplicate or .t3d import)
	 * Allow the object to perform any cleanup for properties which shouldn't be duplicated or
	 * are unsupported by the script serialization
	 */
	virtual void PostEditImport() override;

	// UEdGraphNode interface.
	/**
	 * A chance to initialize a new node; called just once when a new node is created, before AutowireNewNode or AllocateDefaultPins is called.
	 * This method is not called when a node is reconstructed, etc...
	 */
	virtual void PostPlacedNewNode() override { RegisterListeners(); }

	/** Allocate default pins for a given node, based only the NodeType, which should already be filled in. */
	virtual void AllocateDefaultPins() override;

	/** Refresh the connectors on a node, preserving as many connections as it can. */
	virtual void ReconstructNode() override;

	/** Whether or not this node can be safely duplicated (via copy/paste, etc...) in the graph */
	virtual bool CanDuplicateNode() const override { return true; }

	/** Whether or not this node can be deleted by user action */
	virtual bool CanUserDeleteNode() const override { return true; }

	/** Perform any steps necessary prior to copying a node into the paste buffer */
	virtual void PrepareForCopying() override { Super::PrepareForCopying(); }

	/** IGNORED. Removes the specified pin from the node, preserving remaining pin ordering. */
	virtual void RemovePinAt(int32 PinIndex, EEdGraphPinDirection PinDirection) override {}

	/** Whether or not struct pins belonging to this node should be allowed to be split or not. */
	virtual bool CanSplitPin(const UEdGraphPin* Pin) const override { return false; }

	/** Determine if this node can be created under the specified schema */
	virtual bool CanCreateUnderSpecifiedSchema(const UEdGraphSchema* Schema) const override
	{
		return Schema->IsA(UEGQuestEdGraphSchema::StaticClass());
	}

	/** Returns the link used for external documentation for the graph node. */
	virtual FString GetDocumentationLink() const override { return TEXT("Shared/QuestGraphNode"); }

	/** Should we show the Palette Icon for this node on the node title */
	virtual bool ShowPaletteIconOnNode() const override { return true; }

	/** Gets the draw color of a node's title bar. */
	virtual FLinearColor GetNodeTitleColor() const override { return GetNodeBackgroundColor(); }

	/** @return Icon to use in menu or on node */
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override
	{
		static const FSlateIcon Icon = FSlateIcon(NY_GET_APP_STYLE_NAME(), "Graph.StateNode.Icon");
		OutColor = GetNodeBackgroundColor();
		return Icon;
	}

	/////////////////////////////////////////////////////////////////////////////////////////////
	// Begin own functions
	UEGQuestGraphNode_Base(const FObjectInitializer& ObjectInitializer);

	/** Perform any fixups (deep copies of associated data, etc...) necessary after a node has been copied in the editor. */
	virtual void PostCopyNode() {}

	/** Checks if this node has a output connection to the TargetNode. */
	virtual bool HasOutputConnectionToNode(const UEdGraphNode* TargetNode) const;

	/** Checks whether an input connection can be added to this node */
	virtual bool CanHaveInputConnections() const { return true; }

	/** Checks whether an output connection can be added from this node */
	virtual bool CanHaveOutputConnections() const { return true; }

	/** Gets the background color of this node. */
	virtual FLinearColor GetNodeBackgroundColor() const { return FLinearColor::Black; }

	/** Performs all checks */
	virtual void CheckAll() const {}

	/** Gets the position in the Graph canvas of this node. */
	virtual FIntPoint GetPosition() const { return FIntPoint(NodePosX, NodePosY); }

	/** Sets the position in the Graph canvas of this node. */
	virtual void SetPosition(int32 X, int32 Y)
	{
		NodePosX = X;
		NodePosY = Y;
	}

	// Compiler methods
	/** Clears the compiler messages on this node. */
	void ClearCompilerMessage();

	/** Sets a compiler message of type warning. */
	void SetCompilerWarningMessage(FString Message);

	/** Is the Input pin initialized? */
	bool HasInputPin() const
	{
		return FindFirstPinByDirection(EGPD_Input) != nullptr;
	}

	/** Is at least one Output pin initialized? */
	bool HasOutputPin() const
	{
		return FindFirstPinByDirection(EGPD_Output) != nullptr;
	}

	// @return the input pin for this quest Node
	UEdGraphPin* GetInputPin() const
	{
		UEdGraphPin* Pin = FindFirstPinByDirection(EGPD_Input);
		check(Pin);
		return Pin;
	}

	// @return the first output pin for this quest Node. Stage cards have one per objective outcome -
	// use GetOutputPins (or the stage card's objective pin lookup) to see them all.
	UEdGraphPin* GetOutputPin() const
	{
		UEdGraphPin* Pin = FindFirstPinByDirection(EGPD_Output);
		check(Pin);
		return Pin;
	}

	// @return every output pin of this node, in pin order
	TArray<UEdGraphPin*> GetOutputPins() const
	{
		TArray<UEdGraphPin*> OutputPinsArray;
		for (UEdGraphPin* Pin : Pins)
		{
			if (Pin && Pin->Direction == EGPD_Output)
			{
				OutputPinsArray.Add(Pin);
			}
		}
		return OutputPinsArray;
	}

	// Helper method to get directly the Quest Graph (which is our parent)
	UEGQuestEdGraph* GetQuestEdGraph() const { return CastChecked<UEGQuestEdGraph>(GetGraph()); }

	// Helper method to get directly the Quest
	UEGQuestGraph* GetQuest() const { return GetQuestEdGraph()->GetQuest(); }

	// TODO fix UEdGraphSchema::BreakSinglePinLink, make it to const
	/** Helper method to get directly the Quest Graph Schema */
	const UEGQuestEdGraphSchema* GetQuestEdGraphSchema() const { return GetQuestEdGraph()->GetQuestEdGraphSchema(); }

	/** Widget representing this node if it exists */
	TSharedPtr<SGraphNode> GetNodeWidget() const { return DEPRECATED_NodeWidget.Pin(); }

protected:
	// Begin own functions
	/** Creates the input pin for this node. */
	virtual void CreateInputPin() { unimplemented(); }

	/** Creates the output pin for this node. */
	virtual void CreateOutputPin() { unimplemented(); }

	/** This function is called after one of the properties of the Quest are changed.  */
	virtual void OnQuestPropertyChanged(const FPropertyChangedEvent& PropertyChangedEvent) {}

	/** Registers all the listener this class listens to. */
	virtual void RegisterListeners();

private:
	UEdGraphPin* FindFirstPinByDirection(EEdGraphPinDirection Direction) const
	{
		for (UEdGraphPin* Pin : Pins)
		{
			if (Pin && Pin->Direction == Direction)
			{
				return Pin;
			}
		}
		return nullptr;
	}
};
