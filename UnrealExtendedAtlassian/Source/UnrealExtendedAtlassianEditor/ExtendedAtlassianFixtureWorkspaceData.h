// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ExtendedAtlassianWorkspaceData.h"

class FJsonObject;

/** In-memory Backlot service seeded exactly from Tests/Parity/BacklotFixture.json. */
class FExtendedAtlassianFixtureWorkspaceData final : public IExtendedAtlassianWorkspaceData
{
public:
	FExtendedAtlassianFixtureWorkspaceData();

	virtual void Load(
		const FExtendedAtlassianWorkspaceRequest& Request,
		FExtendedAtlassianWorkspaceLoadDelegate Completion) override;

	virtual void Mutate(
		const FExtendedAtlassianWorkspaceMutation& Mutation,
		FExtendedAtlassianWorkspaceMutationDelegate Completion) override;

	virtual void CancelGeneration(uint64 Generation) override;
	virtual const FExtendedAtlassianCapabilities& GetCapabilities() const override;
	virtual bool IsFixtureProvider() const override { return true; }

	bool IsValid() const { return bValid; }
	const FString& GetLoadError() const { return LoadError; }

private:
	bool LoadFixture();
	void ParseFixture(const TSharedRef<FJsonObject>& Root);
	void ApplyMutation(const FExtendedAtlassianWorkspaceMutation& Mutation);

	FExtendedAtlassianWorkspaceSnapshot Snapshot;
	TSet<uint64> CancelledGenerations;
	int32 NextIssueNumber = 1065;
	int32 NextPageNumber = 1;
	int32 NextCommentNumber = 20;
	bool bValid = false;
	FString LoadError;
};
