// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

EXTENDEDSTEAMEDITOR_API DECLARE_LOG_CATEGORY_EXTERN(LogExtendedSteamEditor, Log, All);

/**
 * Editor-only module for the Extended Steam plugin.
 *
 * Adds the "Extended Steam Publish" page under Project Settings -> Extended Framework, which drives
 * SteamPipe (steamcmd) build uploads directly from the editor: it extracts the bundled ContentBuilder
 * tools, stores upload credentials in a version-control-ignored file, generates the VDF build scripts
 * from the settings, and runs steamcmd. Excluded from packaged/shipping builds.
 */
class FExtendedSteamEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
