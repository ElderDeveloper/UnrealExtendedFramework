// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnrealExtendedSettings.h"

#include "ISettingsModule.h"
#include "UnrealExtendedSettings/Subsystems/ESQualitySubsystem.h"

#define LOCTEXT_NAMESPACE "FUnrealExtendedSettingsModule"

void FUnrealExtendedSettingsModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->RegisterSettings(
			"Project",
			"Plugins",
			"SettingsPlugin",
			LOCTEXT("RuntimeSettingName" , "Extended Settings Plugin") ,
			LOCTEXT("RuntimeSettingsDescription" , "Configure my setting") ,
			GetMutableDefault<UESQualitySubsystem>());
	}
}

void FUnrealExtendedSettingsModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	if (const auto Settings = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		Settings->UnregisterSettings("Project","Plugins","SettingsPlugin");
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FUnrealExtendedSettingsModule, UnrealExtendedSettings)