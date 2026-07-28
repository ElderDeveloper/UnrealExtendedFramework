// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ExtendedAtlassianEditorMenu.h"

#include "ExtendedAtlassianBugReportDialog.h"
#include "ExtendedAtlassianEditorCommands.h"
#include "UnrealExtendedAtlassianEditor.h"

#include "Framework/Commands/UICommandList.h"
#include "ISettingsModule.h"
#include "LevelEditor.h"
#include "Modules/ModuleManager.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "ExtendedAtlassianEditorMenu"

void FExtendedAtlassianEditorMenu::Register()
{
	BindCommands();
	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FExtendedAtlassianEditorMenu::RegisterMenus));
}

void FExtendedAtlassianEditorMenu::Unregister()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
	CommandList.Reset();
}

void FExtendedAtlassianEditorMenu::BindCommands()
{
	CommandList = MakeShared<FUICommandList>();
	const FExtendedAtlassianEditorCommands& Commands = FExtendedAtlassianEditorCommands::Get();

	CommandList->MapAction(
		Commands.OpenIssueBrowser,
		FExecuteAction::CreateRaw(this, &FExtendedAtlassianEditorMenu::OpenIssueBrowser));

	CommandList->MapAction(
		Commands.RefreshIssues,
		FExecuteAction::CreateRaw(this, &FExtendedAtlassianEditorMenu::RefreshIssues));

	CommandList->MapAction(
		Commands.ReportBug,
		FExecuteAction::CreateStatic(&SExtendedAtlassianBugReportDialog::Open));

	CommandList->MapAction(
		Commands.OpenConfluenceBrowser,
		FExecuteAction::CreateRaw(this, &FExtendedAtlassianEditorMenu::OpenConfluenceBrowser));

	CommandList->MapAction(
		Commands.OpenSettings,
		FExecuteAction::CreateRaw(this, &FExtendedAtlassianEditorMenu::OpenSettings));
}

void FExtendedAtlassianEditorMenu::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	// Bind Report Bug into the level editor's own action list so the chord still fires while the
	// viewport (including PIE) has focus, rather than only when a menu is open.
	if (FLevelEditorModule* LevelEditorModule = FModuleManager::GetModulePtr<FLevelEditorModule>(TEXT("LevelEditor")))
	{
		if (const TSharedPtr<FUICommandList> GlobalActions = LevelEditorModule->GetGlobalLevelEditorActions())
		{
			GlobalActions->MapAction(
				FExtendedAtlassianEditorCommands::Get().ReportBug,
				FExecuteAction::CreateStatic(&SExtendedAtlassianBugReportDialog::Open));
		}
	}

	UToolMenu* MainMenu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu"));
	FToolMenuSection& TopLevelSection = MainMenu->FindOrAddSection(TEXT("Window"));

	TopLevelSection.AddSubMenu(
		TEXT("Atlassian"),
		LOCTEXT("AtlassianMenuLabel", "Atlassian"),
		LOCTEXT("AtlassianMenuTooltip", "Jira and Confluence tools."),
		FNewToolMenuDelegate::CreateLambda([this](UToolMenu* InMenu)
		{
			const FExtendedAtlassianEditorCommands& Commands = FExtendedAtlassianEditorCommands::Get();

			FToolMenuSection& JiraSection = InMenu->AddSection(TEXT("Jira"), LOCTEXT("JiraSection", "Jira"));
			JiraSection.AddMenuEntryWithCommandList(Commands.ReportBug, CommandList);
			JiraSection.AddMenuEntryWithCommandList(Commands.OpenIssueBrowser, CommandList);
			JiraSection.AddMenuEntryWithCommandList(Commands.RefreshIssues, CommandList);

			FToolMenuSection& ConfluenceSection = InMenu->AddSection(TEXT("Confluence"), LOCTEXT("ConfluenceSection", "Confluence"));
			ConfluenceSection.AddMenuEntryWithCommandList(Commands.OpenConfluenceBrowser, CommandList);

			FToolMenuSection& ConfigSection = InMenu->AddSection(TEXT("Configuration"), LOCTEXT("ConfigSection", "Configuration"));
			ConfigSection.AddMenuEntryWithCommandList(Commands.OpenSettings, CommandList);
		}));
}

void FExtendedAtlassianEditorMenu::OpenIssueBrowser()
{
	FUnrealExtendedAtlassianEditorModule::OpenIssueBrowser();
}

void FExtendedAtlassianEditorMenu::RefreshIssues()
{
	FUnrealExtendedAtlassianEditorModule::RefreshIssueBrowser();
}

void FExtendedAtlassianEditorMenu::OpenConfluenceBrowser()
{
	FUnrealExtendedAtlassianEditorModule::OpenConfluenceBrowser();
}

void FExtendedAtlassianEditorMenu::OpenSettings()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>(TEXT("Settings")))
	{
		SettingsModule->ShowViewer(TEXT("Project"), TEXT("Extended Framework"), TEXT("ExtendedAtlassian"));
	}
}

#undef LOCTEXT_NAMESPACE
