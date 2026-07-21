// Copyright Devil of the Plague. All Rights Reserved.
#include "EGQuestValidateCommandlet.h"

#include "UnrealExtendedQuest/EGQuestGraph.h"
#include "UnrealExtendedQuest/EGQuestManager.h"
#include "UnrealExtendedQuestEditor/Editor/EGQuestCompiler.h"
#include "UnrealExtendedQuest/Nodes/EGQuestNode_End.h"
#include "UnrealExtendedQuest/Nodes/EGQuestNode_Objective.h"

DEFINE_LOG_CATEGORY(LogQuestValidateCommandlet);

UEGQuestValidateCommandlet::UEGQuestValidateCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
	// Main returns the canonical quest diagnostic result. Unrelated engine/plugin startup errors
	// remain in the log but must not turn a clean quest validation into a false CI failure.
	ShowErrorCount = false;
}

int32 UEGQuestValidateCommandlet::Main(const FString& Params)
{
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> ParamVals;
	UCommandlet::ParseCommandLine(*Params, Tokens, Switches, ParamVals);
	const bool bStrict = Switches.ContainsByPredicate([](const FString& Switch)
	{
		return Switch.Equals(TEXT("Strict"), ESearchCase::IgnoreCase);
	});

	UEGQuestManager::LoadAllQuestsIntoMemory();
	const TArray<UEGQuestGraph*> Quests = UEGQuestManager::GetAllQuestsFromMemory();
	TMap<const UEGQuestGraph*, FEGQuestDiagnostics> DiagnosticsByQuest;
	for (const UEGQuestGraph* Quest : Quests)
	{
		if (Quest)
		{
			ValidateQuest(*Quest, DiagnosticsByQuest.FindOrAdd(Quest));
		}
	}
	ValidateDefinitionIdUniqueness(Quests, DiagnosticsByQuest);

	int32 ErrorCount = 0;
	int32 WarningCount = 0;
	for (const UEGQuestGraph* Quest : Quests)
	{
		if (!Quest)
		{
			continue;
		}
		const FEGQuestDiagnostics& Diagnostics = DiagnosticsByQuest.FindChecked(Quest);
		ReportDiagnostics(*Quest, Diagnostics);
		ErrorCount += Diagnostics.CountBySeverity(EEGQuestDiagnosticSeverity::Error);
		WarningCount += Diagnostics.CountBySeverity(EEGQuestDiagnosticSeverity::Warning);
	}

	UE_LOG(LogQuestValidateCommandlet, Display,
		TEXT("Quest validation complete: %d graph(s), %d error(s), %d warning(s), strict=%s"),
		Quests.Num(), ErrorCount, WarningCount, bStrict ? TEXT("true") : TEXT("false"));
	const int32 ExitCode = ErrorCount > 0 || (bStrict && WarningCount > 0) ? 1 : 0;
	// UnrealEditor-Cmd otherwise replaces Main's explicit result with accumulated startup errors
	// from unrelated modules. The quest counts above are already captured and reported.
	if (GWarn) GWarn->ClearWarningsAndErrors();
	return ExitCode;
}

bool UEGQuestValidateCommandlet::ValidateQuest(
	const UEGQuestGraph& Quest, FEGQuestDiagnostics& OutDiagnostics) const
{
	FEGQuestCompilerContext::ValidateQuest(Quest, OutDiagnostics);
	return !OutDiagnostics.HasErrors();
}

void UEGQuestValidateCommandlet::ValidateDefinitionIdUniqueness(
	const TArray<UEGQuestGraph*>& Quests,
	TMap<const UEGQuestGraph*, FEGQuestDiagnostics>& DiagnosticsByQuest) const
{
	TMap<FName, TArray<const UEGQuestGraph*>> QuestsByDefinitionId;
	for (const UEGQuestGraph* Quest : Quests)
	{
		if (Quest && !Quest->GetDefinitionId().IsNone())
		{
			QuestsByDefinitionId.FindOrAdd(Quest->GetDefinitionId()).Add(Quest);
		}
	}

	for (const TPair<FName, TArray<const UEGQuestGraph*>>& Pair : QuestsByDefinitionId)
	{
		if (Pair.Value.Num() < 2)
		{
			continue;
		}

		for (const UEGQuestGraph* Quest : Pair.Value)
		{
			TArray<FString> OtherPaths;
			for (const UEGQuestGraph* Other : Pair.Value)
			{
				if (Other != Quest)
				{
					OtherPaths.Add(Other->GetPathName());
				}
			}
			DiagnosticsByQuest.FindOrAdd(Quest).Add(
				FName(EGQuestDiagnosticRule::DuplicateDefinitionId),
				EEGQuestDiagnosticSeverity::Error,
				FGuid(),
				FText::Format(
					NSLOCTEXT("QuestValidateCommandlet", "DuplicateDefinitionId", "DefinitionId '{0}' is also claimed by {1}."),
					FText::FromName(Pair.Key), FText::FromString(FString::Join(OtherPaths, TEXT(", ")))),
				NSLOCTEXT("QuestValidateCommandlet", "DuplicateDefinitionIdFix", "Assign a unique namespaced DefinitionId to each quest."));
		}
	}
}

void UEGQuestValidateCommandlet::ReportDiagnostics(
	const UEGQuestGraph& Quest, const FEGQuestDiagnostics& Diagnostics) const
{
	for (const FEGQuestDiagnostic& Diagnostic : Diagnostics.Items)
	{
		const TCHAR* Severity = TEXT("Info");
		if (Diagnostic.Severity == EEGQuestDiagnosticSeverity::Error)
		{
			Severity = TEXT("Error");
		}
		else if (Diagnostic.Severity == EEGQuestDiagnosticSeverity::Warning)
		{
			Severity = TEXT("Warning");
		}

		const FString Element = Diagnostic.ElementGuid.IsValid()
			? Diagnostic.ElementGuid.ToString()
			: TEXT("Graph");
		const FString Fix = Diagnostic.FixHint.IsEmpty()
			? FString()
			: FString::Printf(TEXT(" Fix: %s"), *Diagnostic.FixHint.ToString());
		UE_LOG(LogQuestValidateCommandlet, Display, TEXT("%s|%s|%s|%s|%s%s"),
			Severity, *Diagnostic.RuleId.ToString(), *Quest.GetPathName(), *Element,
			*Diagnostic.Message.ToString(), *Fix);
	}
}

UEGQuestCoverageCommandlet::UEGQuestCoverageCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
	ShowErrorCount = false;
}

bool UEGQuestCoverageCommandlet::CheckCoverage(const UEGQuestGraph& Quest, TArray<FString>& OutErrors) const
{
	const TArray<UEGQuestNode*>& Nodes = Quest.GetNodes();
	TSet<int32> Reachable;
	TSet<int32> CanReachEnd;
	TArray<int32> Queue;
	for (const UEGQuestNode* Start : Quest.GetStartNodes())
		if (Start) for (const FEGQuestEdge& Edge : Start->GetNodeChildren())
			if (Nodes.IsValidIndex(Edge.TargetIndex)) Queue.AddUnique(Edge.TargetIndex);
	while (!Queue.IsEmpty())
	{
		const int32 Index = Queue[0];
		Queue.RemoveAt(0, EAllowShrinking::No);
		if (Reachable.Contains(Index) || !Nodes.IsValidIndex(Index) || !Nodes[Index]) continue;
		Reachable.Add(Index);
		for (const FEGQuestEdge& Edge : Nodes[Index]->GetNodeChildren())
			if (Nodes.IsValidIndex(Edge.TargetIndex) && !Reachable.Contains(Edge.TargetIndex)) Queue.AddUnique(Edge.TargetIndex);
	}
	int32 EndCount = 0;
	TArray<int32> ReverseQueue;
	for (int32 Index = 0; Index < Nodes.Num(); ++Index)
	{
		const UEGQuestNode* Node = Nodes[Index];
		if (!Node) continue;
		if (Node->IsA<UEGQuestNode_End>())
		{
			++EndCount;
			CanReachEnd.Add(Index);
			ReverseQueue.Add(Index);
			if (!Reachable.Contains(Index)) OutErrors.Add(FString::Printf(TEXT("unreachable End %s"), *Node->GetGUID().ToString()));
		}
		else if (Node->IsA<UEGQuestNode_Objective>() && !Reachable.Contains(Index))
			OutErrors.Add(FString::Printf(TEXT("unreachable objective %s"), *Node->GetGUID().ToString()));
	}
	if (EndCount == 0) OutErrors.Add(TEXT("graph has no End"));

	// Reverse reachability proves that every state the executor can enter has at least one authored
	// input/outcome sequence leading to an End. Forward reachability alone misses reachable cul-de-sacs.
	while (!ReverseQueue.IsEmpty())
	{
		const int32 Destination = ReverseQueue.Pop(EAllowShrinking::No);
		for (int32 Source = 0; Source < Nodes.Num(); ++Source)
		{
			if (!Nodes[Source] || CanReachEnd.Contains(Source)) continue;
			if (Nodes[Source]->GetNodeChildren().ContainsByPredicate([Destination](const FEGQuestEdge& Edge)
			{
				return Edge.IsValid() && Edge.TargetIndex == Destination;
			}))
			{
				CanReachEnd.Add(Source);
				ReverseQueue.Add(Source);
			}
		}
	}
	for (const int32 Index : Reachable)
	{
		const UEGQuestNode* Node = Nodes.IsValidIndex(Index) ? Nodes[Index] : nullptr;
		if (Node && !Node->IsA<UEGQuestNode_End>() && !CanReachEnd.Contains(Index))
			OutErrors.Add(FString::Printf(TEXT("reachable dead end %s"), *Node->GetGUID().ToString()));
	}
	for (const UEGQuestNode* Start : Quest.GetStartNodes())
	{
		if (!Start) continue;
		for (const FEGQuestEdge& Edge : Start->GetNodeChildren())
			if (Nodes.IsValidIndex(Edge.TargetIndex) && !CanReachEnd.Contains(Edge.TargetIndex))
				OutErrors.Add(FString::Printf(TEXT("entry %s cannot reach an End"), *Start->GetGUID().ToString()));
	}
	return OutErrors.IsEmpty();
}

int32 UEGQuestCoverageCommandlet::Main(const FString& Params)
{
	UEGQuestManager::LoadAllQuestsIntoMemory();
	const TArray<UEGQuestGraph*> Quests = UEGQuestManager::GetAllQuestsFromMemory();
	int32 FailureCount = 0;
	for (const UEGQuestGraph* Quest : Quests)
	{
		if (!Quest) continue;
		TArray<FString> Errors;
		CheckCoverage(*Quest, Errors);
		for (const FString& Error : Errors)
		{
			++FailureCount;
			UE_LOG(LogQuestValidateCommandlet, Error, TEXT("Coverage|%s|%s"), *Quest->GetPathName(), *Error);
		}
	}
	UE_LOG(LogQuestValidateCommandlet, Display, TEXT("Quest coverage complete: %d graph(s), %d failure(s)"), Quests.Num(), FailureCount);
	const int32 ExitCode = FailureCount == 0 ? 0 : 1;
	if (GWarn) GWarn->ClearWarningsAndErrors();
	return ExitCode;
}
