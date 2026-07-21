// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.

#include "EGQuestStatsCommandlet.h"
#include "UnrealExtendedQuest/EGQuestManager.h"
#include "UnrealExtendedQuest/EGQuestGraph.h"
#include "EGQuestCommandletHelper.h"
#include "UnrealExtendedQuest/Nodes/EGQuestNode_Objective.h"
#include "UnrealExtendedQuest/EGQuestHelper.h"


DEFINE_LOG_CATEGORY(LogQuestStatsCommandlet);


UEGQuestStatsCommandlet::UEGQuestStatsCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = false;
	ShowErrorCount = false;
}

int32 UEGQuestStatsCommandlet::Main(const FString& Params)
{
	UE_LOG(LogQuestStatsCommandlet, Display, TEXT("Starting"));

	// Parse command line - we're interested in the param vals
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> ParamVals;
	UCommandlet::ParseCommandLine(*Params, Tokens, Switches, ParamVals);

	UEGQuestManager::LoadAllQuestsIntoMemory();
	const TArray<UEGQuestGraph*> AllQuests = UEGQuestManager::GetAllQuestsFromMemory();

	FEGQuestStatsQuest TotalStats;
	for (const UEGQuestGraph* Quest : AllQuests)
	{
		UPackage* Package = Quest->GetOutermost();
		check(Package);
		const FString OriginalQuestPath = Package->GetPathName();

		// Only count game quests
		if (!FEGQuestHelper::IsPathInProjectDirectory(OriginalQuestPath))
		{
			UE_LOG(LogQuestStatsCommandlet, Warning, TEXT("Quest = `%s` is not in the game directory, ignoring"), *OriginalQuestPath);
			continue;
		}

		FEGQuestStatsQuest QuestStats;
		GetStatsForQuest(*Quest, QuestStats);
		TotalStats += QuestStats;
		UE_LOG(LogQuestStatsCommandlet, Display, TEXT("Quest = %s. Total Text Word count = %d"), *OriginalQuestPath, QuestStats.WordCount);
	}

	UE_LOG(LogQuestStatsCommandlet, Display,
		LINE_TERMINATOR TEXT("Stats:") LINE_TERMINATOR
		TEXT("Total Text Word Count = %d"),
		TotalStats.WordCount);

	return 0;
}


bool UEGQuestStatsCommandlet::GetStatsForQuest(const UEGQuestGraph& Quest, FEGQuestStatsQuest& OutStats)
{
	// Root
	for (const UEGQuestNode* StartNode : Quest.GetStartNodes())
	{
		OutStats.WordCount += GetNodeWordCount(*StartNode);
	}

	// Nodes
	const TArray<UEGQuestNode*>& Nodes = Quest.GetNodes();
	for (int32 NodeIndex = 0; NodeIndex < Nodes.Num(); NodeIndex++)
	{
		OutStats.WordCount += GetNodeWordCount(*Nodes[NodeIndex]);
	}

	return true;
}

int32 UEGQuestStatsCommandlet::GetNodeWordCount(const UEGQuestNode& Node) const
{
	const UEGQuestNode* NodePtr = &Node;
	int32 WordCount = 0;

	if (const UEGQuestNode_Objective* NodeObjective = Cast<UEGQuestNode_Objective>(NodePtr))
	{
		WordCount += GetTextWordCount(NodeObjective->GetNodeText());
	}
	else
	{
		// not supported
	}

	// Edges carry no text, so they contribute no words.
	return WordCount;
}

int32 UEGQuestStatsCommandlet::GetStringWordCount(const FString& String) const
{
	TArray<FString> Out;
	String.ParseIntoArray(Out, TEXT(" "), true);
	return Out.Num();
}
