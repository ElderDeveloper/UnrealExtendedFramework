// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ExtendedAtlassianEditorCommands.h"

#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "ExtendedAtlassianEditorCommands"

FExtendedAtlassianEditorCommands::FExtendedAtlassianEditorCommands()
	: TCommands<FExtendedAtlassianEditorCommands>(
		TEXT("ExtendedAtlassianEditor"),
		LOCTEXT("ContextDescription", "Extended Atlassian"),
		NAME_None,
		FAppStyle::GetAppStyleSetName())
{
}

void FExtendedAtlassianEditorCommands::RegisterCommands()
{
	UI_COMMAND(OpenIssueBrowser, "Jira Issues", "Open the Jira issue browser.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(RefreshIssues, "Refresh Issues", "Re-run the current JQL query.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(NewIssue, "New Issue...", "Create a task, story or other work item in the configured Jira project.",
		EUserInterfaceActionType::Button, FInputChord());
#if PLATFORM_MAC
	constexpr EModifierKey::Type PrimaryModifier = EModifierKey::Command;
#else
	constexpr EModifierKey::Type PrimaryModifier = EModifierKey::Control;
#endif
	UI_COMMAND(
		ReportBug,
		"Report Bug...",
		"Capture the viewport and editor state, and file a Jira issue.",
		EUserInterfaceActionType::Button,
		FInputChord(PrimaryModifier | EModifierKey::Shift, EKeys::B));
	UI_COMMAND(
		ReportBugLegacy,
		"Report Bug (legacy shortcut)",
		"Temporary compatibility shortcut for viewport capture.",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Control | EModifierKey::Alt, EKeys::B));
	UI_COMMAND(OpenConfluenceBrowser, "Confluence Docs", "Open the Confluence documentation browser.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(OpenSettings, "Atlassian Settings", "Open the Extended Atlassian project settings.", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE
