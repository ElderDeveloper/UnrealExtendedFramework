// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ExtendedAtlassianJira.h"
#include "ExtendedAtlassianTypes.h"

class FJsonObject;

DECLARE_DELEGATE_ThreeParams(FExtendedAtlassianSpacesDelegate, bool /*bSuccess*/, const TArray<FExtendedAtlassianSpace>&, const FExtendedAtlassianError&);
DECLARE_DELEGATE_ThreeParams(FExtendedAtlassianPagesDelegate, bool /*bSuccess*/, const TArray<FExtendedAtlassianPage>&, const FExtendedAtlassianError&);
DECLARE_DELEGATE_ThreeParams(FExtendedAtlassianPageDelegate, bool /*bSuccess*/, const FExtendedAtlassianPage&, const FExtendedAtlassianError&);

/**
 * Confluence Cloud operations, layered over FExtendedAtlassianClient.
 *
 * Uses the v2 API for spaces and pages, following the cursor in _links.next. Search is still only
 * available on v1, so Search() targets /wiki/rest/api/search.
 *
 * Page bodies are requested as body-format=storage, the representation supported by the
 * Confluence v2 page endpoint, then converted into the shared Markdown/block read model.
 */
class UNREALEXTENDEDATLASSIAN_API FExtendedAtlassianConfluence
{
public:
	/** Spaces the user can read, narrowed to the configured space keys when any are set. */
	static void ListSpaces(FExtendedAtlassianSpacesDelegate OnComplete);

	/** Every page in a space, flat. The browser rebuilds the hierarchy from ParentId. */
	static void ListPages(const FString& SpaceId, FExtendedAtlassianPagesDelegate OnComplete);

	/** A single page including its converted body. */
	static void GetPage(const FString& PageId, FExtendedAtlassianPageDelegate OnComplete);

	/**
	 * Fetches a page as storage format for editing.
	 *
	 * Populates Markdown, Version and the round-trip safety flags. Separate from GetPage because
	 * viewing wants the rendered body and editing needs the source of truth Confluence stores.
	 */
	static void GetPageForEditing(const FString& PageId, FExtendedAtlassianPageDelegate OnComplete);

	/** Creates a page. ParentId may be empty for a space root page. */
	static void CreatePage(
		const FString& SpaceId,
		const FString& ParentId,
		const FString& Title,
		const FString& Markdown,
		FExtendedAtlassianPageDelegate OnComplete);

	/**
	 * Updates a page.
	 *
	 * ExpectedVersion is the version the edit was based on; Confluence rejects the write if the page
	 * has moved on, which is what stops one person's save from silently discarding another's.
	 */
	static void UpdatePage(
		const FString& PageId,
		const FString& Title,
		const FString& Markdown,
		int32 ExpectedVersion,
		FExtendedAtlassianPageDelegate OnComplete);

	/** Sends a page to Confluence's archive queue. The endpoint normally accepts with HTTP 202. */
	static void ArchivePage(const FString& PageId, FExtendedAtlassianActionDelegate OnComplete);

	/** Moves a page to the trash. */
	static void DeletePage(const FString& PageId, FExtendedAtlassianActionDelegate OnComplete);

	/** CQL search. Results carry titles and URLs but no body. */
	static void Search(const FString& Cql, FExtendedAtlassianPagesDelegate OnComplete);

	/** Turns a relative _links.webui value into an absolute browser URL. */
	static FString MakeWebUrl(const FString& WebUiLink);
};
