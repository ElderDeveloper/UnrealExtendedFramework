// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"

/** Commands exposed by the Extended Atlassian editor menu. */
class FExtendedAtlassianEditorCommands : public TCommands<FExtendedAtlassianEditorCommands>
{
public:
	FExtendedAtlassianEditorCommands();

	virtual void RegisterCommands() override;

	TSharedPtr<FUICommandInfo> OpenIssueBrowser;
	TSharedPtr<FUICommandInfo> RefreshIssues;
	TSharedPtr<FUICommandInfo> ReportBug;
	TSharedPtr<FUICommandInfo> OpenConfluenceBrowser;
	TSharedPtr<FUICommandInfo> OpenSettings;
};
