// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "UnrealExtendedAtlassianEditor.h"

#include "ExtendedAtlassianConfluenceBrowser.h"
#include "ExtendedAtlassianDocumentStyle.h"
#include "ExtendedAtlassianEditorCommands.h"
#include "ExtendedAtlassianEditorMenu.h"
#include "ExtendedAtlassianIssueBrowser.h"
#include "ExtendedAtlassianLog.h"
#include "ExtendedAtlassianSettingsCustomization.h"

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
	FGlobalTabmanager::Get()->TryInvokeTab(ExtendedAtlassianIssueBrowserTabName);
}

void FUnrealExtendedAtlassianEditorModule::OpenConfluenceBrowser()
{
	FGlobalTabmanager::Get()->TryInvokeTab(ExtendedAtlassianConfluenceBrowserTabName);
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
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUnrealExtendedAtlassianEditorModule, UnrealExtendedAtlassianEditor)
