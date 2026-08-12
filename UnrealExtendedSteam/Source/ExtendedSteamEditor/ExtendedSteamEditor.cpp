// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ExtendedSteamEditor.h"

#include "Editor.h"
#include "ExtendedSteamSharedModule.h"

#define LOCTEXT_NAMESPACE "FExtendedSteamEditorModule"

DEFINE_LOG_CATEGORY(LogExtendedSteamEditor);

void FExtendedSteamEditorModule::StartupModule()
{
	// UESteamPublishSettings is a UDeveloperSettings, so it auto-registers with the Settings editor
	// when this module loads. Nothing else to do there — credentials are loaded lazily by the settings
	// object in PostInitProperties.

	// Deferred client lifecycle. Bind unconditionally and re-check the setting per session: the
	// settings object is editable at runtime, so a decision made once at startup would go stale.
	//
	// BeginPIE rather than PostPIEStarted: PostPIEStarted fires after BeginPlay, which is too late for
	// game code that expects Steam to be up by then. ShutdownPIE rather than EndPIE for the mirror
	// reason — EndPIE fires before the world and its subsystems tear down, and those are exactly the
	// things that still want a live Steam API on the way out.
	BeginPIEHandle = FEditorDelegates::BeginPIE.AddRaw(this, &FExtendedSteamEditorModule::HandleBeginPIE);
	ShutdownPIEHandle = FEditorDelegates::ShutdownPIE.AddRaw(this, &FExtendedSteamEditorModule::HandleShutdownPIE);
}

void FExtendedSteamEditorModule::ShutdownModule()
{
	FEditorDelegates::BeginPIE.Remove(BeginPIEHandle);
	FEditorDelegates::ShutdownPIE.Remove(ShutdownPIEHandle);
	BeginPIEHandle.Reset();
	ShutdownPIEHandle.Reset();

	// A PIE session still running at module shutdown (editor closing mid-play) would otherwise leak
	// the client into the shared module's own shutdown, which is fine but logs out of order.
	ShutdownClientIfOwned();
}

void FExtendedSteamEditorModule::HandleBeginPIE(const bool bIsSimulating)
{
	if (!FExtendedSteamSharedModule::IsModuleAvailable())
	{
		return;
	}

	FExtendedSteamSharedModule& Steam = FExtendedSteamSharedModule::Get();
	if (!Steam.ShouldInitializeClientForPIE())
	{
		return;
	}

	if (Steam.IsSteamClientInitialized())
	{
		// Already up — someone initialized explicitly, or a previous session did not release it.
		// Leave ownership where it is so we do not shut down a client we did not start.
		return;
	}

	if (Steam.InitializeSteamClient())
	{
		bClientInitializedForPIE = true;
		UE_LOG(LogExtendedSteamEditor, Log, TEXT("Steam client initialized for this PIE session."));
	}
	else
	{
		// Not fatal: PIE runs without Steam features. InitializeSteamClient has already logged why.
		UE_LOG(LogExtendedSteamEditor, Warning,
			TEXT("Steam client could not be initialized for PIE — Steam features are unavailable this session."));
	}
}

void FExtendedSteamEditorModule::HandleShutdownPIE(const bool bIsSimulating)
{
	ShutdownClientIfOwned();
}

void FExtendedSteamEditorModule::ShutdownClientIfOwned()
{
	if (!bClientInitializedForPIE)
	{
		return;
	}

	bClientInitializedForPIE = false;

	if (FExtendedSteamSharedModule::IsModuleAvailable())
	{
		FExtendedSteamSharedModule::Get().ShutdownSteamClient();
		UE_LOG(LogExtendedSteamEditor, Log, TEXT("Steam client shut down with the PIE session."));
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FExtendedSteamEditorModule, ExtendedSteamEditor)
