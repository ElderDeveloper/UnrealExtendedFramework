// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ExtendedAtlassianTypes.h"

/**
 * Resolves and reveals durable Unreal Editor identities used by collaborative
 * Backlot Pins. This stays in the editor module so the runtime Atlassian model
 * never depends on Content Browser, actor selection, or editor-world APIs.
 */
class FExtendedAtlassianEditorTargetService
{
public:
	/**
	 * Resolves the current editor/page selection for a requested Pin kind.
	 *
	 * Page identity is supplied by the workspace because Confluence selection
	 * belongs to the Atlassian controller rather than Unreal Editor.
	 */
	static bool ResolveCurrentTarget(
		EExtendedAtlassianPinKind Kind,
		const FString& SelectedPageId,
		const FString& SelectedPageTitle,
		FExtendedAtlassianPinTarget& OutTarget,
		FText& OutError);

	/**
	 * Reveals an Unreal asset, level, or actor. Page navigation is deliberately
	 * handled by the workspace controller.
	 */
	static bool RevealUnrealTarget(
		const FExtendedAtlassianPinTarget& Target,
		FText& OutError);
};
