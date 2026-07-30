// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ExtendedAtlassianJira.h"
#include "ExtendedAtlassianTypes.h"

/** Confluence v2 page footer/inline comment operations. */
class UNREALEXTENDEDATLASSIAN_API FExtendedAtlassianConfluenceComments
{
public:
	static void GetPageComments(
		const FString& PageId,
		FExtendedAtlassianCommentsDelegate OnComplete);

	/** Creates a top-level footer comment or reply when ParentCommentId is set. */
	static void CreateFooterComment(
		const FString& PageId,
		const FString& ParentCommentId,
		const FString& Body,
		FExtendedAtlassianActionDelegate OnComplete);

	static void UpdateComment(
		const FExtendedAtlassianComment& Comment,
		const FString& Body,
		FExtendedAtlassianActionDelegate OnComplete);
	static void DeleteComment(
		const FExtendedAtlassianComment& Comment,
		FExtendedAtlassianActionDelegate OnComplete);

	/** Native Confluence resolution exists only for inline comments. */
	static void SetInlineResolved(
		const FExtendedAtlassianComment& Comment,
		bool bResolved,
		FExtendedAtlassianActionDelegate OnComplete);

	/** Pure parser used by live paging, cache validation, and contract tests. */
	static FExtendedAtlassianComment ParseComment(
		const TSharedPtr<FJsonObject>& Object,
		const FString& ContainerId,
		bool bInline);
};
