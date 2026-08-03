// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "UnrealExtendedAtlassianEditor.h"

#include "ExtendedAtlassianConfluenceBrowser.h"
#include "ExtendedAtlassianClient.h"
#include "ExtendedAtlassianDocumentStyle.h"
#include "ExtendedAtlassianEditorCommands.h"
#include "ExtendedAtlassianEditorMenu.h"
#include "ExtendedAtlassianIssueBrowser.h"
#include "ExtendedAtlassianLog.h"
#include "ExtendedAtlassianNewIssueDialog.h"
#include "ExtendedAtlassianSettingsCustomization.h"
#include "SExtendedAtlassianWorkspace.h"
#include "UnrealExtendedAtlassian.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Styling/AppStyle.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "FUnrealExtendedAtlassianEditorModule"

const FName ExtendedAtlassianIssueBrowserTabName(TEXT("ExtendedAtlassianIssueBrowser"));
const FName ExtendedAtlassianConfluenceBrowserTabName(TEXT("ExtendedAtlassianConfluenceBrowser"));
const FName ExtendedAtlassianWorkspaceTabName(TEXT("ExtendedAtlassianBacklot"));

FUnrealExtendedAtlassianEditorModule* FUnrealExtendedAtlassianEditorModule::Instance = nullptr;

FUnrealExtendedAtlassianEditorModule::~FUnrealExtendedAtlassianEditorModule() = default;

void FUnrealExtendedAtlassianEditorModule::StartupModule()
{
	// Nothing here needs editor UI when running headless.
	if (IsRunningCommandlet())
	{
		return;
	}

	Instance = this;

	// ReloadCredentials already hydrated any cached verified profile. Refresh it in the
	// background so the account is immediately present without trusting stale data forever.
	if (const TSharedPtr<FExtendedAtlassianClient> Client =
		FUnrealExtendedAtlassianModule::GetClient())
	{
		Client->EnsureUserInfo();
	}

	// UFUNCTION(CallInEditor) buttons do not render or fire in the Project Settings viewer in this
	// engine build, so the Connection panel is built with Slate instead.
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	PropertyModule.RegisterCustomClassLayout(
		FExtendedAtlassianSettingsCustomization::GetCustomizedClassName(),
		FOnGetDetailCustomizationInstance::CreateStatic(&FExtendedAtlassianSettingsCustomization::MakeInstance));
	PropertyModule.NotifyCustomizationModuleChanged();

	FExtendedAtlassianDocumentStyle::Register();
	FExtendedAtlassianEditorCommands::Register();

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		ExtendedAtlassianWorkspaceTabName,
		FOnSpawnTab::CreateRaw(this, &FUnrealExtendedAtlassianEditorModule::SpawnWorkspaceTab))
		.SetDisplayName(LOCTEXT("WorkspaceTabName", "Backlot"))
		.SetTooltipText(LOCTEXT("WorkspaceTabTooltip", "Unified Jira, Confluence, Board, Pins, Inbox, and Capture workspace."))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory());

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		ExtendedAtlassianIssueBrowserTabName,
		FOnSpawnTab::CreateRaw(this, &FUnrealExtendedAtlassianEditorModule::SpawnIssueBrowserTab))
		.SetDisplayName(LOCTEXT("IssueBrowserTabName", "Jira Issues"))
		.SetTooltipText(LOCTEXT("IssueBrowserTabTooltip", "Browse and update Jira issues without leaving the editor."))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Search")));

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		ExtendedAtlassianConfluenceBrowserTabName,
		FOnSpawnTab::CreateRaw(this, &FUnrealExtendedAtlassianEditorModule::SpawnConfluenceBrowserTab))
		.SetDisplayName(LOCTEXT("ConfluenceBrowserTabName", "Confluence Docs"))
		.SetTooltipText(LOCTEXT("ConfluenceBrowserTabTooltip", "Read Confluence documentation without leaving the editor."))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Documentation")));

	EditorMenu = MakeUnique<FExtendedAtlassianEditorMenu>();
	EditorMenu->Register();

	UE_LOG(LogExtendedAtlassian, Verbose, TEXT("Extended Atlassian editor module started."));
}

void FUnrealExtendedAtlassianEditorModule::ShutdownModule()
{
	if (IsRunningCommandlet())
	{
		return;
	}

	if (EditorMenu)
	{
		EditorMenu->Unregister();
	}
	EditorMenu.Reset();

	if (FSlateApplication::IsInitialized())
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ExtendedAtlassianIssueBrowserTabName);
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ExtendedAtlassianConfluenceBrowserTabName);
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ExtendedAtlassianWorkspaceTabName);
	}

	FExtendedAtlassianEditorCommands::Unregister();
	FExtendedAtlassianDocumentStyle::Unregister();

	// The UObject system may already be torn down, so use the cached name rather than StaticClass().
	if (FModuleManager::Get().IsModuleLoaded(TEXT("PropertyEditor")))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
		PropertyModule.UnregisterCustomClassLayout(FExtendedAtlassianSettingsCustomization::GetCustomizedClassName());
		PropertyModule.NotifyCustomizationModuleChanged();
	}

	Instance = nullptr;
}

TSharedRef<SDockTab> FUnrealExtendedAtlassianEditorModule::SpawnWorkspaceTab(const FSpawnTabArgs& Args)
{
	TSharedRef<SExtendedAtlassianWorkspace> Workspace =
		SNew(SExtendedAtlassianWorkspace).StartRoute(PendingWorkspaceRoute);
	WorkspaceWidget = Workspace;

	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			Workspace
		];
}

TSharedRef<SDockTab> FUnrealExtendedAtlassianEditorModule::SpawnIssueBrowserTab(const FSpawnTabArgs& Args)
{
	TSharedRef<SExtendedAtlassianIssueBrowser> Browser = SNew(SExtendedAtlassianIssueBrowser);
	IssueBrowserWidget = Browser;

	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			Browser
		];
}

TSharedRef<SDockTab> FUnrealExtendedAtlassianEditorModule::SpawnConfluenceBrowserTab(const FSpawnTabArgs& Args)
{
	TSharedRef<SExtendedAtlassianConfluenceBrowser> Browser = SNew(SExtendedAtlassianConfluenceBrowser);
	ConfluenceBrowserWidget = Browser;

	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			Browser
		];
}

void FUnrealExtendedAtlassianEditorModule::OpenIssueBrowser()
{
	OpenWorkspace(EExtendedAtlassianWorkspaceRoute::Issues);
}

void FUnrealExtendedAtlassianEditorModule::OpenConfluenceBrowser()
{
	OpenWorkspace(EExtendedAtlassianWorkspaceRoute::Docs);
}

void FUnrealExtendedAtlassianEditorModule::OpenWorkspace(EExtendedAtlassianWorkspaceRoute Route)
{
	if (!Instance)
	{
		return;
	}

	Instance->PendingWorkspaceRoute = Route;
	FGlobalTabmanager::Get()->TryInvokeTab(ExtendedAtlassianWorkspaceTabName);
	if (const TSharedPtr<SExtendedAtlassianWorkspace> Workspace = Instance->WorkspaceWidget.Pin())
	{
		Workspace->Navigate(Route);
	}
}

void FUnrealExtendedAtlassianEditorModule::RefreshIssueBrowser()
{
	if (!Instance)
	{
		return;
	}

	if (const TSharedPtr<SExtendedAtlassianIssueBrowser> Browser = Instance->IssueBrowserWidget.Pin())
	{
		Browser->Refresh();
	}
	if (const TSharedPtr<SExtendedAtlassianWorkspace> Workspace = Instance->WorkspaceWidget.Pin())
	{
		Workspace->Refresh();
	}
}

void FUnrealExtendedAtlassianEditorModule::OpenNewIssueDialog()
{
	OpenIssueBrowser();

	if (Instance)
	{
		if (const TSharedPtr<SExtendedAtlassianIssueBrowser> Browser = Instance->IssueBrowserWidget.Pin())
		{
			Browser->OpenNewIssueDialog();
			return;
		}
	}

	// The tab could not be invoked — filing the issue still works, it just cannot be listed after.
	SExtendedAtlassianNewIssueDialog::Open();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUnrealExtendedAtlassianEditorModule, UnrealExtendedAtlassianEditor)
