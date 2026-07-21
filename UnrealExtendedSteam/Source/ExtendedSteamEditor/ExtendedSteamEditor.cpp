// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ExtendedSteamEditor.h"

#define LOCTEXT_NAMESPACE "FExtendedSteamEditorModule"

DEFINE_LOG_CATEGORY(LogExtendedSteamEditor);

void FExtendedSteamEditorModule::StartupModule()
{
	// UESteamPublishSettings is a UDeveloperSettings, so it auto-registers with the Settings editor
	// when this module loads. Nothing else to do here — credentials are loaded lazily by the settings
	// object in PostInitProperties.
}

void FExtendedSteamEditorModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FExtendedSteamEditorModule, ExtendedSteamEditor)
