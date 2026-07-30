// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ExtendedAtlassianWorkspaceData.h"

/** Per-account, machine-local state applied to synthesized Inbox events. */
struct FExtendedAtlassianInboxUserState
{
	int32 SchemaVersion = 1;
	bool bInitialized = false;
	TSet<FString> KnownEventIds;
	TSet<FString> ReadEventIds;
	TSet<FString> MutedEventIds;
	TSet<FString> ArchivedEventIds;
	TSet<FString> DismissedEventIds;
	TMap<FString, FString> SourceCursors;
};

/**
 * Exactly-once Inbox synthesis and per-user persistence.
 *
 * Remote Jira, Confluence, and shared Pin records remain authoritative. Only
 * view state and source cursors are written under the OS user settings folder.
 */
class FExtendedAtlassianInboxState
{
public:
	static FString MakeStableEventId(
		const FString& Source,
		const FString& SourceObjectId,
		const FString& EventKind,
		const FString& Revision);

	static void SynthesizeAndApply(
		FExtendedAtlassianWorkspaceSnapshot& Snapshot,
		FExtendedAtlassianInboxUserState& UserState);

	static void ApplyMutation(
		const FExtendedAtlassianWorkspaceMutation& Mutation,
		const TArray<FExtendedAtlassianNotification>& CurrentNotifications,
		FExtendedAtlassianInboxUserState& UserState);

	static bool Load(
		const FString& AccountId,
		FExtendedAtlassianInboxUserState& OutState,
		FString& OutError);

	static bool Save(
		const FString& AccountId,
		const FExtendedAtlassianInboxUserState& State,
		FString& OutError);
	static bool Serialize(
		const FExtendedAtlassianInboxUserState& State,
		FString& OutJson,
		FString& OutError);
	static bool Deserialize(
		const FString& Json,
		FExtendedAtlassianInboxUserState& OutState,
		FString& OutError);

	static FString StatePathForAccount(const FString& AccountId);
};
