// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
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
 * Page bodies are requested as body-format=view — server-rendered HTML with macros already
 * expanded — falling back to storage format when a page does not offer a view rendering.
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

	/** CQL search. Results carry titles and URLs but no body. */
	static void Search(const FString& Cql, FExtendedAtlassianPagesDelegate OnComplete);

	/** Turns a relative _links.webui value into an absolute browser URL. */
	static FString MakeWebUrl(const FString& WebUiLink);
};
