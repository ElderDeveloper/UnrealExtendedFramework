// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#include "EGQuestPluginModule.h"

#include "Modules/ModuleManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetData.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "EngineUtils.h"

#if WITH_GAMEPLAY_DEBUGGER
#include "GameplayDebugger.h"
#endif // WITH_GAMEPLAY_DEBUGGER

#if WITH_EDITOR
#include "WorkspaceMenuStructureModule.h"
#include "WorkspaceMenuStructure.h"
#endif // WITH_EDITOR

#include "EGQuestConstants.h"
#include "EGQuestManager.h"
#include "EGQuestGraph.h"
#include "EGQuestComponent.h"
#include "EGQuestFactsSubsystem.h"
#include "GameplayDebugger/EGQuestGameplayDebuggerCategory.h"
#include "GameplayDebugger/SEGQuestDataDisplay.h"
#include "Logging/EGQuestLogger.h"
#include "EGQuestHelper.h"

#define LOCTEXT_NAMESPACE "FEGQuestPluginModule"

//////////////////////////////////////////////////////////////////////////
DEFINE_LOG_CATEGORY(LogEGQuestPlugin)
//////////////////////////////////////////////////////////////////////////

void FEGQuestPluginModule::StartupModule()
{
	FEGQuestLogger::OnStart();

	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	FEGQuestLogger::Get().Info(TEXT("QuestPluginModule: StartupModule"));

	OnPreLoadMapHandle = FCoreUObjectDelegates::PreLoadMap.AddRaw(this, &Self::HandleOnPreLoadMap);
	OnPostLoadMapWithWorldHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddRaw(this, &Self::HandleOnPostLoadMapWithWorld);

	// Listen for deleted assets
	// Maybe even check OnAssetRemoved if not loaded into memory?
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(NAME_MODULE_AssetRegistry).Get();
	OnInMemoryAssetDeletedHandle = AssetRegistry.OnInMemoryAssetDeleted().AddRaw(this, &Self::HandleOnInMemoryAssetDeleted);
	// NOTE: this seems to be the same as the OnInMemoryAssetDeleted as they are called from the same method inside
	// the asset registry.
	OnAssetRemovedHandle = AssetRegistry.OnAssetRemoved().AddRaw(this, &Self::HandleOnAssetRemoved);
	OnAssetRenamedHandle = AssetRegistry.OnAssetRenamed().AddRaw(this, &Self::HandleOnAssetRenamed);

#if WITH_GAMEPLAY_DEBUGGER
	// If the gameplay debugger is available, register the category and notify the editor about the changes
	IGameplayDebugger& GameplayDebuggerModule = IGameplayDebugger::Get();
	GameplayDebuggerModule.RegisterCategory(
		QUEST_SYSTEM_PLUGIN_NAME,
		IGameplayDebugger::FOnGetCategory::CreateStatic(&FEGQuestGameplayDebuggerCategory::MakeInstance),
		EGameplayDebuggerCategoryState::EnabledInGameAndSimulate
	);
	GameplayDebuggerModule.NotifyCategoriesChanged();
#endif // WITH_GAMEPLAY_DEBUGGER

	// Register tab spawners
	bHasRegisteredTabSpawners = true;

	QuestDataDisplayTabSpawnEntry = &FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		QUEST_DATA_DISPLAY_TAB_ID,
		FOnSpawnTab::CreateLambda([this](const FSpawnTabArgs& Args) -> TSharedRef<SDockTab>
		{
			TSharedRef<SDockTab> QuestDataDisplayTab = SNew(SDockTab)
				.TabRole(ETabRole::NomadTab)
				[
					GetQuestDataDisplayWindow()
				];

#if WITH_EDITOR
			const auto* IconBrush = FNYAppStyle::GetBrush(TEXT("DebugTools.TabIcon"));
			QuestDataDisplayTab->SetTabIcon(IconBrush);
#endif

			return QuestDataDisplayTab;
		}))
		.SetDisplayName(LOCTEXT("QuestDataDisplayTitle", "Quest Data Display"))
		.SetTooltipText(LOCTEXT("QuestDataDisplayTooltipText", "Open the Quest Data Display tab.")
	);
}

void FEGQuestPluginModule::ShutdownModule()
{
	// Unregister the console commands in case the user forgot to clear them
	UnregisterConsoleCommands();

	// Unregister the tab spawners
	bHasRegisteredTabSpawners = false;
	FGlobalTabmanager::Get()->UnregisterTabSpawner(QUEST_DATA_DISPLAY_TAB_ID);

#if WITH_GAMEPLAY_DEBUGGER
	// If the gameplay debugger is available, unregister the category
	if (IGameplayDebugger::IsAvailable())
	{
		IGameplayDebugger& GameplayDebuggerModule = IGameplayDebugger::Get();
		GameplayDebuggerModule.UnregisterCategory(QUEST_SYSTEM_PLUGIN_NAME);
		GameplayDebuggerModule.NotifyCategoriesChanged();
	}
#endif // WITH_GAMEPLAY_DEBUGGER

	// This function may be called during shutdown to clean up your module. For modules that support dynamic reloading,
	// we call this function before unloading the module.
	const FModuleManager& ModuleManger = FModuleManager::Get();

	// Unregister the the asset registry delete listeners
	if (ModuleManger.IsModuleLoaded(NAME_MODULE_AssetRegistry))
	{
		IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(NAME_MODULE_AssetRegistry).Get();
		if (OnInMemoryAssetDeletedHandle.IsValid())
		{
			AssetRegistry.OnInMemoryAssetDeleted().Remove(OnInMemoryAssetDeletedHandle);
		}
		if (OnAssetRemovedHandle.IsValid())
		{
			AssetRegistry.OnAssetRemoved().Remove(OnAssetRemovedHandle);
		}
		if (OnAssetRenamedHandle.IsValid())
		{
			AssetRegistry.OnAssetRenamed().Remove(OnAssetRenamedHandle);
		}
	}

	if (OnPreLoadMapHandle.IsValid())
	{
		FCoreUObjectDelegates::PreLoadMap.Remove(OnPreLoadMapHandle);
	}
	if (OnPostLoadMapWithWorldHandle.IsValid())
	{
		FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(OnPostLoadMapWithWorldHandle);
	}

	FEGQuestLogger::Get().Info(TEXT("QuestPluginModule: ShutdownModule"));
	FEGQuestLogger::OnShutdown();
}

TSharedRef<SWidget> FEGQuestPluginModule::GetQuestDataDisplayWindow()
{
	TSharedPtr<SEGQuestDataDisplay> QuestData = QuestDataDisplayWidget.Pin();
	if (!QuestData.IsValid())
	{
		QuestData = SNew(SEGQuestDataDisplay, WorldContextObjectPtr);
		QuestDataDisplayWidget = QuestData;
	}

	return QuestData.ToSharedRef();
}

FTabSpawnerEntry* FEGQuestPluginModule::GetQuestDataDisplaySpawnEntry()
{
	return QuestDataDisplayTabSpawnEntry;
}

void FEGQuestPluginModule::RegisterConsoleCommands(const TWeakObjectPtr<const UObject>& InWorldContextObjectPtr)
{
	// Unregister first to prevent double register of commands
	UnregisterConsoleCommands();

	if (InWorldContextObjectPtr.IsValid())
	{
		WorldContextObjectPtr = InWorldContextObjectPtr;
	}

	IConsoleManager& ConsoleManager = IConsoleManager::Get();
	ConsoleCommands.Add(
		ConsoleManager.RegisterConsoleCommand(
			TEXT("Quest.DataDisplay"),
			TEXT("Displays the Quest Data Window"),
			FConsoleCommandDelegate::CreateRaw(this, &Self::DisplayQuestDataWindow),
			ECVF_Default
		)
	);

	ConsoleCommands.Add(
		ConsoleManager.RegisterConsoleCommand(
			TEXT("Quest.ListActive"),
			TEXT("Lists replicated shared/private quest snapshots and their revisions."),
			FConsoleCommandDelegate::CreateRaw(this, &Self::DumpActiveQuests),
			ECVF_Default
		)
	);

	ConsoleCommands.Add(
		ConsoleManager.RegisterConsoleCommand(
			TEXT("Quest.LoadAllQuests"),
			TEXT("Load All Quests into memory"),
			FConsoleCommandDelegate::CreateLambda([]()
			{
				UEGQuestManager::LoadAllQuestsIntoMemory();
			}),
			ECVF_Default
		)
	);

#if !UE_BUILD_SHIPPING
	// World-aware cheats for multiplayer testing. The world-args delegate variant receives the
	// world the command was entered in, so these behave correctly in PIE with several worlds.
	ConsoleCommands.Add(
		ConsoleManager.RegisterConsoleCommand(
			TEXT("Quest.SendEvent"),
			TEXT("Quest.SendEvent <GameplayTag> [Magnitude] - publish a quest gameplay event to every quest component (authority only)."),
			FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Self::CheatSendQuestEvent),
			ECVF_Cheat
		)
	);
	ConsoleCommands.Add(
		ConsoleManager.RegisterConsoleCommand(
			TEXT("Quest.CompleteObjective"),
			TEXT("Quest.CompleteObjective <QuestInstanceGuid> [ObjectiveGuid] - complete an objective of that quest's active stage; without an ObjectiveGuid the first pending one (authority only)."),
			FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Self::CheatCompleteObjective),
			ECVF_Cheat
		)
	);
	ConsoleCommands.Add(ConsoleManager.RegisterConsoleCommand(TEXT("Quest.SetFact"),
		TEXT("Quest.SetFact <GameplayTag> <Value>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Self::CheatSetFact), ECVF_Cheat));
	ConsoleCommands.Add(ConsoleManager.RegisterConsoleCommand(TEXT("Quest.FailObjective"),
		TEXT("Quest.FailObjective <QuestInstanceGuid> [ObjectiveGuid]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Self::CheatFailObjective), ECVF_Cheat));
	ConsoleCommands.Add(ConsoleManager.RegisterConsoleCommand(TEXT("Quest.JumpToStage"),
		TEXT("Quest.JumpToStage <QuestInstanceGuid> <StageGuid>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Self::CheatJumpToStage), ECVF_Cheat));
#endif

	// In case the QuestDataDisplay is already opened, simply refresh the actor reference
	RefreshDisplayQuestDataWindow(false);
}

void FEGQuestPluginModule::DumpActiveQuests() const
{
	const UObject* ContextObject = WorldContextObjectPtr.Get();
	UWorld* World = ContextObject ? ContextObject->GetWorld() : LastLoadedWorld.Get();
	if (!World)
	{
		UE_LOG(LogEGQuestPlugin, Warning, TEXT("Quest.ListActive: no current world"));
		return;
	}

	int32 ComponentCount = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		const UEGQuestComponent* Component = It->FindComponentByClass<UEGQuestComponent>();
		if (!Component)
		{
			continue;
		}
		++ComponentCount;
		auto PrintSnapshots = [&It](const TCHAR* Scope, const TArray<FEGQuestRuntimeSnapshot>& Snapshots)
		{
			for (const FEGQuestRuntimeSnapshot& Snapshot : Snapshots)
			{
				UE_LOG(LogEGQuestPlugin, Display,
					TEXT("Quest.ListActive Owner=%s Scope=%s Instance=%s Asset=%s State=%d ActiveNode=%s Revision=%d"),
					*It->GetPathName(), Scope, *Snapshot.QuestInstanceGuid.ToString(),
					*Snapshot.QuestAssetId.ToString(), static_cast<int32>(Snapshot.LifecycleState),
					*Snapshot.ActiveNodeGuid.ToString(), Snapshot.Revision);
			}
		};
		PrintSnapshots(TEXT("Shared"), Component->GetSharedQuestSnapshots());
		PrintSnapshots(TEXT("Private"), Component->GetPrivateQuestSnapshots());
	}
	UE_LOG(LogEGQuestPlugin, Display, TEXT("Quest.ListActive: inspected %d quest component(s)"), ComponentCount);
}

#if !UE_BUILD_SHIPPING
void FEGQuestPluginModule::CheatSendQuestEvent(const TArray<FString>& Args, UWorld* World)
{
	if (!World || Args.Num() < 1)
	{
		UE_LOG(LogEGQuestPlugin, Warning, TEXT("Quest.SendEvent usage: Quest.SendEvent <GameplayTag> [Magnitude]"));
		return;
	}
	FEGQuestGameplayEvent Event;
	Event.EventTag = FGameplayTag::RequestGameplayTag(FName(*Args[0]), false);
	if (!Event.EventTag.IsValid())
	{
		UE_LOG(LogEGQuestPlugin, Warning, TEXT("Quest.SendEvent: unknown gameplay tag '%s'"), *Args[0]);
		return;
	}
	if (Args.Num() >= 2)
	{
		Event.Magnitude = FCString::Atof(*Args[1]);
	}
	int32 NotifiedComponents = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (UEGQuestComponent* Component = It->FindComponentByClass<UEGQuestComponent>())
		{
			if (Component->NotifyGameplayEvent(Event))
			{
				++NotifiedComponents;
			}
		}
	}
	UE_LOG(LogEGQuestPlugin, Display, TEXT("Quest.SendEvent: '%s' (magnitude %.2f) accepted by %d component(s)"),
		*Args[0], Event.Magnitude, NotifiedComponents);
}

void FEGQuestPluginModule::CheatCompleteObjective(const TArray<FString>& Args, UWorld* World)
{
	FGuid InstanceGuid;
	FGuid ObjectiveGuid;
	if (!World || Args.Num() < 1 || !FGuid::Parse(Args[0], InstanceGuid))
	{
		UE_LOG(LogEGQuestPlugin, Warning, TEXT("Quest.CompleteObjective usage: Quest.CompleteObjective <QuestInstanceGuid> [ObjectiveGuid]"));
		return;
	}
	// A stage can hold several objectives, so without an explicit one this takes the first still
	// pending - which is what a cheat walking a linear quest forward wants.
	const bool bHasExplicitObjective = Args.Num() >= 2 && FGuid::Parse(Args[1], ObjectiveGuid);
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		UEGQuestComponent* Component = It->FindComponentByClass<UEGQuestComponent>();
		FEGQuestRuntimeSnapshot Snapshot;
		if (!Component || !Component->FindQuestSnapshot(InstanceGuid, Snapshot))
		{
			continue;
		}
		if (!bHasExplicitObjective)
		{
			const FEGQuestSnapshotObjective* Pending = Snapshot.ActiveObjectives.FindByPredicate(
				[](const FEGQuestSnapshotObjective& Entry){ return !Entry.IsResolved(); });
			if (!Pending)
			{
				UE_LOG(LogEGQuestPlugin, Warning, TEXT("Quest.CompleteObjective: instance %s has no pending objective"), *InstanceGuid.ToString());
				return;
			}
			ObjectiveGuid = Pending->Guid;
		}
		const bool bCompleted = Component->CompleteActiveObjective(InstanceGuid, ObjectiveGuid);
		UE_LOG(LogEGQuestPlugin, Display, TEXT("Quest.CompleteObjective: %s objective %s -> %s"),
			*InstanceGuid.ToString(), *ObjectiveGuid.ToString(),
			bCompleted ? TEXT("completed") : TEXT("rejected (see OnQuestRequestRejected)"));
		return;
	}
	UE_LOG(LogEGQuestPlugin, Warning, TEXT("Quest.CompleteObjective: no quest component holds instance %s"), *InstanceGuid.ToString());
}

void FEGQuestPluginModule::CheatSetFact(const TArray<FString>& Args, UWorld* World)
{
	if (!World || Args.Num() < 2) { UE_LOG(LogEGQuestPlugin, Warning, TEXT("Quest.SetFact <GameplayTag> <Value>")); return; }
	const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*Args[0]), false);
	UEGQuestFactsSubsystem* Facts = World->GetSubsystem<UEGQuestFactsSubsystem>();
	if (!Facts || !Tag.IsValid()) { UE_LOG(LogEGQuestPlugin, Warning, TEXT("Quest.SetFact: invalid tag or no facts subsystem")); return; }
	Facts->SetFact(Tag, FCString::Atoi(*Args[1]));
}

void FEGQuestPluginModule::CheatFailObjective(const TArray<FString>& Args, UWorld* World)
{
	FGuid RunId, ObjectiveId;
	if (!World || Args.IsEmpty() || !FGuid::Parse(Args[0], RunId))
	{ UE_LOG(LogEGQuestPlugin, Warning, TEXT("Quest.FailObjective <QuestInstanceGuid> [ObjectiveGuid]")); return; }
	const bool bExplicit = Args.Num() > 1 && FGuid::Parse(Args[1], ObjectiveId);
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		UEGQuestComponent* Component = It->FindComponentByClass<UEGQuestComponent>();
		FEGQuestViewSnapshot Snapshot;
		if (!Component || !Component->FindQuestSnapshot(RunId, Snapshot)) continue;
		if (!bExplicit)
		{
			const FEGQuestSnapshotObjective* Pending = Snapshot.ActiveObjectives.FindByPredicate(
				[](const FEGQuestSnapshotObjective& Line){ return !Line.IsResolved(); });
			if (!Pending) return;
			ObjectiveId = Pending->Guid;
		}
		Component->FailActiveObjective(RunId, ObjectiveId);
		return;
	}
}

void FEGQuestPluginModule::CheatJumpToStage(const TArray<FString>& Args, UWorld* World)
{
	FGuid RunId, StageId;
	if (!World || Args.Num() < 2 || !FGuid::Parse(Args[0], RunId) || !FGuid::Parse(Args[1], StageId))
	{ UE_LOG(LogEGQuestPlugin, Warning, TEXT("Quest.JumpToStage <QuestInstanceGuid> <StageGuid>")); return; }
	for (TActorIterator<AActor> It(World); It; ++It)
		if (UEGQuestComponent* Component = It->FindComponentByClass<UEGQuestComponent>())
			if (Component->DebugJumpToStage(RunId, StageId).IsSuccess()) return;
}
#endif // !UE_BUILD_SHIPPING

void FEGQuestPluginModule::UnregisterConsoleCommands()
{
	WorldContextObjectPtr.Reset();
	for (IConsoleCommand* Command : ConsoleCommands)
	{
		IConsoleManager::Get().UnregisterConsoleObject(Command);
	}
	ConsoleCommands.Empty();
}

void FEGQuestPluginModule::DisplayQuestDataWindow()
{
	if (!bHasRegisteredTabSpawners)
	{
		FEGQuestLogger::Get().Error(TEXT("Did not Initialize the tab spawner for the DisplayQuestDataWindow"));
		return;
	}

	if (!RefreshDisplayQuestDataWindow())
	{
		// Create, because it does not exist yet
		FEGQuestHelper::InvokeTab(FGlobalTabmanager::Get(), FTabId(QUEST_DATA_DISPLAY_TAB_ID));
	}
}

bool FEGQuestPluginModule::RefreshDisplayQuestDataWindow(bool bFocus)
{
	const TSharedPtr<SDockTab> QuestDisplayDataTab =
		FGlobalTabmanager::Get()->FindExistingLiveTab(FTabId(QUEST_DATA_DISPLAY_TAB_ID));
	if (QuestDisplayDataTab.IsValid())
	{
		// Set the new WorldContextObjectPtr.
		TSharedRef<SEGQuestDataDisplay> Window = StaticCastSharedRef<SEGQuestDataDisplay>(QuestDisplayDataTab->GetContent());
		if (WorldContextObjectPtr.IsValid())
		{
			Window->SetWorldContextObject(WorldContextObjectPtr);
			Window->RefreshTree(false);
		}

		// Focus
		if (bFocus)
		{
			FGlobalTabmanager::Get()->DrawAttention(QuestDisplayDataTab.ToSharedRef());
		}

		return true;
	}

	return false;
}

void FEGQuestPluginModule::HandleOnInMemoryAssetDeleted(UObject* DeletedObject)
{
	// Should be safe to access it here
	// See UAssetRegistryImpl::AssetDeleted
	if (UEGQuestGraph* Quest = Cast<UEGQuestGraph>(DeletedObject))
	{
		HandleQuestDeleted(Quest);
	}
}

void FEGQuestPluginModule::HandleOnAssetRemoved(const FAssetData& RemovedAsset)
{
	if (!RemovedAsset.IsAssetLoaded())
	{
		return;
	}
}

void FEGQuestPluginModule::HandleOnAssetRenamed(const FAssetData& AssetRenamed, const FString& OldObjectPath)
{
	UObject* ObjectRenamed = AssetRenamed.GetAsset();
	if (UEGQuestGraph* Quest = Cast<UEGQuestGraph>(ObjectRenamed))
	{
		HandleQuestRenamed(Quest, OldObjectPath);
	}
}

void FEGQuestPluginModule::HandleQuestDeleted(UEGQuestGraph* DeletedQuest)
{
	if (!IsValid(DeletedQuest))
	{
		return;
	}

	DeletedQuest->DeleteAllTextFiles();
}

void FEGQuestPluginModule::HandleQuestRenamed(UEGQuestGraph* RenamedQuest, const FString& OldObjectPath)
{
	if (!IsValid(RenamedQuest))
	{
		return;
	}

	// Rename text file file to new location
	const FString OldTextFilePathName = UEGQuestGraph::GetTextFilePathNameFromAssetPathName(OldObjectPath);
	if (OldTextFilePathName.IsEmpty())
	{
		FEGQuestLogger::Get().Error(TEXT("OldTextFilePathName is empty. This should never happen"));
		return;
	}

	// Current PathName
	const FString CurrentTextFilePathName = RenamedQuest->GetTextFilePathName(false);
	if (OldTextFilePathName == CurrentTextFilePathName)
	{
		FEGQuestLogger::Get().Errorf(
			TEXT("Quest was renamed but the paths before and after are equal :O | `%s` == `%s`"),
			*OldTextFilePathName, *CurrentTextFilePathName
		);
		return;
	}

	// Iterate over all possible text formats
	for (const FString& FileExtension : GetDefault<UEGQuestPluginSettings>()->GetAllTextFileExtensions())
	{
		const FString OldFileName = OldTextFilePathName + FileExtension;
		const FString NewFileName = CurrentTextFilePathName + FileExtension;
		FEGQuestHelper::RenameFile(OldFileName, NewFileName, true);
	}
}

void FEGQuestPluginModule::HandleOnPreLoadMap(const FString& MapName)
{
	// NOTE: only in NON editor game
	const UEGQuestPluginSettings* Settings = GetDefault<UEGQuestPluginSettings>();
	if (!Settings)
	{
		return;
	}

	// Quest history is instance-owned by replicated quest components and is discarded with the world.
}

void FEGQuestPluginModule::HandleOnPostLoadMapWithWorld(UWorld* LoadedWorld)
{
	// NOTE: only in NON editor game
	if (!LoadedWorld)
	{
		return;
	}

	LastLoadedWorld = LoadedWorld;
	const UEGQuestPluginSettings* Settings = GetDefault<UEGQuestPluginSettings>();
	if (!Settings)
	{
		return;
	}

	if (Settings->bRegisterQuestConsoleCommandsAutomatically)
	{
		FEGQuestLogger::Get().Debugf(TEXT("PostLoadMapWithWorld = %s. Registering Console commands"), *LoadedWorld->GetMapName());
		RegisterConsoleCommands(LoadedWorld);
	}
}

#undef LOCTEXT_NAMESPACE

//////////////////////////////////////////////////////////////////////////
IMPLEMENT_MODULE(FEGQuestPluginModule, UnrealExtendedQuest)
//////////////////////////////////////////////////////////////////////////
