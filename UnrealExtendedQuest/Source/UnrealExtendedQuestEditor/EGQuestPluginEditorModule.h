// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "IEGQuestPluginEditorModule.h"
#include "Templates/SharedPointer.h"
#include "AssetTypeCategories.h"
#include "IAssetTypeActions.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/Docking/WorkspaceItem.h"
#include "Logging/LogMacros.h"

class FSpawnTabArgs;
class UK2Node;
class UEGQuestGraph;
struct FGraphPanelNodeFactory;
struct FGraphPanelPinFactory;
class FExtender;
class UEdGraph;

DECLARE_LOG_CATEGORY_EXTERN(LogEGQuestPluginEditor, Verbose, All)


// Implementation of the QuestPluginEditor Module
class UNREALEXTENDEDQUESTEDITOR_API FEGQuestPluginEditorModule : public IEGQuestPluginEditorModule
{
	typedef FEGQuestPluginEditorModule Self;
public:
	FEGQuestPluginEditorModule();

	//
	// IModuleInterface interface
	//

	void StartupModule() override;
	void ShutdownModule() override;

	EAssetTypeCategories::Type GetAssetCategory() const override { return QuestPluginAssetCategoryBit; }


	//
	// Own functions
	//

	// Create the menu extenders
	static TSharedRef<FExtender> CreateFileMenuExtender(
		TSharedRef<FUICommandList> Commands,
		const TArray<TSharedPtr<FUICommandInfo>>& AdditionalMenuEntries = {}
	);
	static void MapActionsForFileMenuExtender(TSharedRef<FUICommandList> Commands);

private:
	// Handle clicking on save all quests.
	static void HandleOnSaveAllQuests();

	// Handle clicking on delete all quests text files.
	static void HandleOnDeleteAllQuestsTextFiles();

	// Handle on post engine init event
	void HandleOnPostEngineInit();

	// Handle PIE events
	void HandleOnBeginPIE(bool bIsSimulating);
	void HandleOnPostPIEStarted(bool bIsSimulating);
	void HandleOnEndPIEHandle(bool bIsSimulating);
	void HandleOnAssetRegistryFilesLoaded();

	// Handle Blueprint Events
	void HandleNewObjectiveBlueprintCreated(UBlueprint* Blueprint);
	void HandleNewCustomTextArgumentBlueprintCreated(UBlueprint* Blueprint);
	void HandleNewCustomEventBlueprintCreated(UBlueprint* Blueprint);

	// Extend the Menus of the editor
	void ExtendMenu();

private:
	// The submenu type of the dialog system
	EAssetTypeCategories::Type QuestPluginAssetCategoryBit;

	// All registered asset type actions. Cached here so that we can unregister them during shutdown.
	TArray<TSharedPtr<IAssetTypeActions>> RegisteredAssetTypeActions;

	// All registered custom class layouts for the details panel. Cached here so that we can unregister them during shutdown.
	TArray<FName> RegisteredCustomClassLayouts;

	// All registered custom property layouts for the details panel. Cached here so that we can unregister them during shutdown.
	TArray<FName> RegisteredCustomPropertyTypeLayout;

	// The factory of how the nodes look.
	TSharedPtr<FGraphPanelNodeFactory> QuestGraphNodeFactory;

	// The factory of how the pins look.
	TSharedPtr<FGraphPanelPinFactory> QuestEdGraphPinFactory;

	// Level Editor commands bound from this plugin.
	TSharedPtr<FUICommandList> LevelMenuEditorCommands;

	// The Tools Quest category.
	TSharedPtr<FWorkspaceItem> ToolsQuestCategory;

	// Handlers
	FDelegateHandle OnPostEngineInitHandle;
	FDelegateHandle OnBeginPIEHandle;
	FDelegateHandle OnPostPIEStartedHandle; // after BeginPlay() has been called
	FDelegateHandle OnEndPIEHandle;

	// Flags
	bool bIsEngineInitialized = false;
};
