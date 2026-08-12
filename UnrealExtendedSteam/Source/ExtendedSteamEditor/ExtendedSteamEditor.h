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
 *
 * Also owns the deferred Steam client lifecycle for the editor: with
 * UESteamSettings::bDeferEditorInitToPIE (the default), the Steam client API is brought up when a PIE
 * session begins and taken down when it has fully shut down, so the editor process is only registered
 * with Steam while you are actually playing.
 */
class FExtendedSteamEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void HandleBeginPIE(const bool bIsSimulating);
	void HandleShutdownPIE(const bool bIsSimulating);

	/** Releases the PIE-scoped client if one is up. Safe to call when there is none. */
	void ShutdownClientIfOwned();

	FDelegateHandle BeginPIEHandle;
	FDelegateHandle ShutdownPIEHandle;

	/**
	 * True only when *this* module started the client for the current PIE session. Guards against
	 * tearing down a client someone else brought up explicitly (InitializeSteamClient is public).
	 */
	bool bClientInitializedForPIE = false;
};
