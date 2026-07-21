// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Modules/ModuleManager.h"
#include "Shared/ESteamTypes.h"

/**
 * Single owner of the Steamworks SDK lifecycle for the whole plugin.
 *
 * Client API: initialized automatically on PostEngineInit when enabled in UESteamSettings
 * (skipped for commandlets, unattended runs, dedicated servers and -NoSteam). Also callable
 * explicitly. Game server API: never automatic; call InitializeSteamGameServer explicitly
 * (the Extended Steam online subsystem orchestrates this in dedicated-server setups).
 *
 * While either API is initialized, a core ticker pumps the Steam callback queues every frame.
 */
class EXTENDEDSTEAMSHARED_API FExtendedSteamSharedModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static FExtendedSteamSharedModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FExtendedSteamSharedModule>("ExtendedSteamShared");
	}

	static bool IsModuleAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("ExtendedSteamShared");
	}

	/**
	 * Initializes the Steam client API (idempotent). Returns true when initialized.
	 * Fails cleanly when the SDK is unavailable, the Steam client is not running,
	 * or the app id is not accessible.
	 */
	bool InitializeSteamClient();

	/** Shuts the Steam client API down (safe to call when not initialized). */
	void ShutdownSteamClient();

	/** True while the Steam client API is initialized and usable. */
	bool IsSteamClientInitialized() const { return bSteamClientInitialized; }

	/** True when a Steam client is running on this machine (does not require prior initialization). */
	bool IsSteamClientRunning() const;

	/**
	 * Initializes the Steam game server API (idempotent). GamePort/QueryPort are the
	 * listen ports advertised to Steam; GameVersion is the server version string used
	 * by the master server for out-of-date detection.
	 */
	bool InitializeSteamGameServer(int32 GamePort, int32 QueryPort, EESteamServerMode ServerMode, const FString& GameVersion);

	/** Shuts the Steam game server API down (safe to call when not initialized). */
	void ShutdownSteamGameServer();

	/** True while the Steam game server API is initialized. */
	bool IsSteamGameServerInitialized() const { return bSteamGameServerInitialized; }

	/** Broadcast after the Steam client API initialized successfully. */
	FSimpleMulticastDelegate OnSteamClientInitialized;

	/** Broadcast after the Steam client API shut down. */
	FSimpleMulticastDelegate OnSteamClientShutdown;

	/** Broadcast after the Steam game server API initialized successfully. */
	FSimpleMulticastDelegate OnSteamGameServerInitialized;

	/** Broadcast after the Steam game server API shut down. */
	FSimpleMulticastDelegate OnSteamGameServerShutdown;

private:
	void HandlePostEngineInit();
	bool ShouldAutoInitializeClient() const;

	/** Loads the platform Steam API library so delay-loaded calls can resolve. */
	bool LoadSteamDll();

	/** Publishes the configured app id to the process environment (non-shipping convenience). */
	void ApplyAppIdEnvironment() const;

	void EnsureCallbackPump();
	void ReleaseCallbackPumpIfIdle();
	bool TickCallbacks(float DeltaTime);

	void* SteamDllHandle = nullptr;
	FTSTicker::FDelegateHandle CallbackPumpHandle;
	FDelegateHandle PostEngineInitHandle;
	bool bSteamClientInitialized = false;
	bool bSteamGameServerInitialized = false;
};
