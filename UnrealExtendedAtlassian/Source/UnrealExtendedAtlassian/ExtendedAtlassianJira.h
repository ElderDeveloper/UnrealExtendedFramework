// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ExtendedAtlassianTypes.h"

class FJsonObject;

/** Outcome of a JQL search, including whether the cap truncated the result set. */
struct UNREALEXTENDEDATLASSIAN_API FExtendedAtlassianIssueQueryResult
{
	TArray<FExtendedAtlassianIssue> Issues;
	bool bSuccess = false;
	FExtendedAtlassianError Error;

	/** True when the query matched more issues than the requested cap returned. */
	bool bTruncated = false;
};

DECLARE_DELEGATE_OneParam(FExtendedAtlassianIssuesDelegate, const FExtendedAtlassianIssueQueryResult&);
DECLARE_DELEGATE_ThreeParams(FExtendedAtlassianIssueDelegate, bool /*bSuccess*/, const FExtendedAtlassianIssue&, const FExtendedAtlassianError&);
DECLARE_DELEGATE_ThreeParams(FExtendedAtlassianTransitionsDelegate, bool /*bSuccess*/, const TArray<FExtendedAtlassianTransition>&, const FExtendedAtlassianError&);
DECLARE_DELEGATE_ThreeParams(FExtendedAtlassianCommentsDelegate, bool /*bSuccess*/, const TArray<FExtendedAtlassianComment>&, const FExtendedAtlassianError&);
DECLARE_DELEGATE_ThreeParams(FExtendedAtlassianActivitiesDelegate, bool /*bSuccess*/, const TArray<FExtendedAtlassianActivity>&, const FExtendedAtlassianError&);
DECLARE_DELEGATE_TwoParams(FExtendedAtlassianActionDelegate, bool /*bSuccess*/, const FExtendedAtlassianError&);
DECLARE_DELEGATE_ThreeParams(FExtendedAtlassianCreateIssueDelegate, bool /*bSuccess*/, const FString& /*IssueKey*/, const FExtendedAtlassianError&);
DECLARE_DELEGATE_ThreeParams(FExtendedAtlassianCreateCommentDelegate, bool /*bSuccess*/, const FString& /*CommentId*/, const FExtendedAtlassianError&);
DECLARE_DELEGATE_ThreeParams(FExtendedAtlassianIssueTypesDelegate, bool /*bSuccess*/, const TArray<FExtendedAtlassianIssueType>&, const FExtendedAtlassianError&);
DECLARE_DELEGATE_ThreeParams(FExtendedAtlassianPrioritiesDelegate, bool /*bSuccess*/, const TArray<FExtendedAtlassianPriority>&, const FExtendedAtlassianError&);
DECLARE_DELEGATE_ThreeParams(FExtendedAtlassianProjectsDelegate, bool /*bSuccess*/, const TArray<FExtendedAtlassianProject>&, const FExtendedAtlassianError&);
DECLARE_DELEGATE_ThreeParams(FExtendedAtlassianUsersDelegate, bool /*bSuccess*/, const TArray<FExtendedAtlassianUser>&, const FExtendedAtlassianError&);

/**
 * Jira Cloud REST v3 operations, layered over FExtendedAtlassianClient.
 *
 * Search transparently handles Atlassian's migration from GET /search to POST /search/jql: the
 * newer endpoint is tried first, and a 404/410 falls back to the legacy one for the rest of the
 * session. Callers never see which was used.
 *
 * Every delegate fires on the game thread.
 */
class UNREALEXTENDEDATLASSIAN_API FExtendedAtlassianJira
{
public:
	/** Runs a JQL query, following pagination until MaxResults is reached or results run out. */
	static void SearchIssues(const FString& Jql, int32 MaxResults, FExtendedAtlassianIssuesDelegate OnComplete);

	static void GetIssue(const FString& IssueKey, FExtendedAtlassianIssueDelegate OnComplete);
	static void UpdateIssue(
		const FString& IssueKey,
		const FExtendedAtlassianIssueUpdate& Update,
		FExtendedAtlassianActionDelegate OnComplete);
	/** Builds the Jira v3 update payload used by UpdateIssue. Exposed for contract tests. */
	static FString BuildIssueUpdateBody(
		const FExtendedAtlassianIssueUpdate& Update);
	/**
	 * Archives an issue through Jira Cloud's experimental archive endpoint.
	 * Jira restricts this to supported plans and Jira/site administrators.
	 */
	static void ArchiveIssue(const FString& IssueKey, FExtendedAtlassianActionDelegate OnComplete);
	static void DeleteIssue(const FString& IssueKey, FExtendedAtlassianActionDelegate OnComplete);
	static void GetTransitions(const FString& IssueKey, FExtendedAtlassianTransitionsDelegate OnComplete);
	static void TransitionIssue(const FString& IssueKey, const FString& TransitionId, FExtendedAtlassianActionDelegate OnComplete);
	static void TransitionIssueToStatus(
		const FString& IssueKey,
		const FString& StatusName,
		FExtendedAtlassianActionDelegate OnComplete);
	/** Resolves an available transition by stable destination status id, with name fallback. */
	static void TransitionIssueToStatusIdOrName(
		const FString& IssueKey,
		const FString& StatusId,
		const FString& StatusName,
		FExtendedAtlassianActionDelegate OnComplete);
	static void GetComments(const FString& IssueKey, FExtendedAtlassianCommentsDelegate OnComplete);
	/** Reads every Jira changelog page for an issue, normalized newest-first. */
	static void GetChangelog(
		const FString& IssueKey,
		FExtendedAtlassianActivitiesDelegate OnComplete);
	static void AddComment(const FString& IssueKey, const FString& CommentText, FExtendedAtlassianActionDelegate OnComplete);
	static void AddCommentWithId(
		const FString& IssueKey,
		const FString& CommentText,
		FExtendedAtlassianCreateCommentDelegate OnComplete);
	static void UpdateComment(
		const FString& IssueKey,
		const FString& CommentId,
		const FString& CommentText,
		FExtendedAtlassianActionDelegate OnComplete);
	static void DeleteComment(
		const FString& IssueKey,
		const FString& CommentId,
		FExtendedAtlassianActionDelegate OnComplete);

	/** Creates an issue and returns its key. */
	static void CreateIssue(const FExtendedAtlassianNewIssue& NewIssue, FExtendedAtlassianCreateIssueDelegate OnComplete);

	/**
	 * Uploads a file to an issue.
	 *
	 * Sends multipart/form-data with the X-Atlassian-Token: no-check header that Jira requires to
	 * accept an attachment from a non-browser client.
	 */
	static void AddAttachment(
		const FString& IssueKey,
		const FString& FileName,
		const FString& ContentType,
		const TArray<uint8>& Data,
		FExtendedAtlassianActionDelegate OnComplete);

	/** Issue types available in a project. Subtask types are filtered out. */
	static void GetIssueTypes(const FString& ProjectKey, FExtendedAtlassianIssueTypesDelegate OnComplete);

	/** Priorities configured on the site. */
	static void GetPriorities(FExtendedAtlassianPrioritiesDelegate OnComplete);

	/** Users Jira says are assignable in the selected project. */
	static void GetAssignableUsers(
		const FString& ProjectKey,
		FExtendedAtlassianUsersDelegate OnComplete);

	/** Projects the account can browse, for the project-key dropdowns in settings. */
	static void GetProjects(FExtendedAtlassianProjectsDelegate OnComplete);

	/**
	 * Finds Jira issue keys in free text.
	 *
	 * Matching is restricted to known project keys on purpose: a bare [A-Z]+-[0-9]+ pattern happily
	 * matches "UTF-8" and "UE-5", which would send junk keys to the API and show phantom issues.
	 * Results are de-duplicated in first-appearance order.
	 */
	static TArray<FString> ExtractIssueKeys(const FString& Text, const TArray<FString>& ProjectKeys);

	/**
	 * Splits a comma-separated label field into labels Jira will accept.
	 *
	 * Spaces become hyphens because Jira rejects a label containing one outright, and blank entries
	 * are dropped so a trailing comma cannot fail the whole create.
	 */
	static TArray<FString> ParseLabels(const FString& CommaSeparated);

	/** Browser URL for an issue, e.g. https://site.atlassian.net/browse/TOT-123 */
	static FString GetIssueBrowseUrl(const FString& IssueKey);

	static FExtendedAtlassianIssue ParseIssue(const TSharedPtr<FJsonObject>& IssueJson);
	static FExtendedAtlassianUser ParseUser(const TSharedPtr<FJsonObject>& UserJson);
	static bool ParseChangelogPage(
		const TSharedPtr<FJsonObject>& Object,
		const FString& IssueKey,
		TArray<FExtendedAtlassianActivity>& OutActivities,
		int32& OutHistoryCount,
		int32& OutTotal,
		FExtendedAtlassianError& OutError);

	/** Jira emits +0000 offsets, which FDateTime::ParseIso8601 rejects without a colon. */
	static bool ParseJiraDateTime(const FString& In, FDateTime& Out);

	/** "POST /search/jql", "GET /search" or "unresolved" — for diagnostics and the connection check. */
	static FString GetResolvedSearchEndpointName();
};
