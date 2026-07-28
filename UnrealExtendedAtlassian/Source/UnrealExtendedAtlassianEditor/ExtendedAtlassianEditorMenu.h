// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FUICommandList;

/** Adds the Atlassian submenu to the level editor's main menu. */
class FExtendedAtlassianEditorMenu
{
public:
	void Register();
	void Unregister();

private:
	void RegisterMenus();
	void BindCommands();

	void OpenIssueBrowser();
	void RefreshIssues();
	void OpenConfluenceBrowser();
	void OpenSettings();

	TSharedPtr<FUICommandList> CommandList;
};
