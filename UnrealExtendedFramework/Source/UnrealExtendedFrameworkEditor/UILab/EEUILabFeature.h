// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

class FSpawnTabArgs;
class FWorkspaceItem;
class SDockTab;
class SEEUILabHostPanel;
class UClass;
class UEFUIFixture;
class UEFUIInteractionScript;

/**
 * Extended UI Lab editor feature.
 *
 * Registers the UI Lab nomad tab under the shared "Extended Framework" workspace group.
 * Hosts real UMG widgets in controlled fixtures without PIE (see
 * Documents/DevilOfPlagueUILabAndLocalizationWorkbenchImplementationPlan.md, Part A).
 */
class FEEUILabFeature
{
public:
	static const FName TabId;

	void Register(const TSharedRef<FWorkspaceItem>& InGroup);
	void Unregister();

	/**
	 * Integration hook (plan section 7): opens (or fronts) the UI Lab tab and hosts the given
	 * widget class with a preview culture. Used by the Localization Workbench context inspector.
	 */
	static bool OpenWidgetInLab(UClass* WidgetClass, const FString& PreviewCulture);

	/** UX-2: opens the lab and applies a saved setup (fixture). */
	static bool OpenFixtureInLab(UEFUIFixture* Fixture);

	/** UX-2: opens the lab with the Scripts panel showing the given script. */
	static bool OpenScriptInLab(UEFUIInteractionScript* Script);

private:
	TSharedRef<SDockTab> SpawnTab(const FSpawnTabArgs& Args);
	static TSharedPtr<SEEUILabHostPanel> InvokeLabPanel();

	static TWeakPtr<SEEUILabHostPanel> LastSpawnedPanel;
	FDelegateHandle ToolMenusStartupHandle;
	bool bRegistered = false;
};

#endif // WITH_EDITOR
