// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Misc/Build.h"
#include "UObject/Object.h"

#if WITH_EDITOR
#include "EdGraph/EdGraphNode.h"
#endif

#include "UnrealExtendedQuest/EGQuestEdge.h"
#include "UnrealExtendedQuest/EGQuestEventCustom.h"
// FEGQuestRoutePriority is stored by value in a TArray member, so the type must be complete here.
#include "UnrealExtendedQuest/EGQuestTypes.h"
// GetTextArguments returns a TArray of these, so the type must be complete here. This used to arrive
// transitively through EGQuestEdge.h, which no longer has texts.
#include "UnrealExtendedQuest/EGQuestTextArgument.h"
#include "EGQuestNode.generated.h"


class UEGQuestPluginSettings;
class UEGQuestContext;
class UEGQuestNode;
class UEGQuestGraph;


/**
 *  Abstract base class for Quest nodes
 * Base class for quest flow and objective nodes.
 */
UCLASS(BlueprintType, Abstract, EditInlineNew, ClassGroup = "Quest")
class UNREALEXTENDEDQUEST_API UEGQuestNode : public UObject
{
	GENERATED_BODY()

	friend class UEGQuestComponent;

public:
	//
	// Begin UObject Interface.
	//

	void Serialize(FArchive& Ar) override;
	FString GetDesc() override { return TEXT("INVALID DESCRIPTION"); }
	void PostLoad() override;
	void PostInitProperties() override;
	void PostDuplicate(bool bDuplicateForPIE) override;
	void PostEditImport() override;

	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

#if WITH_EDITOR
	/**
	 * Called when a property on this object has been modified externally
	 *
	 * @param PropertyChangedEvent the property that was modified
	 */
	void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	/**
	 * This alternate version of PostEditChange is called when properties inside structs are modified.  The property that was actually modified
	 * is located at the tail of the list.  The head of the list of the FStructProperty member variable that contains the property that was modified.
	 */
	void PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent) override;

	//
	// Begin own function
	//

	// Used internally by the Quest editor:
	virtual FString GetNodeTypeString() const { return TEXT("INVALID"); }
#endif //WITH_EDITOR

	virtual void OnCreatedInEditor() {};

#if WITH_EDITOR
	void SetGraphNode(UEdGraphNode* InNode) { GraphNode = InNode; }
	void ClearGraphNode() { GraphNode = nullptr; }
	UEdGraphNode* GetGraphNode() const { return GraphNode; }
#endif

	/** Broadcasts whenever a property of this quest changes. */
	DECLARE_EVENT_TwoParams(UEGQuestNode, FEGQuestNodePropertyChanged, const FPropertyChangedEvent& /* PropertyChangedEvent */, int32 /* EdgeIndexChanged */);
	FEGQuestNodePropertyChanged OnQuestNodePropertyChanged;

	/** Fires this node's enter events and formats its texts into the context. */
	virtual bool HandleNodeEnter(UEGQuestContext& Context);

	//
	// Getters/Setters:
	//

	UFUNCTION(BlueprintPure, Category = "Quest|Node")
	FGuid GetGUID() const { return NodeGUID; }

	UFUNCTION(BlueprintPure, Category = "Quest|Node")
	bool HasGUID() const { return NodeGUID.IsValid(); }

	void RegenerateGUID()
	{
		NodeGUID = FGuid::NewGuid();
		Modify();
	}

	//
	// For the EnterEvents
	//

	UFUNCTION(BlueprintPure, Category = "Quest|Node")
	virtual bool HasAnyEnterEvents() const { return GetNodeEnterEvents().Num() > 0; }

	// NOTE: not a UFUNCTION - UHT rejects TObjectPtr in Blueprint signatures. Blueprint reaches the
	// events through HasAnyEnterEvents above.
	virtual const TArray<TObjectPtr<UEGQuestEventCustom>>& GetNodeEnterEvents() const { return EnterEvents; }

	virtual void SetNodeEnterEvents(const TArray<TObjectPtr<UEGQuestEventCustom>>& InEnterEvents) { EnterEvents = InEnterEvents; }

	//
	// For the Children
	//

	/// Gets this nodes children (edges) as a const/mutable array

	UFUNCTION(BlueprintPure, Category = "Quest|Node")
	virtual const TArray<FEGQuestEdge>& GetNodeChildren() const { return Children; }
	virtual void SetNodeChildren(const TArray<FEGQuestEdge>& InChildren) { Children = InChildren; }

	UFUNCTION(BlueprintPure, Category = "Quest|Node")
	virtual int32 GetNumNodeChildren() const { return Children.Num(); }

	UFUNCTION(BlueprintPure, Category = "Quest|Node")
	virtual const FEGQuestEdge& GetNodeChildAt(int32 EdgeIndex) const { return Children[EdgeIndex]; }

	// Adds an Edge to the end of the Children Array.
	virtual void AddNodeChild(const FEGQuestEdge& InChild) { Children.Add(InChild); }

	// Removes the Edge at the specified EdgeIndex location.
	virtual void RemoveChildAt(int32 EdgeIndex)
	{
		check(Children.IsValidIndex(EdgeIndex));
		Children.RemoveAt(EdgeIndex);
	}

	// Removes all edges/children
	virtual void RemoveAllChildren() { Children.Empty(); }

	// Gets the mutable edge/child at location EdgeIndex.
	virtual FEGQuestEdge* GetSafeMutableNodeChildAt(int32 EdgeIndex)
	{
		check(Children.IsValidIndex(EdgeIndex));
		return &Children[EdgeIndex];
	}

	// Unsafe version, can be null
	virtual FEGQuestEdge* GetMutableNodeChildAt(int32 EdgeIndex)
	{
		return Children.IsValidIndex(EdgeIndex) ? &Children[EdgeIndex] : nullptr;
	}

	// Gets the mutable Edge that corresponds to the provided TargetIndex or nullptr if nothing was found.
	virtual FEGQuestEdge* GetMutableNodeChildForTargetIndex(int32 TargetIndex);

	//
	// For the RoutePriorities
	//

	/**
	 * Does this node's Children carry routing, or ownership?
	 *
	 * True for every node whose Children the executor arbitrates over: an objective's outcome arrows,
	 * a start node's entry arrows, a custom node's outgoing arrows. False only for a stage, whose
	 * Children are ownership edges built from its objective rows and never from pins - a stage card
	 * emits no route, so it must never carry a route priority.
	 *
	 * The array below stays on this base class rather than being copied onto the three routing
	 * subclasses because the compiler, the card UI and validation would otherwise need three
	 * identical code paths for one concept - and UEGQuestNode_Custom subclasses live in games, which
	 * must not have to redeclare it. This gate is what keeps it honest: the compiler never writes,
	 * and validation never reads, priorities on a node that answers false.
	 */
	UFUNCTION(BlueprintPure, Category = "Quest|Node")
	virtual bool EmitsRoutes() const { return true; }

	/**
	 * The authored arbitration order of this node's outgoing arrows, keyed by destination GUID.
	 * Always empty when EmitsRoutes() is false.
	 *
	 * Compiler-maintained, like Children: the stage card's row UI edits it, and every compile
	 * reconciles it against the pins (entries for destinations that are still linked survive; a new
	 * link gets a new entry appended; an entry for a destination that is gone is dropped). The
	 * details panel shows it read-only for debugging.
	 */
	const TArray<FEGQuestRoutePriority>& GetRoutePriorities() const { return RoutePriorities; }
	void SetRoutePriorities(const TArray<FEGQuestRoutePriority>& InRoutePriorities) { RoutePriorities = InRoutePriorities; }

	/** The authored priority of one route, or INDEX_NONE when this node has none for it. */
	int32 FindRoutePriority(const FGuid& DestinationGuid, EEGQuestArrowOutcome Outcome) const;

	/** The highest authored priority in one outcome group, or INDEX_NONE when the group is empty. */
	int32 GetMaxRoutePriority(EEGQuestArrowOutcome Outcome) const;

	/** Updates one linked route's authored priority. Returns false when the key is absent or unchanged. */
	bool SetRoutePriority(const FGuid& DestinationGuid, EEGQuestArrowOutcome Outcome, int32 Priority);

	// Updates the value of the texts from the default values or the remappings (if any)
	virtual void UpdateTextsValuesFromDefaultsAndRemappings(
		const UEGQuestPluginSettings& Settings, bool bUpdateGraphNode = true
	);

	// Updates the namespace and key of all the texts depending on the settings
	virtual void UpdateTextsNamespacesAndKeys(const UEGQuestPluginSettings& Settings, bool bUpdateGraphNode = true);

	// Rebuilds ConstructedText
	virtual void RebuildTextArguments(bool bUpdateGraphNode = true);
	virtual void RebuildTextArgumentsFromPreview(const FText& Preview) {}

	// Formats this node's text arguments and stores the result in the Context
	// (via UEGQuestContext::SetConstructedNodeText). Must never mutate this node or the graph asset.
	virtual void RebuildConstructedText(UEGQuestContext& Context) const {}

	// Gets the text arguments for this Node (if any). Used for FText::Format
	UFUNCTION(BlueprintPure, Category = "Quest|Node")
	virtual const TArray<FEGQuestTextArgument>& GetTextArguments() const
	{
		static TArray<FEGQuestTextArgument> EmptyArray;
		return EmptyArray;
	};

	/**
	 * Gets this node's authored text, with any {arguments} intact - a node never holds a formatted
	 * string. The formatted result of one run lives on that run's UEGQuestContext, keyed by node GUID
	 * (see RebuildConstructedText). To get the text arguments call GetTextArguments.
	 */
	UFUNCTION(BlueprintPure, Category = "Quest|Node")
	virtual const FText& GetNodeText() const { return FText::GetEmpty(); }

	// Helper method to get directly the Quest (which is our parent)
	UEGQuestGraph* GetQuest() const;

	// Helper functions to get the names of some properties. Used by the QuestPluginEditor module.
	static FName GetMemberNameEnterEvents() { return GET_MEMBER_NAME_CHECKED(UEGQuestNode, EnterEvents); }
	static FName GetMemberNameChildren() { return GET_MEMBER_NAME_CHECKED(UEGQuestNode, Children); }
	static FName GetMemberNameGUID() { return GET_MEMBER_NAME_CHECKED(UEGQuestNode, NodeGUID); }
	static FName GetMemberNameRoutePriorities() { return GET_MEMBER_NAME_CHECKED(UEGQuestNode, RoutePriorities); }

	// Syncs the GraphNode Edges with our edges
	void UpdateGraphNode();

	// Fires this Node enter Events
	void FireNodeEnterEvents(UEGQuestContext& Context);

protected:
#if WITH_EDITORONLY_DATA
	// Node's Graph representation, used to get position.
	UPROPERTY(Meta = (QuestNoExport))
	TObjectPtr<UEdGraphNode> GraphNode = nullptr;

	// Used to build the change event and broadcast it
	int32 BroadcastPropertyEdgeIndexChanged = INDEX_NONE;
#endif // WITH_EDITORONLY_DATA

	// Events fired when the node is reached in the quest. Every UEGQuestEventCustom subclass is
	// offered by the picker, so an event type is just a class.
	UPROPERTY(EditAnywhere, Instanced, Category = "Node")
	TArray<TObjectPtr<UEGQuestEventCustom>> EnterEvents;

	// The Unique identifier for each Node. This is much safer than a Node Index.
	// Compile/Save Asset to generate this
	UPROPERTY(VisibleAnywhere, Category = "Node", AdvancedDisplay)
	FGuid NodeGUID;
	// NOTE: For some reason if this is named GUID the details panel does not work all the time for this, wtf unreal?

	// Edges that point to Children of this Node
	UPROPERTY(VisibleAnywhere, EditFixedSize, AdvancedDisplay, Category = "Node")
	TArray<FEGQuestEdge> Children;

	// The authored arbitration order of this node's arrows. See GetRoutePriorities/EmitsRoutes.
	// Authored data, unlike Children: the compiler reconciles it against the pins, never rebuilds it.
	UPROPERTY(VisibleAnywhere, EditFixedSize, AdvancedDisplay, Category = "Node")
	TArray<FEGQuestRoutePriority> RoutePriorities;

private:
	/** Runtime evaluator copies identify the authored checklist row; editor duplication still regenerates GUIDs. */
	void RestoreRuntimeEvaluatorGUID(const FGuid& InGuid) { NodeGUID = InGuid; }
};
