// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "CoreTypes.h"
#include "Kismet2/CompilerResultsLog.h"

#include "UnrealExtendedQuest/EGQuestDiagnostics.h"
#include "UnrealExtendedQuest/EGQuestTypes.h"

class UEdGraphPin;
class UEGQuestGraphNode_Root;
class UEGQuestNode;
class UEGQuestGraph;
class UEGQuestGraphNode;
class UEGQuestPluginSettings;

/**
 * Translates the graph (the editing authority) into the runtime arrays.
 *
 * A stage card compiles into a stage node followed by its objective nodes; the stage's Children
 * become ownership edges to those objectives, and each objective's Children become routing edges
 * built from the card's outcome pins (success pin links first, then fail pin links). The root's
 * Children point at whatever its output pin links to. Nothing else holds edges.
 *
 * Arbitration order is authored, not drawn: each outcome group is emitted in RoutePriority order and
 * the start nodes in EntryPriority order. Canvas position survives in exactly one place - the
 * breadth-first walk of Step 2, which only decides what runtime *index* a node gets, and no index is
 * semantic (the executor arbitrates over Children array order and StartNodes array order, never over
 * index order). See GetOrderedChildNodes.
 *
 * The class splits in two: Compile() writes, ValidateQuest() only reads. UEGQuestGraph::IsDataValid
 * is const and must never dirty a package, so it goes through the second one.
 */
class UNREALEXTENDEDQUESTEDITOR_API FEGQuestCompilerContext
{
	friend UEGQuestGraph;

public:
	FEGQuestCompilerContext(UEGQuestGraph* InQuest, const UEGQuestPluginSettings* InSettings, FCompilerResultsLog& InMessageLog)
	: Quest(InQuest), Settings(InSettings), MessageLog(InMessageLog)
	{}

	/** Compile the Quest from its graph nodes */
	void Compile();

	/** Everything the last Compile() found. Cleared at the top of each Compile(). */
	const FEGQuestDiagnostics& GetDiagnostics() const { return Diagnostics; }

	/**
	 * The pure half: reports what a compile would report, without touching Quest.
	 *
	 * Static and taking a const Quest so the "must not mutate" contract is checked by the compiler
	 * rather than trusted. It evaluates every rule that can be evaluated by reading - which is all of
	 * them except the cross-asset DefinitionId uniqueness rule, since one graph cannot see another
	 * (that one belongs to UEGQuestValidateCommandlet).
	 *
	 * It reports the state the asset is actually in, not the state a compile would leave it in: an
	 * un-compiled asset genuinely has nodes without GUIDs and wires without priorities, and saying so
	 * is the point. Do not "helpfully" skip a rule because a compile would have fixed it.
	 */
	static void ValidateQuest(const UEGQuestGraph& Quest, FEGQuestDiagnostics& OutDiagnostics);

private:
	//
	// Compile steps, in call order.
	//

	/**
	 * Mints a GUID on every root's start node.
	 *
	 * The only place a root ever gets one. Roots are excluded from QuestGraphNodes, so they never
	 * reach AssignIndices (which is where every other node is minted), and
	 * UEGQuestGraphNode_Root::SetQuestNodeIndex is a no-op - start nodes have simply never had a
	 * GUID. Both new features need one: route priorities are keyed by destination GUID, and every
	 * diagnostic anchors to an element GUID.
	 *
	 * Runs in Step 1, right after GraphNodeRoots is populated and BEFORE OrderRootGraphNodes, so the
	 * duplicate-EntryPriority tie-break has GUIDs to fall back on.
	 */
	void MintRootGUIDs();

	/**
	 * Seeds EntryPriority from canvas X on a pre-migration asset, so the first compile after this
	 * change produces a byte-identical StartNodes array. Needs only NodePosX, so it can - and must -
	 * run before OrderRootGraphNodes, which is the sort it is feeding.
	 *
	 * Peeks UEGQuestGraph::NeedsPriorityMigration; the route seed consumes it.
	 */
	void SeedEntryPrioritiesFromLayout();

	/** Reorders start nodes by their authored EntryPriority; ties fall back to start-node GUID. */
	void OrderRootGraphNodes();

	/** Assigns this node (and a stage card's objectives) their indices in the result array. */
	void AssignIndices(UEGQuestGraphNode* GraphNode);

	/**
	 * The children of a card for the breadth-first walk, still ordered by target canvas position.
	 *
	 * Deliberately NOT converted to priority order. This runs during Step 2, before AssignIndices has
	 * minted the destinations' GUIDs, so the priorities it would need do not exist yet - and it does
	 * not need them: this order only decides which runtime index each node is assigned, and nothing
	 * reads index order. Arbitration is BuildRuntimeEdges' business.
	 */
	TArray<UEGQuestGraphNode*> GetOrderedChildNodes(const UEGQuestGraphNode* GraphNode) const;

	/**
	 * Seeds RoutePriorities from today's per-pin position sort on a pre-migration asset, so the first
	 * compile after this change produces byte-identical Children arrays.
	 *
	 * Keyed by destination GUID, which Steps 2-3 mint in AssignIndices, so this cannot share a site
	 * with the entry seed: it runs after Step 3 and before BuildRuntimeEdges. This is the site that
	 * calls UEGQuestGraph::ConsumePriorityMigration.
	 */
	void SeedRoutePrioritiesFromLayout();

	/** Rebuilds the runtime Children arrays of every emitted node from the pins, in priority order. */
	void BuildRuntimeEdges();

	//
	// Route priority.
	//

	/**
	 * The reconciled priority group for ONE pin: an entry for every destination the pin still links
	 * to - keeping the authored Priority where there is one, appending a fresh one after the group's
	 * current maximum where there is not - and nothing else.
	 *
	 * RETURNS its group instead of writing SourceNode.RoutePriorities, and that is load-bearing.
	 * BuildRuntimeEdges calls this once per pin - Success, then Fail - and each call can only see its
	 * own pin's links. A version that wrote the array directly would, on the Fail call, replace a
	 * group containing both outcomes with one containing only Fail, silently deleting every success
	 * priority the author set. The caller collects the group of every pin of one source node, merges
	 * them, and writes RoutePriorities exactly once per source node.
	 *
	 * Mutating only in that it may allocate new priorities; it does not touch the node.
	 */
	TArray<FEGQuestRoutePriority> ReconcileRoutePrioritiesForPin(
		const UEGQuestNode& SourceNode, const UEdGraphPin* Pin, EEGQuestArrowOutcome Outcome) const;

	/**
	 * The links of one pin in authored arbitration order: ascending Priority, ties broken by
	 * destination GUID so a compile is deterministic even when an author duplicated a priority
	 * (which is reported as Quest.Route.DuplicatePriority, not silently repaired - IsDataValid would
	 * disagree with the compiler if the compiler renumbered).
	 *
	 * Takes the group rather than reading SourceNode.RoutePriorities so it can be fed the reconciled
	 * group before it has been written back.
	 */
	TArray<const UEdGraphPin*> GetOrderedLinksForPin(
		const UEdGraphPin* Pin, EEGQuestArrowOutcome Outcome, const TArray<FEGQuestRoutePriority>& Group) const;

	//
	// Diagnostics.
	//

	/** Records a finding and mirrors it into MessageLog. */
	void AddDiagnostic(FName RuleId, EEGQuestDiagnosticSeverity Severity, const FGuid& ElementGuid, FText Message, FText FixHint = FText::GetEmpty());

	/** Pushes each node's diagnostics onto its graph node badge. Replaces the ad-hoc warning strings. */
	void ApplyDiagnosticsToGraphNodes();

private:
	/** The quest being compiled. */
	UEGQuestGraph* Quest = nullptr;

	// Settings we will use
	const UEGQuestPluginSettings* Settings = nullptr;

	/**
	 * Compiler message log (errors, warnings, notes).
	 *
	 * Was dead - constructed by the editor access, stored, never written. AddDiagnostic mirrors every
	 * finding into it, which is what finally makes it worth having: the quest compiler reports through
	 * the same channel as every other asset compiler in the editor.
	 */
	FCompilerResultsLog& MessageLog;

	/** Everything this compile found. The canonical result; MessageLog and the node badges are views of it. */
	FEGQuestDiagnostics Diagnostics;

	/** All the (non root) graph nodes of the Quest */
	TArray<UEGQuestGraphNode*> QuestGraphNodes;

	/** The root graph nodes. */
	TArray<UEGQuestGraphNode_Root*> GraphNodeRoots;

	/** The result of the compile. This becomes the new Quest.Nodes Array. */
	TArray<UEGQuestNode*> ResultQuestNodes;

	/** Runtime index assigned to every emitted quest node (stages, objectives, ends, customs). */
	TMap<const UEGQuestNode*, int32> NodeToIndex;

	/** All the visited graph nodes in the BFS. */
	TSet<UEGQuestGraphNode*> VisitedNodes;
};
