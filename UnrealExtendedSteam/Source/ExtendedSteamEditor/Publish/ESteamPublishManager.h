// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UESteamPublishSettings;

/**
 * Editor-side driver for SteamPipe uploads. Stateless static helpers invoked by the settings buttons:
 * prepare the bundled ContentBuilder tools, add the secrets folder to version-control ignore files,
 * generate the VDF build scripts, and run steamcmd (async so the editor stays responsive).
 */
class FESteamPublishManager
{
public:
	/** <Project>/Config/SteamPublish/ContentBuilder — default working folder for the extracted tools. */
	static FString GetDefaultContentBuilderDirectory();

	/** Copy the plugin's bundled ContentBuilder into the working folder and remember the path in settings. */
	static void ExtractContentBuilderTools(UESteamPublishSettings* Settings);

	/** Project check: add the Config/SteamPublish folder to .gitignore / .svnignore / .p4ignore as detected. */
	static void SecureForVersionControl();

	/** Launch steamcmd in a console for a one-time interactive login (enter Steam Guard code + authorize). */
	static void LaunchInteractiveLogin(const UESteamPublishSettings* Settings);

	/** Validate settings, generate VDFs, and run steamcmd on a background thread. Reports via editor notification. */
	static void PublishBuildAsync(const UESteamPublishSettings* Settings);

	/** Show an editor toast + log line. Safe to call from any thread. */
	static void Notify(const FString& Message, bool bSuccess);

private:
	static FString ResolveContentBuilderDirectory(const UESteamPublishSettings* Settings);
	static FString ResolveContentRoot(const UESteamPublishSettings* Settings);
	static FString GetSteamCmdExecutable(const FString& ContentBuilderDir);
	static FString FindPluginContentBuilderSource();
	static bool GenerateVdfScripts(const UESteamPublishSettings* Settings, const FString& ContentBuilderDir, FString& OutAppBuildVdfPath, FString& OutError);

	/** Walk up from the project directory looking for a folder that contains MarkerName; empty if none. */
	static FString FindMarkerRoot(const FString& MarkerName);

	/** Append RelEntry to <VcsRoot>/<IgnoreFileName> if not already present. Returns true if a line was added. */
	static bool AddIgnoreEntry(const FString& VcsRoot, const FString& IgnoreFileName, const FString& RelEntry, FString& OutStatus);
};
