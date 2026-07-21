// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "Modules/ModuleManager.h"
#include "Modules/ModuleInterface.h"

// The name of the Quest System plugin as defined in the .uplugin file
const FName QUEST_SYSTEM_PLUGIN_NAME(TEXT("UnrealExtendedQuest"));
// The host plugin name used for content/config path discovery.
const FName QUEST_SYSTEM_HOST_PLUGIN_NAME(TEXT("UnrealExtendedQuest"));

class AActor;
class SWidget;
class SDockTab;
struct FTabSpawnerEntry;

/**
 * Interface for the QuestPlugin module.
 */
class UNREALEXTENDEDQUEST_API IEGQuestPluginModule : public IModuleInterface
{
public:

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static IEGQuestPluginModule& Get()
	{
		return FModuleManager::LoadModuleChecked<IEGQuestPluginModule>(QUEST_SYSTEM_PLUGIN_NAME);
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded(QUEST_SYSTEM_PLUGIN_NAME);
	}

public:
	virtual ~IEGQuestPluginModule() {}

	/**
	 * Registers all the console commands.
	 * @param WorldConextObjectPtr - The reference actor for the World. Without this the runtime module won't know how to get the UWorld.
	 */
	virtual void RegisterConsoleCommands(const TWeakObjectPtr<const UObject>& WorldContextObjectPtr) = 0;

	// Unregister all the console commands
	virtual void UnregisterConsoleCommands() = 0;

	// Gets the debug Quest Data Display Window.
	virtual TSharedRef<SWidget> GetQuestDataDisplayWindow() = 0;

	// Gets the tab entry for the Window
	virtual FTabSpawnerEntry* GetQuestDataDisplaySpawnEntry() = 0;

	// Display the debug Quest Data Window on the screen
	virtual void DisplayQuestDataWindow() = 0;
};
