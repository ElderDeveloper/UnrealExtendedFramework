// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

class FSpawnTabArgs;
class FWorkspaceItem;
class SDockTab;

/**
 * Extended Localization Workbench editor feature.
 *
 * Registers the workbench nomad tab under the shared "Extended Framework" workspace group.
 * Provides localization discovery, context, authoring, validation, and pipeline orchestration
 * on top of the standard Unreal localization pipeline (see
 * Documents/DevilOfPlagueUILabAndLocalizationWorkbenchImplementationPlan.md, Part B).
 */
class FEELocalizationWorkbenchFeature
{
public:
	static const FName TabId;

	void Register(const TSharedRef<FWorkspaceItem>& InGroup);
	void Unregister();

private:
	TSharedRef<SDockTab> SpawnTab(const FSpawnTabArgs& Args);

	bool bRegistered = false;
};

#endif // WITH_EDITOR
