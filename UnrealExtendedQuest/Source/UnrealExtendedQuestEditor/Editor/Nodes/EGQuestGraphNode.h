// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "CoreTypes.h"
#include "UObject/ObjectMacros.h"
#include "Runtime/Launch/Resources/Version.h"

#include "UnrealExtendedQuest/Nodes/EGQuestNode_End.h"
#include "UnrealExtendedQuest/Nodes/EGQuestNode_Objective.h"
#include "UnrealExtendedQuest/Nodes/EGQuestNode_Stage.h"
#include "UnrealExtendedQuest/Nodes/EGQuestNode_Custom.h"
#include "UnrealExtendedQuest/EGQuestTypes.h"
#include "EGQuestGraphNode_Base.h"
#include "UnrealExtendedQuest/NYEngineVersionHelpers.h"

#include "EGQuestGraphNode.generated.h"

class UEdGraphPin;
class UToolMenu;
class UGraphNodeContextMenuContext;
struct FEGQuestDiagnostics;

/**
 * The graph node for everything placeable on the quest canvas: a stage card, an end pill, or a
 * custom node.
 *
 * A stage card owns its objectives: they are not graph nodes but rows of this node, and each row
 * exposes one output pin per outcome (a success pin always, a fail pin only while the objective can
 * fail - see UEGQuestNode_Objective::CanEverFail). Pins are named by the objective's GUID so wires
 * survive reorders and rebuilds.
 *
 * The EdGraph is the authority while editing; the runtime arrays (Quest.Nodes/Children) are compiler
 * output, rebuilt on every compile. Nothing here keeps the runtime edges in sync live.
 */
UCLASS()
class UNREALEXTENDEDQUESTEDITOR_API UEGQuestGraphNode : public UEGQuestGraphNode_Base
{
	GENERATED_BODY()

public:
	//
	// Begin UObject Interface.
	//

	/** Fixup any QuestNode/Objective back pointers that may be out of date */
	virtual void PostLoad() override;

	/** Called after importing property values for this object (paste, duplicate or .t3d import) */
	virtual void PostEditImport() override;

	/** Called when a property on this object has been modified externally */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	/** Called when properties inside structs/sub-objects are modified (objective rows edit through here) */
	virtual void PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent) override;

	virtual bool Modify(bool bAlwaysMarkDirty = true) override;

	//
	// Begin UEdGraphNode interface
	//

	/** Gets the name of this node, shown in title bar */
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;

	/** Gets the tooltip to display when over the node */
	virtual FText GetTooltipText() const override;
	virtual FString GetDocumentationExcerptName() const override;

	/** Whether or not this node can be safely duplicated (via copy/paste, etc...) in the graph. */
	virtual bool CanDuplicateNode() const override { return !IsRootNode(); }

	/** Perform any steps necessary prior to copying a node into the paste buffer */
	virtual void PrepareForCopying() override;

	/** Called when something external to this node has changed the connection list of any of the pins */
	virtual void NodeConnectionListChanged() override
	{
		ApplyCompilerWarnings();
	}

	/** Allocate pins: stage cards get one input and per-objective outcome pins. */
	virtual void AllocateDefaultPins() override;

	/** Gets a list of actions that can be done to this particular node */
#if NY_ENGINE_VERSION >= 424
	virtual void GetNodeContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const override;
#else
	virtual void GetContextMenuActions(const FGraphNodeContextMenuBuilder& Context) const override;
#endif

	/**
	 * Autowire a newly created node.
	 *
	 * @param FromPin	The source pin that caused the new node to be created (typically a drag-release context menu creation).
	 */
	virtual void AutowireNewNode(UEdGraphPin* FromPin) override;

	// Begin UEGQuestGraphNode_Base interface

	/** Checks whether an input connection can be added to this node */
	virtual bool CanHaveInputConnections() const override;

	/** Checks whether an output connection can be added from this node */
	virtual bool CanHaveOutputConnections() const override;

	/** Gets the background color of this node. */
	virtual FLinearColor GetNodeBackgroundColor() const override;

	/** Perform any fixups (deep copies of associated data, etc...) necessary after a node has been copied in the editor. */
	virtual void PostCopyNode() override;

	/** Perform all checks */
	virtual void CheckAll() const override
	{
#if DO_CHECK
		Super::CheckAll();
		check(IsQuestNodeSet());
		CheckQuestNodeIndexMatchesNode();
#endif
	}

	/** Is this the undeletable root node */
	virtual bool IsRootNode() const { return false; }

	//
	// Begin own functions
	//

	/** Is this an End Node? */
	bool IsEndNode() const { return QuestNode->IsA<UEGQuestNode_End>(); }

	/** Is this a Stage Node? */
	bool IsStageNode() const { return QuestNode->IsA<UEGQuestNode_Stage>(); }

	/** Is custom Node? */
	bool IsCustomNode() const { return QuestNode->IsA<UEGQuestNode_Custom>(); }

	/** Does this node has any enter events? */
	bool HasEnterEvents() const
	{
		return QuestNode ? QuestNode->HasAnyEnterEvents() : false;
	}

	/** Checks if it is normal for the node to not have a parent */
	bool CanBeOrphan() const;

	/** Gets the node depth in the graph. */
	int32 GetNodeDepth() const { return NodeDepth; }

	/** Sets the new node depth. */
	void SetNodeDepth(int32 NewNodeDepth) { NodeDepth = NewNodeDepth; }

	/** Sets the Quest Node. */
	virtual void SetQuestNode(UEGQuestNode* InNode)
	{
		QuestNode = InNode;
		QuestNode->SetFlags(RF_Transactional);
		QuestNode->SetGraphNode(this);
		RegisterListeners();
	}

	/** Sets the Quest node index number, this represents the index from the QuestGraph.Nodes Array */
	virtual void SetQuestNodeIndex(int32 InIndex)
	{
		check(InIndex > INDEX_NONE);
		NodeIndex = InIndex;
	}

	/**
	 * The same SetQuestNodeIndex and SetQuestNode only that it sets them both at once and it does some sanity checking
	 * such as verifying the index is valid in the Quest node and that the index corresponds to this InNode.
	 */
	void SetQuestNodeDataChecked(int32 InIndex, UEGQuestNode* InNode);

	/** Gets the copy of the QuestNode stored by this graph node */
	template <typename QuestNodeType>
	const QuestNodeType& GetQuestNode() const { return *CastChecked<QuestNodeType>(QuestNode); }

	/** Gets the copy of the QuestNode stored by this graph node as a mutable pointer */
	template <typename QuestNodeType>
	QuestNodeType* GetMutableQuestNode() { return CastChecked<QuestNodeType>(QuestNode); }

	// Specialization for the methods above  (by overloading) for the base type UEGQuestNode type so that we do not need to cast
	const UEGQuestNode& GetQuestNode() const { return *QuestNode; }
	UEGQuestNode* GetMutableQuestNode() const { return QuestNode; }

	/** Tells us if the Quest Node is valid non null. */
	bool IsQuestNodeSet() const { return QuestNode != nullptr; }

	/** Gets the Quest node index number for the QuestGraph.Nodes Array */
	virtual int32 GetQuestNodeIndex() const { return NodeIndex; }

	//
	// Objectives: the rows of a stage card. Only meaningful when IsStageNode().
	//

	const TArray<TObjectPtr<UEGQuestNode_Objective>>& GetObjectives() const { return Objectives; }
	TArray<TObjectPtr<UEGQuestNode_Objective>>& GetMutableObjectives() { return Objectives; }

	/** Sets the rows of this stage card. Used when rebuilding the graph from the runtime arrays. */
	void SetObjectives(const TArray<UEGQuestNode_Objective*>& InObjectives);

	/**
	 * Appends a new empty objective row inside its own transaction, rebuilds the pins and recompiles.
	 * The card's "+ add objective" button lands here.
	 */
	UEGQuestNode_Objective* AddNewObjectiveInteractive();

	/** Removes one objective row inside its own transaction, rebuilds the pins and recompiles. */
	void RemoveObjectiveInteractive(UEGQuestNode_Objective* Objective);

	/** The outcome pin of one of this stage's objectives, or null (e.g. no fail pin when it cannot fail). */
	UEdGraphPin* FindObjectivePin(const UEGQuestNode_Objective& Objective, EEGQuestArrowOutcome Outcome) const;

	/** The objective row and outcome a pin of this node belongs to, or null for non-objective pins. */
	UEGQuestNode_Objective* FindObjectiveForPin(const UEdGraphPin& Pin, EEGQuestArrowOutcome& OutOutcome) const;

	/** The pin name encoding an objective row outcome: the outcome prefix plus the objective's GUID. */
	static FName MakeObjectivePinName(const UEGQuestNode_Objective& Objective, EEGQuestArrowOutcome Outcome);

	/** Checks the node for warnings and applies the compiler warnings messages */
	void ApplyCompilerWarnings();

	/** Appends this card's read-only authoring diagnostics to the canonical compiler result. */
	void CollectDiagnostics(FEGQuestDiagnostics& OutDiagnostics) const;

	/** Applies findings anchored to this card or one of its objective rows. */
	void ApplyDiagnostics(const FEGQuestDiagnostics& InDiagnostics);

	/** Estimate the width of this Node from the length of its content */
	int32 EstimateNodeWidth() const;

	/** Checks Quest.Nodes[NodeIndex] == QuestNode */
	void CheckQuestNodeIndexMatchesNode() const;

	/** Gets the parent nodes that are connected to the input pin. */
	TArray<UEGQuestGraphNode*> GetParentNodes() const;

	/** Gets the child nodes that are connected from any output pin. */
	TArray<UEGQuestGraphNode*> GetChildNodes() const;

	/** Helper constants to get the names of some properties. Used by the QuestPluginEditor module. */
	static FName GetMemberNameQuestNode() { return GET_MEMBER_NAME_CHECKED(UEGQuestGraphNode, QuestNode); }
	static FName GetMemberNameObjectives() { return GET_MEMBER_NAME_CHECKED(UEGQuestGraphNode, Objectives); }

protected:
	// Begin UEGQuestGraphNode_Base interface
	/** Creates the input pin for this node. */
	virtual void CreateInputPin()
	{
		static const FName PinName(TEXT("Quest Input Pin"));
		CreatePin(EGPD_Input, UEGQuestEdGraphSchema::PIN_CATEGORY_Input, PinName);
	}

	/** Creates the output pin for this node. */
	virtual void CreateOutputPin()
	{
		static const FName PinName(TEXT("Quest Output Pin"));
		CreatePin(EGPD_Output, UEGQuestEdGraphSchema::PIN_CATEGORY_Output, PinName);
	}

	/** Creates the success (and fail, when it can fail) pins of one objective row. */
	void CreateObjectivePins(const UEGQuestNode_Objective& Objective);

	/** Registers all the listener this class listens to. */
	void RegisterListeners() override;

	//
	// Begin own functions
	//

	/** This function is called after one of the properties of the QuestNode are changed.  */
	void OnQuestNodePropertyChanged(const FPropertyChangedEvent& PropertyChangedEvent, int32 EdgeIndexChanged);

	/** Make sure the QuestNode and the objectives are owned by the Quest */
	void ResetQuestNodeOwner(ERenameFlags ExtraRenameFlags = REN_None);

	/** Rebuilds this node's pins in place and recompiles the quest. */
	void RebuildPinsAndCompile();

protected:
	/** The Quest Node this graph node references.  */
	UPROPERTY(EditAnywhere, Instanced, Category = QuestGraphNode, Meta = (ShowOnlyInnerProperties))
	UEGQuestNode* QuestNode;

	/**
	 * The objectives owned by this stage card, in row (journal and arbitration) order.
	 * Only used when the quest node is a stage.
	 */
	UPROPERTY(EditAnywhere, Instanced, Category = "Stage")
	TArray<TObjectPtr<UEGQuestNode_Objective>> Objectives;

	/** The Quest Node index in the Quest (array) this represents. This is not relevant for the StartNode. */
	UPROPERTY(VisibleAnywhere, Category = QuestGraphNode)
	int32 NodeIndex = INDEX_NONE;

	/** Indicates the distance from the start node. This is only set after the graph is compiled. */
	UPROPERTY()
	int32 NodeDepth = INDEX_NONE;
};
