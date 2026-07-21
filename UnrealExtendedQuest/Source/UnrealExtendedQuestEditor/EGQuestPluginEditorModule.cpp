// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#include "EGQuestPluginEditorModule.h"

#include "Engine/BlueprintCore.h"
#include "Templates/SharedPointer.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "WorkspaceMenuStructureModule.h"
#include "WorkspaceMenuStructure.h"
#include "Widgets/Docking/SDockTab.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "LevelEditor.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "Editor.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "K2Node_Event.h"
#include "Runtime/Launch/Resources/Version.h"

#include "Factories/EGQuestGraphFactories.h"
#include "AssetTypeActions/EGQuestGraphAssetTypeActions.h"
#include "AssetTypeActions/EGQuestBlueprintDerivedAssetTypeActions.h"
#include "EGQuestCommands.h"
#include "UnrealExtendedQuest/EGQuestConstants.h"
#include "Editor/Nodes/EGQuestGraphNode.h"
#include "Browser/SEGQuestBrowser.h"
#include "Search/EGQuestSearchManager.h"
#include "Editor/DetailsPanel/EGQuestAsset_Details.h"
#include "Editor/DetailsPanel/EGQuestGraphNode_Details.h"
#include "Editor/DetailsPanel/EGQuestNode_Details.h"
#include "Editor/DetailsPanel/EGQuestTagQuery_Details.h"
#include "Editor/DetailsPanel/EGQuestTextArgument_Details.h"
#include "UnrealExtendedQuest/EGQuestManager.h"
#include "UnrealExtendedQuest/IEGQuestPluginModule.h"

#include "UnrealExtendedQuest/Logging/EGQuestLogger.h"

#define LOCTEXT_NAMESPACE "QuestPluginEditor"

//////////////////////////////////////////////////////////////////////////
DEFINE_LOG_CATEGORY(LogEGQuestPluginEditor)
//////////////////////////////////////////////////////////////////////////

// Just some constants
static const FName QUEST_BROWSER_TAB_ID("QuestBrowser");

FEGQuestPluginEditorModule::FEGQuestPluginEditorModule() : QuestPluginAssetCategoryBit(EAssetTypeCategories::UI)
{
}

void FEGQuestPluginEditorModule::StartupModule()
{
#if NY_ENGINE_VERSION >= 424
	// Ensure custom Blueprint object types are registered before editor use.
	const FString LongName = FPackageName::ConvertToLongScriptPackageName(TEXT("UnrealExtendedQuestEditor"));
	if (UPackage* Package = Cast<UPackage>(StaticFindObjectFast(UPackage::StaticClass(), nullptr, *LongName, false)))
	{
		Package->SetPackageFlags(PKG_EditorOnly);
	}
#endif

	UE_LOG(LogEGQuestPluginEditor, Log, TEXT("QuestPluginEditorModule: StartupModule"));
	OnPostEngineInitHandle = FCoreDelegates::OnPostEngineInit.AddRaw(this, &Self::HandleOnPostEngineInit);
	OnBeginPIEHandle = FEditorDelegates::BeginPIE.AddRaw(this, &Self::HandleOnBeginPIE);
	OnPostPIEStartedHandle = FEditorDelegates::PostPIEStarted.AddRaw(this, &Self::HandleOnPostPIEStarted);
	OnEndPIEHandle = FEditorDelegates::EndPIE.AddRaw(this, &Self::HandleOnEndPIEHandle);

	// Listen for when the asset registry has finished discovering files
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(NAME_MODULE_AssetRegistry).Get();

	// Register Blueprint events
	FKismetEditorUtilities::RegisterOnBlueprintCreatedCallback(
		this,
		UEGQuestNode_Objective::StaticClass(),
		FKismetEditorUtilities::FOnBlueprintCreated::CreateRaw(this, &Self::HandleNewObjectiveBlueprintCreated)
	);
	FKismetEditorUtilities::RegisterOnBlueprintCreatedCallback(
		this,
		UEGQuestTextArgumentCustom::StaticClass(),
		FKismetEditorUtilities::FOnBlueprintCreated::CreateRaw(this, &Self::HandleNewCustomTextArgumentBlueprintCreated)
	);
	FKismetEditorUtilities::RegisterOnBlueprintCreatedCallback(
		this,
		UEGQuestEventCustom::StaticClass(),
		FKismetEditorUtilities::FOnBlueprintCreated::CreateRaw(this, &Self::HandleNewCustomEventBlueprintCreated)
	);

	// Register slate style overrides
	FEGQuestStyle::Initialize();

	// Register commands
	FEGQuestCommands::Register();

	// Register asset types, add the right click submenu
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(NAME_MODULE_AssetTools).Get();

	// Place Quest assets under Extended Framework -> Quest in filters and the create-new menu.
	QuestPluginAssetCategoryBit = AssetTools.RegisterAdvancedAssetCategory(QUEST_SYSTEM_MENU_CATEGORY_KEY, QUEST_SYSTEM_MENU_CATEGORY_KEY_TEXT);
	{
		auto Action = MakeShared<FEGQuestGraphAssetTypeActions>(QuestPluginAssetCategoryBit);
		AssetTools.RegisterAssetTypeActions(Action);
		RegisteredAssetTypeActions.Add(Action);
	}
	{
		auto Action = MakeShared<FAssetTypeActions_QuestEventCustom>(QuestPluginAssetCategoryBit);
		AssetTools.RegisterAssetTypeActions(Action);
		RegisteredAssetTypeActions.Add(Action);
	}
	{
		auto Action = MakeShared<FAssetTypeActions_QuestObjective>(QuestPluginAssetCategoryBit);
		AssetTools.RegisterAssetTypeActions(Action);
		RegisteredAssetTypeActions.Add(Action);
	}
	{
		auto Action = MakeShared<FAssetTypeActions_QuestTextArgumentCustom>(QuestPluginAssetCategoryBit);
		AssetTools.RegisterAssetTypeActions(Action);
		RegisteredAssetTypeActions.Add(Action);
	}
	// {
	// 	AssetTools.RegisterAssetTypeActions(Action);
	// 	RegisteredAssetTypeActions.Add(Action);
	// }

	// Register the details panel customizations
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(NAME_MODULE_PropertyEditor);

		// For classes:
		// NOTE Order of these two arrays must match
		TArray<FOnGetDetailCustomizationInstance> CustomClassLayouts = {
			  FOnGetDetailCustomizationInstance::CreateStatic(&FEGQuestAsset_Details::MakeInstance),
			  FOnGetDetailCustomizationInstance::CreateStatic(&FEGQuestGraphNode_Details::MakeInstance),
			  FOnGetDetailCustomizationInstance::CreateStatic(&FEGQuestNode_Details::MakeInstance)
		};
		RegisteredCustomClassLayouts = {
			UEGQuestGraph::StaticClass()->GetFName(),
			UEGQuestGraphNode::StaticClass()->GetFName(),
			UEGQuestNode::StaticClass()->GetFName()
		};
		for (int32 i = 0; i < RegisteredCustomClassLayouts.Num(); i++)
		{
			PropertyModule.RegisterCustomClassLayout(RegisteredCustomClassLayouts[i], CustomClassLayouts[i]);
		}

		// For structs:
		// NOTE Order of these two arrays must match
		TArray<FOnGetPropertyTypeCustomizationInstance> CustomPropertyTypeLayouts = {
			FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FEGQuestTextArgument_Details::MakeInstance)
		};
		RegisteredCustomPropertyTypeLayout = {
			FEGQuestTextArgument::StaticStruct()->GetFName()
		};
		for (int32 i = 0; i < RegisteredCustomPropertyTypeLayout.Num(); i++)
		{
			PropertyModule.RegisterCustomPropertyTypeLayout(RegisteredCustomPropertyTypeLayout[i], CustomPropertyTypeLayouts[i]);
		}

		PropertyModule.NotifyCustomizationModuleChanged();
	}

	// Register the thumbnail renderers
//	UThumbnailManager::Get().RegisterCustomRenderer(UEGQuestGraph::StaticClass(), UEGQuestThumbnailRenderer::StaticClass());

	// Create factories
	QuestGraphNodeFactory = MakeShared<FEGQuestGraphNodeFactory>();
	FEdGraphUtilities::RegisterVisualNodeFactory(QuestGraphNodeFactory);

	QuestEdGraphPinFactory = MakeShared<FEGQuestGraphPinFactory>();
	FEdGraphUtilities::RegisterVisualPinFactory(QuestEdGraphPinFactory);

	// Bind Editor commands
	LevelMenuEditorCommands = MakeShared<FUICommandList>();
	MapActionsForFileMenuExtender(LevelMenuEditorCommands.ToSharedRef());

	// Extend menu/toolbar
	ExtendMenu();
}

void FEGQuestPluginEditorModule::ShutdownModule()
{
	const FModuleManager& ModuleManger = FModuleManager::Get();
	if (QuestEdGraphPinFactory.IsValid())
	{
		FEdGraphUtilities::UnregisterVisualPinFactory(QuestEdGraphPinFactory);
	}

	if (QuestGraphNodeFactory.IsValid())
	{
		FEdGraphUtilities::UnregisterVisualNodeFactory(QuestGraphNodeFactory);
	}

	// Unregister the custom details panel stuff
	if (ModuleManger.IsModuleLoaded(NAME_MODULE_PropertyEditor))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(NAME_MODULE_PropertyEditor);
		for (int32 i = 0; i < RegisteredCustomClassLayouts.Num(); i++)
		{
			PropertyModule.UnregisterCustomClassLayout(RegisteredCustomClassLayouts[i]);
		}

		for (int32 i = 0; i < RegisteredCustomPropertyTypeLayout.Num(); i++)
		{
			PropertyModule.UnregisterCustomPropertyTypeLayout(RegisteredCustomPropertyTypeLayout[i]);
		}
	}
	RegisteredCustomClassLayouts.Empty();
	RegisteredCustomPropertyTypeLayout.Empty();

	// Unregister all the asset types that we registered
	if (ModuleManger.IsModuleLoaded(NAME_MODULE_AssetTools))
	{
		IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>(NAME_MODULE_AssetTools).Get();
		for (auto TypeAction : RegisteredAssetTypeActions)
		{
			AssetTools.UnregisterAssetTypeActions(TypeAction.ToSharedRef());
		}
	}
	RegisteredAssetTypeActions.Empty();

	// unregister commands
	FEGQuestCommands::Unregister();

	// Unregister slate style overrides
	FEGQuestStyle::Shutdown();

	// Unregister the Quest Browser
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(QUEST_BROWSER_TAB_ID);

	// Unregister the Quest Search
	FEGQuestSearchManager::Get()->DisableGlobalFindResults();

	if (OnBeginPIEHandle.IsValid())
	{
		FEditorDelegates::BeginPIE.Remove(OnBeginPIEHandle);
	}
	if (OnPostPIEStartedHandle.IsValid())
	{
		FEditorDelegates::PostPIEStarted.Remove(OnPostPIEStartedHandle);
	}
	if (OnEndPIEHandle.IsValid())
	{
		FEditorDelegates::EndPIE.Remove(OnEndPIEHandle);
	}
	if (OnPostEngineInitHandle.IsValid())
	{
		FCoreDelegates::OnPostEngineInit.Remove(OnPostEngineInitHandle);
	}

	UE_LOG(LogEGQuestPluginEditor, Log, TEXT("QuestPluginEditorModule: ShutdownModule"));
}

void FEGQuestPluginEditorModule::HandleOnSaveAllQuests()
{
	const EAppReturnType::Type Response = FPlatformMisc::MessageBoxExt(EAppMsgType::YesNo,
		TEXT("Save all Quest assets/files? This will save both the .uasset and the text files depending on the TextFormat from the Quest Settings."),
		TEXT("Save Quests?")
	);
	if (Response == EAppReturnType::No)
	{
		return;
	}

	if (!FEGQuestEditorUtilities::SaveAllQuests())
	{
		UE_LOG(LogEGQuestPluginEditor, Error, TEXT("Failed To save all Quests. An error occurred."));
	}
}

void FEGQuestPluginEditorModule::HandleOnDeleteAllQuestsTextFiles()
{
	const TSet<FString> AllFileExtensions = GetDefault<UEGQuestPluginSettings>()->GetAllTextFileExtensions();
	const FString StringAllFileExtensions = FString::Join(AllFileExtensions, TEXT(","));
	const FString Text = FString::Printf(
		TEXT("Delete all Quests text files? Delete all quests text files on the disk with the following extensions: %s"),
		*StringAllFileExtensions
	);

	const EAppReturnType::Type Response = FPlatformMisc::MessageBoxExt(EAppMsgType::YesNo, *Text, TEXT("Delete All Quests text files?"));
	if (Response == EAppReturnType::No)
	{
		return;
	}

	if (!FEGQuestEditorUtilities::DeleteAllQuestsTextFiles())
	{
		UE_LOG(LogEGQuestPluginEditor, Error, TEXT("Failed To delete all Quests text files. An error occurred."));
	}
}



void FEGQuestPluginEditorModule::HandleOnPostEngineInit()
{
	bIsEngineInitialized = true;
	UE_LOG(LogEGQuestPluginEditor, Log, TEXT("QuestPluginEditorModule::HandleOnPostEngineInit"));

	// Swap the engine's FGameplayTagQuery customization for our identical one: the engine's own
	// registration makes any details tree containing a tag query recurse to a stack overflow (see
	// FEGQuestTagQuery_Details). GameplayTagsEditor registers on this same delegate, earlier in the
	// subscription order, so by now its registration exists and can be replaced.
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(NAME_MODULE_PropertyEditor);
		PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("GameplayTagQuery"));
		PropertyModule.RegisterCustomPropertyTypeLayout(TEXT("GameplayTagQuery"),
			FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FEGQuestTagQuery_Details::MakeInstance));
		PropertyModule.NotifyCustomizationModuleChanged();
	}
}

void FEGQuestPluginEditorModule::HandleOnBeginPIE(bool bIsSimulating)
{
}

void FEGQuestPluginEditorModule::HandleOnPostPIEStarted(bool bIsSimulating)
{
	const UEGQuestPluginSettings* Settings = GetDefault<UEGQuestPluginSettings>();
	if (!Settings)
	{
		return;
	}

}

void FEGQuestPluginEditorModule::HandleOnEndPIEHandle(bool bIsSimulating)
{
	const UEGQuestPluginSettings* Settings = GetDefault<UEGQuestPluginSettings>();
	if (!Settings)
	{
		return;
	}

}

void FEGQuestPluginEditorModule::HandleNewObjectiveBlueprintCreated(UBlueprint* Blueprint)
{
	if (!Blueprint || Blueprint->BlueprintType != BPTYPE_Normal)
	{
		return;
	}

	// A fresh objective Blueprint opens on its evaluation entry point.
	Blueprint->bForceFullEditor = true;
	UK2Node_Event* EventNode = FEGQuestEditorUtilities::BlueprintGetOrAddEvent(
		Blueprint,
		FName(TEXT("OnQuestGameplayEvent")),
		UEGQuestNode_Objective::StaticClass()
	);
	if (EventNode)
	{
		Blueprint->LastEditedDocuments.Add(EventNode->GetGraph());
	}
}

void FEGQuestPluginEditorModule::HandleNewCustomTextArgumentBlueprintCreated(UBlueprint* Blueprint)
{
	if (!Blueprint || Blueprint->BlueprintType != BPTYPE_Normal)
	{
		return;
	}

	Blueprint->bForceFullEditor = true;
	UEdGraph* FunctionGraph = FEGQuestEditorUtilities::BlueprintGetOrAddFunction(
		Blueprint,
		GET_FUNCTION_NAME_CHECKED(UEGQuestTextArgumentCustom, GetText),
		UEGQuestTextArgumentCustom::StaticClass()
	);
	if (FunctionGraph)
	{
		Blueprint->LastEditedDocuments.Add(FunctionGraph);
	}
}

void FEGQuestPluginEditorModule::HandleNewCustomEventBlueprintCreated(UBlueprint* Blueprint)
{
	if (!Blueprint || Blueprint->BlueprintType != BPTYPE_Normal)
	{
		return;
	}

	Blueprint->bForceFullEditor = true;
	UK2Node_Event* EventNode = FEGQuestEditorUtilities::BlueprintGetOrAddEvent(
		Blueprint,
		GET_FUNCTION_NAME_CHECKED(UEGQuestEventCustom, EnterEvent),
		UEGQuestEventCustom::StaticClass()
	);
	if (EventNode)
	{
		Blueprint->LastEditedDocuments.Add(EventNode->GetGraph());
	}
}

void FEGQuestPluginEditorModule::ExtendMenu()
{
	// Running in game mode (standalone game) or dedicated server (-server) exit as we can't get the LevelEditorModule.
	if (IsRunningGame() || IsRunningCommandlet() || IsRunningDedicatedServer())
	{
		return;
	}

	// File menu extender
	{
		const TSharedRef<FExtender> FileMenuExtender = CreateFileMenuExtender(LevelMenuEditorCommands.ToSharedRef());

		// Add to the level editor
		FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>(NAME_MODULE_LevelEditor);
		LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(FileMenuExtender);
	}

	// Window -> Quest search, Quest Browse, Quest Data Display
	{
		ToolsQuestCategory = WorkspaceMenu::GetMenuStructure().GetStructureRoot()
			->AddGroup(
				LOCTEXT("WorkspaceMenu_QuestCategory", "Quest" ),
				FSlateIcon(
					FEGQuestStyle::GetStyleSetName(),
					FEGQuestStyle::PROPERTY_QuestGraphClassIcon
				),
				false
			);

		// Register the Quest Overview Browser
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(QUEST_BROWSER_TAB_ID,
			FOnSpawnTab::CreateLambda([this](const FSpawnTabArgs& Args) -> TSharedRef<SDockTab>
			{
				const TSharedRef<SDockTab> DockTab = SNew(SDockTab)
					.TabRole(ETabRole::NomadTab)
					[
						SNew(SEGQuestBrowser)
					];
				return DockTab;
			}))
			.SetDisplayName(LOCTEXT("QuestBrowserTabTitle", "Quest Browser"))
			.SetTooltipText(LOCTEXT("QuestBrowserTooltipText", "Open the Quest Overview Browser tab."))
			.SetIcon(FSlateIcon(FEGQuestStyle::GetStyleSetName(), FEGQuestStyle::PROPERTY_QuestBrowser_TabIcon))
			.SetGroup(ToolsQuestCategory.ToSharedRef());

		// Register the Quest Search
		FEGQuestSearchManager::Get()->Initialize(ToolsQuestCategory);

		// Register the Quest Data Display
		FTabSpawnerEntry* TabQuestDataDisplay = IEGQuestPluginModule::Get().GetQuestDataDisplaySpawnEntry();
		TabQuestDataDisplay->SetGroup(ToolsQuestCategory.ToSharedRef());
		TabQuestDataDisplay->SetIcon(FSlateIcon(FEGQuestStyle::GetStyleSetName(), FEGQuestStyle::PROPERTY_QuestDataDisplay_TabIcon));
	}
}

TSharedRef<FExtender> FEGQuestPluginEditorModule::CreateFileMenuExtender(
	TSharedRef<FUICommandList> Commands,
	const TArray<TSharedPtr<FUICommandInfo>>& AdditionalMenuEntries
)
{
	// Fill after the File->FileLoadAndSave
	TSharedRef<FExtender> FileMenuExtender(new FExtender);
	FileMenuExtender->AddMenuExtension(
		"FileLoadAndSave",
		EExtensionHook::After,
		Commands,
		FMenuExtensionDelegate::CreateLambda([AdditionalMenuEntries](FMenuBuilder& MenuBuilder)
		{
			// Save Quests
			MenuBuilder.BeginSection("Quest", LOCTEXT("QuestMenuKeyCategory", "Quest"));
			{
				MenuBuilder.AddMenuEntry(FEGQuestCommands::Get().SaveAllQuests);
				MenuBuilder.AddMenuEntry(FEGQuestCommands::Get().DeleteAllQuestsTextFiles);
				MenuBuilder.AddMenuSeparator();
				for (auto& MenuEntry : AdditionalMenuEntries)
				{
					MenuBuilder.AddMenuEntry(MenuEntry);
				}
			}
			MenuBuilder.EndSection();
		})
	);

	return FileMenuExtender;
}

void FEGQuestPluginEditorModule::MapActionsForFileMenuExtender(TSharedRef<FUICommandList> Commands)
{
	Commands->MapAction(
		FEGQuestCommands::Get().SaveAllQuests,
		FExecuteAction::CreateStatic(&Self::HandleOnSaveAllQuests)
	);
	Commands->MapAction(
		FEGQuestCommands::Get().DeleteAllQuestsTextFiles,
		FExecuteAction::CreateStatic(&Self::HandleOnDeleteAllQuestsTextFiles)
	);
}

#undef LOCTEXT_NAMESPACE

//////////////////////////////////////////////////////////////////////////
IMPLEMENT_MODULE(FEGQuestPluginEditorModule, UnrealExtendedQuestEditor)
//////////////////////////////////////////////////////////////////////////
