// Copyright Devil of the Plague. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

/**
 * The compiler's structured result: what is wrong, where, and what to do about it.
 *
 * One list feeds all three consumers - the warning/error badge on a graph node, UEGQuestGraph::
 * IsDataValid, and the validate-all commandlet that fails CI - so a rule is authored once and
 * everything that reports it agrees.
 *
 *
 * WHY THIS LIVES IN THE RUNTIME MODULE, ENTIRELY UNDER WITH_EDITOR:
 *
 * UObject::IsDataValid is declared on the runtime class (UEGQuestGraph), so the runtime must be able
 * to name this type - it cannot live in the editor module. It is guarded because nothing outside the
 * editor produces or consumes a diagnostic: the executor reads a compiled graph and has no opinion
 * about authoring rules.
 *
 * WHY IT IS NOT A USTRUCT:
 *
 * A USTRUCT cannot be wrapped in #if WITH_EDITOR - UHT parses the declaration regardless of the
 * preprocessor and emits reflection code that would then fail to link in a non-editor build. So the
 * choice is "USTRUCT compiled into every build" or "plain struct, editor only", and nothing here
 * needs reflection: a diagnostic is never a UPROPERTY, never serialized (it is recomputed by every
 * compile), never replicated, and never exposed to Blueprint. Editor-only plain structs it is. The
 * cost is that the node UI must read these through C++ rather than a details customization, which is
 * how the existing compiler warning string already reaches the node.
 */
#if WITH_EDITOR

/** Ordered by escalation: the container's HasErrors and the commandlet's exit code compare against Error. */
enum class EEGQuestDiagnosticSeverity : uint8
{
	// Worth telling the author, never worth blocking on. Never fails validation or CI.
	Info,

	// The graph compiles and runs, but probably not the way it reads. Fails CI only under -Strict.
	Warning,

	// The graph is broken: it cannot run, or it runs arbitrarily. Fails IsDataValid and fails CI.
	Error
};

/**
 * The canonical rule list. A rule id is stable API: it appears in CI output, in suppression lists,
 * and in bug reports, so ids are added and deprecated, never repurposed or silently reworded.
 *
 * TCHAR literals rather than FName constants on purpose: a global FName has a runtime constructor
 * and would run before the name table is guaranteed to exist. Callers build the FName at the use
 * site, which is the idiom the rest of this plugin already uses for pin and property names.
 */
namespace EGQuestDiagnosticRule
{
	//
	// Graph-level. These anchor to an invalid ElementGuid - see FEGQuestDiagnostic::ElementGuid.
	//

	/** The graph has no DefinitionId. Unreachable for loaded assets (PostLoad seeds one); a graph built in code can hit it. */
	inline constexpr const TCHAR* MissingDefinitionId = TEXT("Quest.Graph.MissingDefinitionId");

	/** DefinitionId is not exactly "Namespace.Name" with both halves non-empty. */
	inline constexpr const TCHAR* MalformedDefinitionId = TEXT("Quest.Graph.MalformedDefinitionId");

	/** Two graphs claim the same DefinitionId. Cross-asset, so only the validate-all commandlet can raise it. */
	inline constexpr const TCHAR* DuplicateDefinitionId = TEXT("Quest.Graph.DuplicateDefinitionId");

	/** The graph has no start node, so nothing can ever start it. */
	inline constexpr const TCHAR* NoStartNode = TEXT("Quest.Graph.NoStartNode");

	//
	// Node-level. ElementGuid is the node's (or objective's) GUID.
	//

	/** A node reached the runtime arrays without a GUID. Routing and diagnostics both key on it. */
	inline constexpr const TCHAR* MissingNodeGUID = TEXT("Quest.Node.MissingGUID");

	/** A node no root can reach. Kept and compiled - deleting a wire must not delete authored work. */
	inline constexpr const TCHAR* OrphanNode = TEXT("Quest.Node.Orphan");

	/** A wire points at something the compile did not emit. Replaces the bare log in BuildRuntimeEdges. */
	inline constexpr const TCHAR* DanglingWire = TEXT("Quest.Node.DanglingWire");

	//
	// Start nodes. ElementGuid is the start node's GUID.
	//

	/** Two start nodes share an EntryPriority, so which one is tried first is not authored. */
	inline constexpr const TCHAR* DuplicateEntryPriority = TEXT("Quest.Start.DuplicateEntryPriority");

	/** A start node points at nothing, so it can never start the quest. */
	inline constexpr const TCHAR* StartWithoutChildren = TEXT("Quest.Start.NoChildren");
	inline constexpr const TCHAR* MainEntryCount = TEXT("Quest.Track.MainEntryCount");
	inline constexpr const TCHAR* DuplicateTrackName = TEXT("Quest.Track.DuplicateName");
	inline constexpr const TCHAR* CrossTrackRoute = TEXT("Quest.Track.CrossTrackRoute");
	inline constexpr const TCHAR* SentinelStageBudget = TEXT("Quest.Track.SentinelStageBudget");

	//
	// Routes. ElementGuid is the SOURCE node (the objective, start or custom node the arrow leaves).
	//

	/** Two arrows leaving one node on one outcome share a Priority: arbitration between them is unauthored. */
	inline constexpr const TCHAR* DuplicateRoutePriority = TEXT("Quest.Route.DuplicatePriority");

	/** An authored priority names a destination this outcome no longer routes to. Harmless, but it is rot. */
	inline constexpr const TCHAR* StaleRoutePriority = TEXT("Quest.Route.StalePriority");

	/** A wire has no authored priority. Means the asset has not been recompiled since the wire was drawn. */
	inline constexpr const TCHAR* MissingRoutePriority = TEXT("Quest.Route.MissingPriority");

	//
	// Objectives. ElementGuid is the objective's GUID.
	//

	/** The objective can fail (CanEverFail) but nothing is wired to its fail pin: failing would hang the stage. */
	inline constexpr const TCHAR* FailWithoutRoute = TEXT("Quest.Objective.FailWithoutRoute");

	/** UEGQuestNode_Objective::ValidateForCompile rejected the row. Carries its OutError as the message. */
	inline constexpr const TCHAR* InvalidObjective = TEXT("Quest.Objective.Invalid");

	/** UEGQuestEventCustom::ValidateForCompile rejected an enter event. ElementGuid is the owning node. */
	inline constexpr const TCHAR* InvalidEnterEvent = TEXT("Quest.Node.InvalidEnterEvent");
}

/** One finding. Value type: copied into node UI, the message log and the commandlet's report. */
struct FEGQuestDiagnostic
{
	/** One of EGQuestDiagnosticRule. Stable; what CI filters and suppresses on. */
	FName RuleId;

	EEGQuestDiagnosticSeverity Severity = EEGQuestDiagnosticSeverity::Error;

	/**
	 * The node, start node or objective this anchors to - which is why root GUIDs must be minted
	 * before anything can report on a start node. An INVALID guid means the finding is about the
	 * graph asset itself and belongs on the asset, not on any card.
	 */
	FGuid ElementGuid;

	/** What is wrong. Shown as-is on the node badge and in the commandlet output. */
	FText Message;

	/** What to do about it. Optional: empty when the message already says it. */
	FText FixHint;
};

/** The result of one validate or compile. Order is report order: rules are raised as they are found. */
struct FEGQuestDiagnostics
{
	TArray<FEGQuestDiagnostic> Items;

	void Add(FName RuleId, EEGQuestDiagnosticSeverity Severity, const FGuid& ElementGuid, FText Message, FText FixHint = FText::GetEmpty())
	{
		FEGQuestDiagnostic& Diagnostic = Items.AddDefaulted_GetRef();
		Diagnostic.RuleId = RuleId;
		Diagnostic.Severity = Severity;
		Diagnostic.ElementGuid = ElementGuid;
		Diagnostic.Message = MoveTemp(Message);
		Diagnostic.FixHint = MoveTemp(FixHint);
	}

	void Append(const FEGQuestDiagnostics& Other) { Items.Append(Other.Items); }

	void Reset() { Items.Reset(); }

	int32 Num() const { return Items.Num(); }

	bool HasErrors() const { return CountBySeverity(EEGQuestDiagnosticSeverity::Error) > 0; }
	bool HasWarnings() const { return CountBySeverity(EEGQuestDiagnosticSeverity::Warning) > 0; }

	int32 CountBySeverity(EEGQuestDiagnosticSeverity Severity) const
	{
		int32 Count = 0;
		for (const FEGQuestDiagnostic& Diagnostic : Items)
		{
			if (Diagnostic.Severity == Severity)
			{
				++Count;
			}
		}
		return Count;
	}

	/** Everything anchored to one element. Pass an invalid guid for the graph-level findings. */
	TArray<FEGQuestDiagnostic> FindForElement(const FGuid& ElementGuid) const
	{
		TArray<FEGQuestDiagnostic> Found;
		for (const FEGQuestDiagnostic& Diagnostic : Items)
		{
			if (Diagnostic.ElementGuid == ElementGuid)
			{
				Found.Add(Diagnostic);
			}
		}
		return Found;
	}

	/** The badge test: does this element have anything at all to say? */
	bool HasAnyForElement(const FGuid& ElementGuid) const
	{
		for (const FEGQuestDiagnostic& Diagnostic : Items)
		{
			if (Diagnostic.ElementGuid == ElementGuid)
			{
				return true;
			}
		}
		return false;
	}
};

#endif // WITH_EDITOR
