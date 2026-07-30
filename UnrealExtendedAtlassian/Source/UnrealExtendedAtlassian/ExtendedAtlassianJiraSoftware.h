// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ExtendedAtlassianJira.h"
#include "ExtendedAtlassianTypes.h"

class FJsonObject;

DECLARE_DELEGATE_ThreeParams(
	FExtendedAtlassianBoardsDelegate,
	bool /*bSuccess*/,
	const TArray<FExtendedAtlassianBoard>&,
	const FExtendedAtlassianError&);
DECLARE_DELEGATE_ThreeParams(
	FExtendedAtlassianSprintsDelegate,
	bool /*bSuccess*/,
	const TArray<FExtendedAtlassianSprint>&,
	const FExtendedAtlassianError&);
DECLARE_DELEGATE_ThreeParams(
	FExtendedAtlassianBoardConfigurationDelegate,
	bool /*bSuccess*/,
	const FExtendedAtlassianBoardConfiguration&,
	const FExtendedAtlassianError&);
DECLARE_DELEGATE_FourParams(
	FExtendedAtlassianEstimateDelegate,
	bool /*bSuccess*/,
	const FString& /*Value*/,
	const FString& /*FieldId*/,
	const FExtendedAtlassianError&);

/** Jira Software REST operations kept separate from Jira Platform REST v3. */
class UNREALEXTENDEDATLASSIAN_API FExtendedAtlassianJiraSoftware
{
public:
	static void ListBoards(
		const FString& ProjectKey,
		FExtendedAtlassianBoardsDelegate OnComplete);
	static void GetBoardConfiguration(
		const FString& BoardId,
		FExtendedAtlassianBoardConfigurationDelegate OnComplete);
	static void ListSprints(
		const FString& BoardId,
		FExtendedAtlassianSprintsDelegate OnComplete);
	static void GetSprintIssues(
		const FString& SprintId,
		int32 MaxResults,
		FExtendedAtlassianIssuesDelegate OnComplete);

	/** Rank IssueKey before or after RankRelativeToKey. Exactly one direction must be true. */
	static void RankIssue(
		const FString& IssueKey,
		const FString& RankRelativeToKey,
		bool bBefore,
		FExtendedAtlassianActionDelegate OnComplete);

	/** Board-specific estimate API, used when board configuration exposes estimation. */
	static void SetIssueEstimate(
		const FString& IssueKey,
		const FString& BoardId,
		const FString& Estimate,
		FExtendedAtlassianActionDelegate OnComplete);
	static void GetIssueEstimate(
		const FString& IssueKey,
		const FString& BoardId,
		FExtendedAtlassianEstimateDelegate OnComplete);

	/** Pure response/body helpers shared by live paging and contract automation. */
	static bool ParseBoardsPage(
		const TSharedPtr<FJsonObject>& Object,
		TArray<FExtendedAtlassianBoard>& OutBoards,
		bool& bOutIsLast,
		FExtendedAtlassianError& OutError);
	static bool ParseSprintsPage(
		const TSharedPtr<FJsonObject>& Object,
		TArray<FExtendedAtlassianSprint>& OutSprints,
		bool& bOutIsLast,
		FExtendedAtlassianError& OutError);
	static bool ParseBoardConfiguration(
		const TSharedPtr<FJsonObject>& Object,
		FExtendedAtlassianBoardConfiguration& OutConfiguration,
		FExtendedAtlassianError& OutError);
	static FString BuildRankBody(
		const FString& IssueKey,
		const FString& RankRelativeToKey,
		bool bBefore);
	static FString BuildEstimateBody(const FString& Estimate);
	static bool ParseEstimate(
		const TSharedPtr<FJsonObject>& Object,
		FString& OutValue,
		FString& OutFieldId,
		FExtendedAtlassianError& OutError);
};
