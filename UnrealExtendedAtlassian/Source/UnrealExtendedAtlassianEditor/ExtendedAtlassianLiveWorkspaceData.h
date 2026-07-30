// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ExtendedAtlassianInboxState.h"
#include "ExtendedAtlassianWorkspaceData.h"

/**
 * Production workspace provider composed over the existing Jira/Confluence transports.
 *
 * It normalizes both products into the same snapshot consumed by the fixture-backed parity UI.
 */
class FExtendedAtlassianLiveWorkspaceData final
	: public IExtendedAtlassianWorkspaceData
	, public TSharedFromThis<FExtendedAtlassianLiveWorkspaceData>
{
public:
	FExtendedAtlassianLiveWorkspaceData();

	virtual void Load(
		const FExtendedAtlassianWorkspaceRequest& Request,
		FExtendedAtlassianWorkspaceLoadDelegate Completion) override;
	virtual void Mutate(
		const FExtendedAtlassianWorkspaceMutation& Mutation,
		FExtendedAtlassianWorkspaceMutationDelegate Completion) override;
	virtual void CancelGeneration(uint64 Generation) override;
	virtual const FExtendedAtlassianCapabilities& GetCapabilities() const override
	{
		return Capabilities;
	}
	virtual bool IsFixtureProvider() const override { return false; }

private:
	struct FPendingLoad;

	void FinishLoadBranch(
		const TSharedRef<FPendingLoad>& Pending,
		bool bSuccess,
		const FExtendedAtlassianError& Error);
	void CompleteMutation(
		uint64 MutationId,
		const FExtendedAtlassianWorkspaceMutationDelegate& Completion,
		bool bSuccess,
		const FExtendedAtlassianError& Error) const;

	FExtendedAtlassianCapabilities Capabilities;
	TSet<uint64> CancelledGenerations;
	FString LastSpaceId;
	TArray<FExtendedAtlassianUser> LastPeople;
	TArray<FExtendedAtlassianIssue> LastIssues;
	TArray<FExtendedAtlassianPin> LastPins;
	TArray<FExtendedAtlassianIssueCommentMetadata> LastIssueCommentMetadata;
	TArray<FExtendedAtlassianNotification> LastNotifications;
	TMap<FString, FExtendedAtlassianComment> LastPageComments;
	FString InboxAccountId;
	FExtendedAtlassianInboxUserState InboxUserState;
};
