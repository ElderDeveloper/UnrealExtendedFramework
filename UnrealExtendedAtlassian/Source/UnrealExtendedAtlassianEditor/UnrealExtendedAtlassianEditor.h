// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "ExtendedAtlassianTypes.h"
#include "Modules/ModuleManager.h"

class FExtendedAtlassianEditorMenu;
class FSpawnTabArgs;
class SDockTab;
class SExtendedAtlassianConfluenceBrowser;
class SExtendedAtlassianIssueBrowser;
class SExtendedAtlassianWorkspace;

/** Tab id of the Jira issue browser. */
extern const FName ExtendedAtlassianIssueBrowserTabName;

/** Tab id of the Confluence documentation browser. */
extern const FName ExtendedAtlassianConfluenceBrowserTabName;

/** Tab id of the unified Backlot workspace. */
extern const FName ExtendedAtlassianWorkspaceTabName;

/**
 * Editor module for the Extended Atlassian integration.
 *
 * Registers the Jira issue browser tab, the Atlassian menu, and the settings customisation that
 * handles credential entry. All Atlassian traffic is delegated to the transport module
 * (UnrealExtendedAtlassian).
 */
class UNREALEXTENDEDATLASSIANEDITOR_API FUnrealExtendedAtlassianEditorModule : public IModuleInterface
{
public:
	virtual ~FUnrealExtendedAtlassianEditorModule() override;
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** Invokes the issue browser tab, creating it if it is not already open. */
	static void OpenIssueBrowser();

	/** Re-runs the current query in the open issue browser. No-op when the tab is closed. */
	static void RefreshIssueBrowser();

	/**
	 * Opens the create form, invoking the issue browser tab first.
	 *
	 * The browser owns the post-create refresh and reports when the new issue falls outside the
	 * current query, so routing through it beats creating an issue the user cannot then see.
	 */
	static void OpenNewIssueDialog();

	/** Invokes the Confluence documentation tab, creating it if it is not already open. */
	static void OpenConfluenceBrowser();

	/** Invokes the unified Backlot tab at the requested route. */
	static void OpenWorkspace(EExtendedAtlassianWorkspaceRoute Route);

private:
	TSharedRef<SDockTab> SpawnWorkspaceTab(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnIssueBrowserTab(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnConfluenceBrowserTab(const FSpawnTabArgs& Args);

	TUniquePtr<FExtendedAtlassianEditorMenu> EditorMenu;
	TWeakPtr<SExtendedAtlassianIssueBrowser> IssueBrowserWidget;
	TWeakPtr<SExtendedAtlassianConfluenceBrowser> ConfluenceBrowserWidget;
	TWeakPtr<SExtendedAtlassianWorkspace> WorkspaceWidget;
	EExtendedAtlassianWorkspaceRoute PendingWorkspaceRoute =
		EExtendedAtlassianWorkspaceRoute::Docs;

	static FUnrealExtendedAtlassianEditorModule* Instance;
};
