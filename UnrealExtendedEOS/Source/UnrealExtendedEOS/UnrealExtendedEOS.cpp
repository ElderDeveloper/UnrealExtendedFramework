// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "UnrealExtendedEOS.h"
#include "Shared/EEOSSettings.h"

#define LOCTEXT_NAMESPACE "FUnrealExtendedEOSModule"

// LogExtendedEOS is now defined in ExtendedEOSShared (see ExtendedEOSSharedModule.cpp).

void FUnrealExtendedEOSModule::StartupModule()
{
	// Re-apply the configured log verbosity here, not only from UEEOSSettings::PostInitProperties:
	// the settings CDO is constructed while ExtendedEOSShared loads (PreDefault), which can run
	// before the engine processes -LogCmds / [Core.Log] and resets category verbosities. This
	// module is Default phase, i.e. after that, so the setting is what actually sticks.
	if (const UEEOSSettings* Settings = UEEOSSettings::Get())
	{
		Settings->ApplyLogVerbosity();
	}

	UE_LOG(LogExtendedEOS, Log, TEXT("UnrealExtendedEOS module started"));
}

void FUnrealExtendedEOSModule::ShutdownModule()
{
	UE_LOG(LogExtendedEOS, Log, TEXT("UnrealExtendedEOS module shutdown"));
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FUnrealExtendedEOSModule, UnrealExtendedEOS)
