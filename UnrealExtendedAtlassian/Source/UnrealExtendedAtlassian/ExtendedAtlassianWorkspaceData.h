// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ExtendedAtlassianTypes.h"

/** Parameters for an idempotent Backlot data refresh. */
struct FExtendedAtlassianWorkspaceRequest
{
	EExtendedAtlassianWorkspaceRoute Route = EExtendedAtlassianWorkspaceRoute::Docs;
	FString SelectedPageId;
	FString SelectedIssueKey;
	FString SelectedPinId;
	FString SelectedNotificationId;
	FString GlobalSearch;
	uint64 Generation = 0;
};

/** Generic mutation names understood by fixture and live providers. */
enum class EExtendedAtlassianWorkspaceMutation : uint8
{
	CreateIssue,
	UpdateIssue,
	ArchiveIssue,
	DeleteIssue,
	TransitionIssue,
	RankIssue,
	/** Atomic presentation move: optional transition followed by Jira rank reconciliation. */
	MoveIssue,
	CreateIssueComment,
	UpdateIssueComment,
	DeleteIssueComment,
	ResolveIssueComment,
	ReopenIssueComment,
	CreatePage,
	UpdatePage,
	ArchivePage,
	DeletePage,
	MovePage,
	DuplicatePage,
	CreateSection,
	RenameSection,
	DeleteSection,
	ReorderPage,
	TogglePageTask,
	CreatePageComment,
	UpdatePageComment,
	DeletePageComment,
	ResolvePageComment,
	ReopenPageComment,
	CreatePin,
	UpdatePin,
	DeletePin,
	CreatePinReply,
	UpdatePinReply,
	DeletePinReply,
	ResolvePinReply,
	MarkNotificationRead,
	MarkAllNotificationsRead,
	DismissNotification,
	ArchiveNotifications,
	MuteNotification,
	CreateCaptureIssue,
};

/** Provider-neutral mutation payload. Stable ids remain strings at the REST/metadata boundary. */
struct FExtendedAtlassianWorkspaceMutation
{
	EExtendedAtlassianWorkspaceMutation Type = EExtendedAtlassianWorkspaceMutation::UpdateIssue;
	FString TargetId;
	FString ParentId;
	TMap<FString, FString> Fields;
	TArray<FString> OrderedIds;
	TArray<uint8> AttachmentBytes;
	uint64 ClientMutationId = 0;
	/** Navigate to the provider-returned result id after this mutation succeeds. */
	bool bOpenResultOnSuccess = false;
};

DECLARE_DELEGATE_TwoParams(
	FExtendedAtlassianWorkspaceLoadDelegate,
	const FExtendedAtlassianWorkspaceRequest&,
	const FExtendedAtlassianWorkspaceSnapshot&);

DECLARE_DELEGATE_FourParams(
	FExtendedAtlassianWorkspaceMutationDelegate,
	uint64,
	bool,
	const FString&,
	const FExtendedAtlassianError&);

/**
 * Service boundary for the unified Backlot workspace.
 *
 * Slate widgets never construct REST URLs or parse JSON. Production composes the existing
 * Jira/Confluence clients behind this interface; deterministic automation uses an in-memory
 * provider seeded from the frozen HTML fixture.
 */
class UNREALEXTENDEDATLASSIAN_API IExtendedAtlassianWorkspaceData
{
public:
	IExtendedAtlassianWorkspaceData();
	virtual ~IExtendedAtlassianWorkspaceData();

	virtual void Load(
		const FExtendedAtlassianWorkspaceRequest& Request,
		FExtendedAtlassianWorkspaceLoadDelegate Completion) = 0;

	virtual void Mutate(
		const FExtendedAtlassianWorkspaceMutation& Mutation,
		FExtendedAtlassianWorkspaceMutationDelegate Completion) = 0;

	virtual void CancelGeneration(uint64 Generation) = 0;
	virtual const FExtendedAtlassianCapabilities& GetCapabilities() const = 0;
	virtual bool IsFixtureProvider() const = 0;
};
