// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ExtendedAtlassianDocBlock.h"

class FJsonObject;

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

/** Primary route hosted by the unified Backlot workspace. */
enum class EExtendedAtlassianWorkspaceRoute : uint8
{
	Docs,
	Issues,
	IssueDetail,
	Board,
	Pins,
	Inbox,
};

/** Common asynchronous presentation state used by every Backlot collection/detail surface. */
enum class EExtendedAtlassianLoadState : uint8
{
	Idle,
	Loading,
	Ready,
	Empty,
	Error,
	Offline,
	PermissionDenied,
};

/** Durable target category for a Backlot pin. */
enum class EExtendedAtlassianPinKind : uint8
{
	Material,
	Level,
	Blueprint,
	Page,
};

/** Viewport annotation tool from the capture composer. */
enum class EExtendedAtlassianAnnotationKind : uint8
{
	Pin,
	Box,
	Blur,
};

/** Normalized Inbox category synthesized from Jira, Confluence, and Backlot metadata. */
enum class EExtendedAtlassianNotificationKind : uint8
{
	Mention,
	Review,
	Pin,
	Assign,
	Status,
	Comment,
	Link,
};

/** An Atlassian account, as returned by /rest/api/3/myself. */
struct FExtendedAtlassianUser
{
	FString AccountId;
	FString DisplayName;
	FString EmailAddress;
	FString AvatarUrl;
	FString Initials;
	FString AvatarBackground;
	FString AvatarForeground;

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
	FString StatusId;

	/** "new" | "indeterminate" | "done" — drives status colouring without hardcoding workflow names. */
	FString StatusCategoryKey;

	/** Stable Jira catalog id. IssueTypeName remains presentation-only. */
	FString IssueTypeId;
	FString IssueTypeName;
	FString IssueTypeIconUrl;
	/** Stable Jira catalog id. PriorityName remains presentation-only. */
	FString PriorityId;
	FString PriorityName;
	FString AssigneeAccountId;
	FString AssigneeDisplayName;
	FString ReporterDisplayName;
	TArray<FString> Labels;
	FString ParentId;
	FString ParentKey;
	FString ParentSummary;
	/** Stable epic/parent issue id. EpicName remains presentation-only. */
	FString EpicId;
	FString EpicName;
	FString EpicColor;
	FString SprintId;
	FString SprintName;
	FString Rank;
	FString AssigneeAvatarUrl;
	/** Time since entering Blocked, when derivable from provider changelog data. */
	FString RelativeBlocked;
	double Estimate = 0.0;
	int32 CommentCount = 0;
	TSet<FString> EditableFields;
	FString RelativeUpdated;
	FDateTime Created = FDateTime::MinValue();
	FDateTime Updated = FDateTime::MinValue();
	FString RelativeCreated;

	bool IsValid() const { return !Key.IsEmpty(); }
};

/** A workflow transition available on a specific issue. */
struct FExtendedAtlassianTransition
{
	FString Id;
	FString Name;
	/** Stable Jira status id reached by this transition. */
	FString ToStatusId;
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
	/** Preferred stable Jira issue-type id. */
	FString IssueTypeId;
	/** Presentation/fallback value for installations that have not discovered ids yet. */
	FString IssueTypeName;
	FString Summary;
	FString Description;

	/** Captured editor state, rendered into a code block beneath the description. */
	FString ContextBlock;

	/** Preferred stable Jira priority id; omitted when the project does not expose priority. */
	FString PriorityId;

	/** Presentation/fallback value retained for compatibility with older settings. */
	FString PriorityName;

	/** Optional Jira account id and stable parent issue id, both omitted when empty. */
	FString AssigneeAccountId;
	FString ParentId;
	/** Compatibility fallback used only when a stable parent id is unavailable. */
	FString ParentKey;

	/** Optional Jira Software estimate field discovered/configured for this project. */
	FString EstimateFieldId;
	TOptional<double> Estimate;

	TArray<FString> Labels;
};

/** Sparse Jira issue update. Set only the optionals that should be sent to Jira. */
struct FExtendedAtlassianIssueUpdate
{
	TOptional<FString> Summary;
	TOptional<FString> Description;
	TOptional<FString> IssueTypeId;
	TOptional<FString> IssueTypeName;
	TOptional<FString> PriorityId;
	TOptional<FString> PriorityName;
	TOptional<FString> AssigneeAccountId;
	TOptional<FString> ParentId;
	TOptional<FString> ParentKey;

	/** Jira Software estimate field discovered/configured for this project. */
	FString EstimateFieldId;
	TOptional<double> Estimate;
};

/** A comment on a Jira issue, with its ADF body already flattened. */
struct FExtendedAtlassianComment
{
	FString Id;
	FString ParentId;
	FString ContainerId;
	FString AuthorAccountId;
	FString AuthorDisplayName;
	FString AuthorAvatarUrl;
	FString Body;
	FString Quote;
	FString AccentColor;
	FString RelativeTime;
	FDateTime Created = FDateTime::MinValue();
	FDateTime Updated = FDateTime::MinValue();
	int32 Version = 0;
	bool bResolved = false;
	bool bInline = false;
	bool bCanEdit = false;
	bool bCanDelete = false;
	TArray<FExtendedAtlassianComment> Replies;
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

/** Versioned Confluence page content property used for Backlot companion metadata. */
struct FExtendedAtlassianContentProperty
{
	FString Id;
	FString Key;
	TSharedPtr<FJsonObject> Value;
	int32 Version = 0;

	bool IsValid() const { return !Id.IsEmpty(); }
};

/** A Confluence page. Body and Blocks are populated only when fetched individually. */
struct FExtendedAtlassianPage
{
	FString Id;
	FString Title;
	FString SpaceId;
	FString ParentId;
	FString WebUrl;
	TArray<FString> AncestorIds;
	FString AuthorAccountId;
	FString AuthorDisplayName;
	FString AuthorAvatarUrl;
	FDateTime VersionCreatedAt = FDateTime::MinValue();
	FString EditedByLabel;
	FString EditedAtLabel;
	TArray<FExtendedAtlassianUser> Contributors;
	int32 CommentCount = 0;
	FString ReviewState;
	FString OwnerAccountId;
	FString MilestoneText;
	int32 ContentPropertyVersion = 0;
	/** Jira issue keys linked from this page or its Backlot companion metadata. */
	TArray<FString> LinkedIssueKeys;

	/** Flat text, used for the source view and for search. */
	FString Body;

	/** Structured content for the document renderer. */
	TArray<FExtendedAtlassianDocBlock> Blocks;

	/** Markdown working copy, produced from storage format when the page is fetched for editing. */
	FString Markdown;

	/**
	 * Confluence version number. Required to update the page: writes carry the next number and are
	 * rejected with 409 if anyone else saved in the meantime.
	 */
	int32 Version = 0;

	/** False when the page contains constructs this plugin cannot rebuild, so editing is refused. */
	bool bCanRoundTrip = true;

	/** What blocked the round trip, for explaining the refusal rather than just disabling a button. */
	TArray<FString> RoundTripBlockers;
};

/** Jira Software board available to the connected user. */
struct FExtendedAtlassianBoard
{
	FString Id;
	FString Name;
	FString Type;
	FString ProjectKey;
	FString SelfUrl;
};

/** Jira Software sprint normalized for the Backlot sidebar and board. */
struct FExtendedAtlassianSprint
{
	FString Id;
	FString Name;
	FString State;
	FString Goal;
	FDateTime StartDate = FDateTime::MinValue();
	FDateTime EndDate = FDateTime::MinValue();
	FDateTime CompleteDate = FDateTime::MinValue();
};

/** Presentation column mapped to one or more Jira statuses. */
struct FExtendedAtlassianBoardColumn
{
	FString Id;
	FString DisplayName;
	TArray<FString> StatusIds;
	TArray<FString> StatusNames;
	int32 WipLimit = 0;
	FString AccentColor;
};

/** Capability-bearing Jira Software board configuration. */
struct FExtendedAtlassianBoardConfiguration
{
	TArray<FExtendedAtlassianBoardColumn> Columns;
	FString EstimateFieldId;
	FString EstimateFieldName;
	FString RankFieldId;

	bool CanEstimate() const { return !EstimateFieldId.IsEmpty(); }
	bool CanRank() const { return !RankFieldId.IsEmpty(); }
};

/** Epic/parent summary used by issue filters and progress bars. */
struct FExtendedAtlassianEpic
{
	FString Id;
	FString Key;
	FString Name;
	FString Color;
	int32 TotalIssues = 0;
	int32 DoneIssues = 0;
};

/** Normalized Jira/plugin activity record. */
struct FExtendedAtlassianActivity
{
	FString Id;
	FString IssueKey;
	FString ActorAccountId;
	FString ActorDisplayName;
	FString Verb;
	FString Detail;
	FDateTime Created = FDateTime::MinValue();
	FString RelativeTime;
};

/** Ordered Docs tree entry; sections and pages intentionally share the authored row model. */
struct FExtendedAtlassianDocumentTreeNode
{
	FString Id;
	FString Label;
	FString ParentId;
	int32 Depth = 0;
	int32 CommentBadge = 0;
	bool bSection = false;
	bool bExpanded = false;
};

/** Named Issues sidebar view and its provider-owned query semantics. */
struct FExtendedAtlassianIssueView
{
	FString Id;
	FString Label;
	FString DotColor;
	FString Jql;
	int32 AuthoredCount = 0;
	/** Provider-resolved membership; avoids trying to interpret arbitrary JQL in Slate. */
	TSet<FString> IssueKeys;
};

/** A comment set attached to a page or issue target. */
struct FExtendedAtlassianCommentCollection
{
	FString TargetId;
	TArray<FExtendedAtlassianComment> Comments;
};

/**
 * Jira comment companion metadata.
 *
 * Jira remains the body owner. Only authored thread-parent and presentation-resolution state
 * lives in the shared versioned Confluence property.
 */
struct FExtendedAtlassianIssueCommentMetadata
{
	FString IssueKey;
	FString CommentId;
	FString ParentId;
	bool bResolved = false;
	FDateTime Updated = FDateTime::MinValue();
};

/** Viewport thread card shown separately from ordinary Jira comments. */
struct FExtendedAtlassianIssueThread
{
	FString Id;
	FString IssueKey;
	FString AuthorAccountId;
	FString AuthorDisplayName;
	FString RelativeTime;
	FString Label;
	FString Body;
	FString AccentColor;
	bool bResolved = false;
};

/** Board sidebar team load row. */
struct FExtendedAtlassianTeamLoad
{
	FExtendedAtlassianUser User;
	double OpenPoints = 0.0;
	double Fraction = 0.0;
	FString ThresholdColor;
};

/** Fixture/live sprint summary values used without deriving contradictory authored fixture totals. */
struct FExtendedAtlassianSprintSummary
{
	FString DaysLeft;
	FString DateRange;
	FString Goal;
	int32 Done = 0;
	int32 Wip = 0;
	int32 Left = 0;
	double DoneFraction = 0.0;
	double WipFraction = 0.0;
	double BlockedFraction = 0.0;
};

/** Stable Unreal/Confluence identity carried by a collaborative Pin. */
struct FExtendedAtlassianPinTarget
{
	EExtendedAtlassianPinKind Kind = EExtendedAtlassianPinKind::Material;
	FString StableId;
	FString DisplayName;
	FString SecondaryId;

	bool IsValid() const { return !StableId.IsEmpty(); }
};

/** One message in a Pin thread. */
struct FExtendedAtlassianPinThread
{
	FString Id;
	FString AuthorAccountId;
	FString AuthorDisplayName;
	FString Body;
	FDateTime Created = FDateTime::MinValue();
	FDateTime Updated = FDateTime::MinValue();
	FString RelativeTime;
	FString LinkedLabel;
	bool bResolved = false;
};

/** Collaborative pin card and its ordered thread messages. */
struct FExtendedAtlassianPin
{
	FString Id;
	FExtendedAtlassianPinTarget Target;
	FString DisplayName;
	FString Color;
	int32 Version = 0;
	TArray<FExtendedAtlassianPinThread> Threads;
};

/** One synthesized Inbox event with per-user state applied. */
struct FExtendedAtlassianNotification
{
	FString Id;
	EExtendedAtlassianNotificationKind Kind = EExtendedAtlassianNotificationKind::Mention;
	FString Source;
	FString SourceId;
	FString ActorAccountId;
	FString ActorDisplayName;
	FString Action;
	FString Target;
	FString Quote;
	FDateTime Created = FDateTime::MinValue();
	FString RelativeTime;
	bool bRead = false;
	bool bMuted = false;
	bool bArchived = false;
};

/** Positioned annotation expressed in normalized capture-frame coordinates. */
struct FExtendedAtlassianAnnotation
{
	FString Id;
	EExtendedAtlassianAnnotationKind Kind = EExtendedAtlassianAnnotationKind::Pin;
	FVector2D NormalizedPosition = FVector2D::ZeroVector;
	FVector2D NormalizedSize = FVector2D::ZeroVector;
	int32 ColorIndex = 0;
};

/** Operation support discovered from permissions and Jira/Confluence metadata. */
struct FExtendedAtlassianCapabilities
{
	bool bCanReadIssues = false;
	bool bCanCreateIssues = false;
	bool bCanEditIssues = false;
	bool bCanDeleteIssues = false;
	bool bCanAssignIssues = false;
	bool bCanTransitionIssues = false;
	bool bCanRankIssues = false;
	bool bCanReadBoards = false;
	bool bCanReadPages = false;
	bool bCanEditPages = false;
	bool bCanDeletePages = false;
	bool bCanComment = false;
	bool bCanUseSharedMetadata = false;
};

/** Unified fixture/live snapshot consumed by the Backlot controller. */
struct FExtendedAtlassianWorkspaceSnapshot
{
	EExtendedAtlassianLoadState State = EExtendedAtlassianLoadState::Idle;
	FExtendedAtlassianError Error;
	/** Optional deterministic shell labels; production derives these from the host when empty. */
	FString ProjectFileLabel;
	FString PluginVersionLabel;
	FString PlatformLabel;
	/** Stable Confluence space identity plus its presentation label. */
	FString ConfluenceSpaceId;
	FString ConfluenceSpaceKey;
	FString ConfluenceSpaceName;
	/** True while a background refresh is in flight and the previous ready snapshot remains visible. */
	bool bRefreshing = false;
	/** True when visible content is cached because the latest refresh failed. */
	bool bStale = false;
	FExtendedAtlassianCapabilities Capabilities;
	FExtendedAtlassianUser CurrentUser;
	TArray<FExtendedAtlassianUser> People;
	TArray<FExtendedAtlassianIssueType> IssueTypes;
	TArray<FExtendedAtlassianPriority> Priorities;
	TArray<FExtendedAtlassianIssue> Issues;
	TArray<FExtendedAtlassianIssueView> IssueViews;
	TArray<FExtendedAtlassianIssueThread> IssueThreads;
	TArray<FExtendedAtlassianCommentCollection> CommentCollections;
	TArray<FExtendedAtlassianPage> Pages;
	TArray<FExtendedAtlassianDocumentTreeNode> DocumentTree;
	TArray<FExtendedAtlassianBoard> Boards;
	TArray<FExtendedAtlassianSprint> Sprints;
	/** Sprint selected by settings/discovery; avoids assuming the API's first sprint is active. */
	FString SelectedSprintId;
	FExtendedAtlassianSprintSummary SprintSummary;
	TArray<FExtendedAtlassianTeamLoad> TeamLoad;
	TArray<FExtendedAtlassianBoardColumn> BoardColumns;
	TArray<FExtendedAtlassianEpic> Epics;
	TArray<FExtendedAtlassianActivity> Activity;
	TArray<FExtendedAtlassianPin> Pins;
	TArray<FExtendedAtlassianNotification> Notifications;
	/** Successful provider reconciliation time used by SYNCED age presentation. */
	FDateTime SyncedAt = FDateTime::MinValue();
};
