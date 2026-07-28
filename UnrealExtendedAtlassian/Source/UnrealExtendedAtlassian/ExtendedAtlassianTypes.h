// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ExtendedAtlassianDocBlock.h"

/** Result of the most recent credential verification. */
enum class EExtendedAtlassianAuthState : uint8
{
	/** No credentials have been stored on this machine. */
	NotConfigured,
	/** Credentials exist but have never been checked against the site. */
	Unverified,
	/** The last check against /rest/api/3/myself succeeded. */
	Verified,
	/** The last check failed — see the accompanying error. */
	Failed,
};

/** An Atlassian account, as returned by /rest/api/3/myself. */
struct FExtendedAtlassianUser
{
	FString AccountId;
	FString DisplayName;
	FString EmailAddress;
	FString AvatarUrl;

	bool IsValid() const { return !AccountId.IsEmpty(); }
	void Reset() { *this = FExtendedAtlassianUser(); }
};

/**
 * A failed request, normalised across Jira and Confluence.
 *
 * Both products report errors differently (Jira uses errorMessages[]/errors{}, Confluence v2 uses
 * errors[].title/detail), so the client flattens whichever it finds into Message.
 */
struct FExtendedAtlassianError
{
	/** HTTP status, or 0 when the request never reached the server. */
	int32 HttpStatus = 0;

	/** Stable machine-readable classification: Network, Unauthorized, Forbidden, NotFound, RateLimited, ServerError, BadRequest, Http<code>. */
	FString Code;

	/** Human-readable text suitable for showing in the editor. */
	FString Message;

	/** True when re-sending the identical request could plausibly succeed. */
	bool bRetryable = false;

	bool IsSet() const { return !Code.IsEmpty(); }
	void Reset() { *this = FExtendedAtlassianError(); }

	FString ToString() const
	{
		if (!IsSet())
		{
			return FString();
		}
		return HttpStatus > 0
			? FString::Printf(TEXT("[%d %s] %s"), HttpStatus, *Code, *Message)
			: FString::Printf(TEXT("[%s] %s"), *Code, *Message);
	}
};

/** A Jira issue, flattened from the fields the editor UI actually shows. */
struct FExtendedAtlassianIssue
{
	FString Id;
	FString Key;
	FString Summary;

	/** Description with ADF flattened to plain text. */
	FString Description;

	FString StatusName;

	/** "new" | "indeterminate" | "done" — drives status colouring without hardcoding workflow names. */
	FString StatusCategoryKey;

	FString IssueTypeName;
	FString IssueTypeIconUrl;
	FString PriorityName;
	FString AssigneeAccountId;
	FString AssigneeDisplayName;
	FString ReporterDisplayName;
	TArray<FString> Labels;
	FDateTime Created = FDateTime::MinValue();
	FDateTime Updated = FDateTime::MinValue();

	bool IsValid() const { return !Key.IsEmpty(); }
};

/** A workflow transition available on a specific issue. */
struct FExtendedAtlassianTransition
{
	FString Id;
	FString Name;
	FString ToStatusName;
	FString ToStatusCategoryKey;
};

/** A Jira project the account can see. */
struct FExtendedAtlassianProject
{
	FString Id;
	FString Key;
	FString Name;
};

/** An issue type available in a project, from the create metadata endpoint. */
struct FExtendedAtlassianIssueType
{
	FString Id;
	FString Name;
	bool bSubtask = false;
};

/** A priority available on the site. */
struct FExtendedAtlassianPriority
{
	FString Id;
	FString Name;
};

/** Fields for a new issue. Description and ContextBlock are plain text; the client converts to ADF. */
struct FExtendedAtlassianNewIssue
{
	FString ProjectKey;
	FString IssueTypeName;
	FString Summary;
	FString Description;

	/** Captured editor state, rendered into a code block beneath the description. */
	FString ContextBlock;

	/** Optional — omitted from the request entirely when empty, since not every project exposes it. */
	FString PriorityName;

	TArray<FString> Labels;
};

/** A comment on a Jira issue, with its ADF body already flattened. */
struct FExtendedAtlassianComment
{
	FString Id;
	FString AuthorDisplayName;
	FString Body;
	FDateTime Created = FDateTime::MinValue();
};

/** A Confluence space. */
struct FExtendedAtlassianSpace
{
	FString Id;
	FString Key;
	FString Name;

	/** "global", "personal", "collaboration" or "knowledge_base". Drives grouping in the browser. */
	FString Type;

	bool IsPersonal() const { return Type.Equals(TEXT("personal"), ESearchCase::IgnoreCase); }
};

/** A Confluence page. Body and Blocks are populated only when fetched individually. */
struct FExtendedAtlassianPage
{
	FString Id;
	FString Title;
	FString SpaceId;
	FString ParentId;
	FString WebUrl;

	/** Flat text, used for the source view and for search. */
	FString Body;

	/** Structured content for the document renderer. */
	TArray<FExtendedAtlassianDocBlock> Blocks;
};
