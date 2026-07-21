// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "IEGQuestPluginModule.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

class UEGQuestGraph;
class SWidget;
struct FAssetData;
class SEGQuestDataDisplay;
class SDockTab;
struct IConsoleCommand;
class AActor;
struct FTabSpawnerEntry;

DECLARE_LOG_CATEGORY_EXTERN(LogEGQuestPlugin, All, All);

// Implementation of the QuestPlugin Module
class UNREALEXTENDEDQUEST_API FEGQuestPluginModule : public IEGQuestPluginModule
{
	typedef FEGQuestPluginModule Self;
public:
	// IModuleInterface implementation
	void StartupModule() override;
	void ShutdownModule() override;

	// IEGQuestPluginModule implementation
	void RegisterConsoleCommands(const TWeakObjectPtr<const UObject>& InWorldContextObjectPtr) override;
	void UnregisterConsoleCommands() override;
	TSharedRef<SWidget> GetQuestDataDisplayWindow() override;
	FTabSpawnerEntry* GetQuestDataDisplaySpawnEntry() override;
	void DisplayQuestDataWindow() override;

private:
	// Refreshes the actor of the QuestDataDisplay if it is already opened. Return true if refresh was successful
	bool RefreshDisplayQuestDataWindow(bool bFocus = true);

	// Handle the event from the asset registry when an asset was deleted.
	void HandleOnInMemoryAssetDeleted(UObject* DeletedObject);

	// Handle the event for when assets are removed from the asset registry.
	void HandleOnAssetRemoved(const FAssetData& RemovedAsset);

	// Handle the event for when assets are renamed in the registry
	void HandleOnAssetRenamed(const FAssetData& AssetRenamed, const FString& OldObjectPath);

	// Handle the event after the Quest was deleted. Deletes the text file(s).
	void HandleQuestDeleted(UEGQuestGraph* DeletedQuest);

	// Handle the event after the Quest was renamed. Rename the text file(s).
	void HandleQuestRenamed(UEGQuestGraph* RenamedQuest, const FString& OldObjectPath);

	// Handle event when a new map is loaded.
	void HandleOnPreLoadMap(const FString& MapName);

	// Handle event when a new map with world is loaded is loaded.
	void HandleOnPostLoadMapWithWorld(UWorld* LoadedWorld);

	// Print authoritative/client snapshot state for every quest component in the current world.
	void DumpActiveQuests() const;

#if !UE_BUILD_SHIPPING
	// Cheat: Quest.SendEvent <GameplayTag> [Magnitude] - publishes a quest gameplay event on the authority.
	static void CheatSendQuestEvent(const TArray<FString>& Args, UWorld* World);
	// Cheat: Quest.CompleteObjective <QuestInstanceGuid> [ObjectiveGuid] - completes an objective of the active stage.
	static void CheatCompleteObjective(const TArray<FString>& Args, UWorld* World);
	static void CheatSetFact(const TArray<FString>& Args, UWorld* World);
	static void CheatFailObjective(const TArray<FString>& Args, UWorld* World);
	static void CheatJumpToStage(const TArray<FString>& Args, UWorld* World);
#endif

private:
	// True if the tab spawners have been registered for this module
	bool bHasRegisteredTabSpawners = false;

	// Holds the widget reflector singleton.
	TWeakPtr<SEGQuestDataDisplay> QuestDataDisplayWidget;
	FTabSpawnerEntry* QuestDataDisplayTabSpawnEntry = nullptr;

	// Holds the console commands for this Module
	TArray<IConsoleCommand*> ConsoleCommands;

	// Reference Object used to get the World
	TWeakObjectPtr<const UObject> WorldContextObjectPtr = nullptr;

	// NOTE: only in NON editor game
	TWeakObjectPtr<UWorld> LastLoadedWorld = nullptr;

	// Handlers
	FDelegateHandle OnPreLoadMapHandle;
	FDelegateHandle OnPostLoadMapWithWorldHandle;
	FDelegateHandle OnInMemoryAssetDeletedHandle;
	FDelegateHandle OnAssetRemovedHandle;
	FDelegateHandle OnAssetRenamedHandle;
};
