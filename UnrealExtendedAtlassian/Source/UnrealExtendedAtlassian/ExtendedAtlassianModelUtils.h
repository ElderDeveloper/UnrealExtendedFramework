// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ExtendedAtlassianTypes.h"

struct FExtendedAtlassianWorkspaceMutation;

/** Shared recursive operations for normalized workspace presentation models. */
namespace ExtendedAtlassianModelUtils
{
	UNREALEXTENDEDATLASSIAN_API FExtendedAtlassianComment* FindComment(
		TArray<FExtendedAtlassianComment>& Comments,
		const FString& CommentId);

	UNREALEXTENDEDATLASSIAN_API const FExtendedAtlassianComment* FindComment(
		const TArray<FExtendedAtlassianComment>& Comments,
		const FString& CommentId);

	UNREALEXTENDEDATLASSIAN_API bool RemoveComment(
		TArray<FExtendedAtlassianComment>& Comments,
		const FString& CommentId);

	UNREALEXTENDEDATLASSIAN_API int32 CountComments(
		const TArray<FExtendedAtlassianComment>& Comments,
		bool bIncludeReplies = true);

	UNREALEXTENDEDATLASSIAN_API int32 CountOpenComments(
		const TArray<FExtendedAtlassianComment>& Comments);

	/**
	 * Apply one provider-neutral Docs mutation to the normalized page/tree model.
	 * Returns true when the mutation belongs to the Docs domain, including a valid no-op.
	 */
	UNREALEXTENDEDATLASSIAN_API bool ApplyDocumentMutation(
		FExtendedAtlassianWorkspaceSnapshot& Snapshot,
		const FExtendedAtlassianWorkspaceMutation& Mutation);

	/** Reconcile page/issue totals and Docs tree open-comment badges from normalized collections. */
	UNREALEXTENDEDATLASSIAN_API void RefreshCommentPresentation(
		FExtendedAtlassianWorkspaceSnapshot& Snapshot);

	/**
	 * Compact age of a timestamp: "now", "12m", "5h", "2d". Empty for an unset time.
	 *
	 * Shared so Jira issues, Confluence pages and comments read the same way; it lived privately in
	 * the Jira provider, which is why the Confluence side had no relative label at all.
	 */
	UNREALEXTENDEDATLASSIAN_API FString RelativeAge(const FDateTime& Timestamp);
}
