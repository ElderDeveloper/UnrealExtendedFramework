// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "SocketSubsystemModule.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"
#include "Subsystem/ExtendedSteamSocketsSubsystem.h"
#include "Shared/ExtendedSteamSocketsTypes.h"
#include "ExtendedSteamSharedModule.h"
#include "Shared/ESteamLog.h"
#include "Shared/ESteamSDK.h"

/**
 * Module for the SteamNetworkingSockets-based socket subsystem ("ExtendedSteamSockets") and its
 * net driver.
 *
 * Registration is deliberately gated and opt-in. The subsystem is registered with the Sockets module
 * only when ALL of the following hold:
 *   1. The Steamworks SDK is compiled in (WITH_EXTENDEDSTEAM_SDK).
 *   2. The process is NOT a commandlet (no networking transport in cook/build tools).
 *   3. [OnlineSubsystemExtendedSteam] bUseSteamNetworking=true in the Engine ini (default FALSE).
 *   4. The Steam client API is initialized. Because Steam usually comes up AFTER modules load, if the
 *      client is not yet initialized we bind FExtendedSteamSharedModule::OnSteamClientInitialized and
 *      register when it fires; if it is already up we register immediately.
 *
 * This module NEVER pumps Steam callbacks — the shared module owns SteamAPI_RunCallbacks, which also
 * dispatches SteamNetworkingSockets connection callbacks.
 */
class FExtendedSteamSocketsModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		if (!ShouldEnableSteamNetworking())
		{
			UE_LOG(LogExtendedSteam, Verbose,
				TEXT("ExtendedSteamSockets: Steam networking disabled (SDK/commandlet/config gate); subsystem not registered"));
			return;
		}

		// Register now if Steam is already up, otherwise defer until the shared module signals init.
		if (FExtendedSteamSharedModule::IsModuleAvailable() && FExtendedSteamSharedModule::Get().IsSteamClientInitialized())
		{
			RegisterSubsystem();
		}
		else if (FExtendedSteamSharedModule::IsModuleAvailable())
		{
			UE_LOG(LogExtendedSteam, Log,
				TEXT("ExtendedSteamSockets: Steam client not initialized yet; deferring socket subsystem registration"));
			SteamInitializedHandle = FExtendedSteamSharedModule::Get().OnSteamClientInitialized.AddRaw(
				this, &FExtendedSteamSocketsModule::RegisterSubsystem);
		}
	}

	virtual void ShutdownModule() override
	{
		if (FExtendedSteamSharedModule::IsModuleAvailable() && SteamInitializedHandle.IsValid())
		{
			FExtendedSteamSharedModule::Get().OnSteamClientInitialized.Remove(SteamInitializedHandle);
			SteamInitializedHandle.Reset();
		}

		UnregisterSubsystem();
	}

	virtual bool SupportsDynamicReloading() override
	{
		return false;
	}

private:
	/** Evaluates the static (non-Steam-state) portion of the registration gate. */
	static bool ShouldEnableSteamNetworking()
	{
#if !WITH_EXTENDEDSTEAM_SDK
		return false;
#else
		if (IsRunningCommandlet())
		{
			return false;
		}

		bool bUseSteamNetworking = false; // Opt-in: absent key stays false.
		if (GConfig != nullptr)
		{
			GConfig->GetBool(TEXT("OnlineSubsystemExtendedSteam"), TEXT("bUseSteamNetworking"), bUseSteamNetworking, GEngineIni);
		}
		return bUseSteamNetworking;
#endif
	}

	/** Creates + Inits the subsystem and registers it with the Sockets module (not as the default). */
	void RegisterSubsystem()
	{
		if (bRegistered)
		{
			return;
		}

		UE_LOG(LogExtendedSteam, Warning,
			TEXT("ExtendedSteamSockets: the SteamNetworkingSockets net driver is EXPERIMENTAL. It carries a live ")
			TEXT("connection (listen-server accept + receive + stateless handshake are wired), but it is NOT the ")
			TEXT("default transport and is less battle-tested than EOS or the engine's Steam sockets. It is enabled ")
			TEXT("because [OnlineSubsystemExtendedSteam] bUseSteamNetworking=true."));

		FSocketSubsystemModule& SocketsModule = FModuleManager::LoadModuleChecked<FSocketSubsystemModule>("Sockets");

		SocketSubsystem = FExtendedSteamSocketsSubsystem::Create();

		FString InitError;
		if (!SocketSubsystem->Init(InitError))
		{
			UE_LOG(LogExtendedSteam, Warning, TEXT("ExtendedSteamSockets: subsystem Init failed: %s"), *InitError);
			FExtendedSteamSocketsSubsystem::Destroy();
			SocketSubsystem = nullptr;
			return;
		}

		// bMakeDefault=false: never override the platform default socket subsystem.
		SocketsModule.RegisterSocketSubsystem(EXTENDEDSTEAM_SOCKETS_SUBSYSTEM, SocketSubsystem, false);
		bRegistered = true;

		UE_LOG(LogExtendedSteam, Log, TEXT("ExtendedSteamSockets: socket subsystem registered as '%s'"),
			*EXTENDEDSTEAM_SOCKETS_SUBSYSTEM.ToString());
	}

	/** Unregisters from the Sockets module (which does not delete it) and frees the singleton. */
	void UnregisterSubsystem()
	{
		if (!bRegistered)
		{
			return;
		}

		if (FModuleManager::Get().IsModuleLoaded("Sockets"))
		{
			FModuleManager::GetModuleChecked<FSocketSubsystemModule>("Sockets")
				.UnregisterSocketSubsystem(EXTENDEDSTEAM_SOCKETS_SUBSYSTEM);
		}

		FExtendedSteamSocketsSubsystem::Destroy();
		SocketSubsystem = nullptr;
		bRegistered = false;
	}

	FExtendedSteamSocketsSubsystem* SocketSubsystem = nullptr;
	FDelegateHandle SteamInitializedHandle;
	bool bRegistered = false;
};

IMPLEMENT_MODULE(FExtendedSteamSocketsModule, ExtendedSteamSockets)
