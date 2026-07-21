// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "AssetTypeCategories.h"
#include "Modules/ModuleManager.h"
#include "Modules/ModuleInterface.h"

// The name of the Quest System Editor plugin as defined in the .uplugin file
const FName QUEST_SYSTEM_EDITOR_PLUGIN_NAME(TEXT("UnrealExtendedQuestEditor"));

/**
 * Interface for the QuestPluginEditor module.
 */
class UNREALEXTENDEDQUESTEDITOR_API IEGQuestPluginEditorModule : public IModuleInterface
{
public:

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static IEGQuestPluginEditorModule& Get()
	{
		return FModuleManager::LoadModuleChecked<IEGQuestPluginEditorModule>(QUEST_SYSTEM_EDITOR_PLUGIN_NAME);
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded(QUEST_SYSTEM_EDITOR_PLUGIN_NAME);
	}

	// Returns the Asset Category for this plugin
	virtual EAssetTypeCategories::Type GetAssetCategory() const = 0;

public:
	virtual ~IEGQuestPluginEditorModule() {}
};
