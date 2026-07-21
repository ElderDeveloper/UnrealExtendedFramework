// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "Commandlets/Commandlet.h"

#include "UnrealExtendedQuest/EGQuestDiagnostics.h"

#include "EGQuestValidateCommandlet.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogQuestValidateCommandlet, All, All);


class UEGQuestGraph;


/**
 * Validates every quest graph and fails the build on anything broken. The CI gate for authored
 * quests: a graph that cannot run should stop a build, not a playtest.
 *
 * Read-only. It loads quests and reports; it never compiles, mints a GUID, seeds a priority or saves
 * an asset, so it is safe to run on a clean checkout and its result does not depend on whether the
 * content happens to have been resaved. That is the same contract UEGQuestGraph::IsDataValid holds,
 * and both go through FEGQuestCompilerContext::ValidateQuest so they cannot disagree.
 *
 * Switches:
 *   -Strict   Warnings fail too. Off by default: a warning is "this probably does not read the way
 *             you think", and blocking a build on that would only teach people to stop reading them.
 *
 * Exit code: 0 when nothing failed, 1 when anything did. Nonzero is what CI keys on.
 */
UCLASS()
class UEGQuestValidateCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UEGQuestValidateCommandlet();

public:

	//~ UCommandlet interface
	int32 Main(const FString& Params) override;

	/** Per-graph rules. Forwards to FEGQuestCompilerContext::ValidateQuest. */
	bool ValidateQuest(const UEGQuestGraph& Quest, FEGQuestDiagnostics& OutDiagnostics) const;

	/**
	 * The one rule a single graph cannot check: two graphs claiming the same DefinitionId. Raises
	 * Quest.Graph.DuplicateDefinitionId on every graph in a colliding set, anchored to the graph
	 * (invalid ElementGuid) with the other asset's path in the message.
	 *
	 * Lives here rather than in ValidateQuest because IsDataValid sees one asset, and a rule that
	 * needs the whole catalog would have to load it - turning a save into a full content scan.
	 */
	void ValidateDefinitionIdUniqueness(
		const TArray<UEGQuestGraph*>& Quests,
		TMap<const UEGQuestGraph*, FEGQuestDiagnostics>& DiagnosticsByQuest) const;

	/** One line per finding, in the format CI log scrapers expect: severity, rule id, asset, element, message. */
	void ReportDiagnostics(const UEGQuestGraph& Quest, const FEGQuestDiagnostics& Diagnostics) const;
};

/** CI topology/coverage gate: every authored End and objective must be reachable from an entry. */
UCLASS()
class UEGQuestCoverageCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	UEGQuestCoverageCommandlet();
	int32 Main(const FString& Params) override;
	bool CheckCoverage(const UEGQuestGraph& Quest, TArray<FString>& OutErrors) const;
};
