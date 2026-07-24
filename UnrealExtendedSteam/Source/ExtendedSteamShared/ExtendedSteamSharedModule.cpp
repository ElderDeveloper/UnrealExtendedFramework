// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ExtendedSteamSharedModule.h"
#include "Shared/ESteamLog.h"
#include "Shared/ESteamSDK.h"
#include "Shared/ESteamSettings.h"

#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

#if WITH_EXTENDEDSTEAM_SDK
THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
#include "steam/steam_gameserver.h"
THIRD_PARTY_INCLUDES_END
#endif

void FExtendedSteamSharedModule::StartupModule()
{
	UE_LOG(LogExtendedSteam, Log, TEXT("ExtendedSteamShared starting up (SDK support: %d, SDK version: %d)"),
		WITH_EXTENDEDSTEAM_SDK, ESTEAM_SDK_VERSION);

	PostEngineInitHandle = FCoreDelegates::OnPostEngineInit.AddRaw(this, &FExtendedSteamSharedModule::HandlePostEngineInit);
}

void FExtendedSteamSharedModule::ShutdownModule()
{
	FCoreDelegates::OnPostEngineInit.Remove(PostEngineInitHandle);

	ShutdownSteamGameServer();
	ShutdownSteamClient();

	if (SteamDllHandle)
	{
		FPlatformProcess::FreeDllHandle(SteamDllHandle);
		SteamDllHandle = nullptr;
	}
}

void FExtendedSteamSharedModule::HandlePostEngineInit()
{
	if (ShouldAutoInitializeClient())
	{
		InitializeSteamClient();
	}
}

bool FExtendedSteamSharedModule::ShouldAutoInitializeClient() const
{
	const UESteamSettings* Settings = UESteamSettings::Get();

	if (!Settings->bInitializeSteamOnStartup)
	{
		return false;
	}
	if (IsRunningCommandlet() || FApp::IsUnattended())
	{
		return false;
	}
	if (IsRunningDedicatedServer())
	{
		// Dedicated servers use the game server API, orchestrated explicitly.
		return false;
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("NoSteam")))
	{
		return false;
	}
	if (GIsEditor && !Settings->bInitializeSteamInEditor)
	{
		return false;
	}
	return true;
}

bool FExtendedSteamSharedModule::InitializeSteamClient()
{
#if WITH_EXTENDEDSTEAM_SDK
	if (bSteamClientInitialized)
	{
		return true;
	}

	const UESteamSettings* Settings = UESteamSettings::Get();

	ApplyAppIdEnvironment();

	if (!LoadSteamDll())
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("InitializeSteamClient: Steam API library could not be loaded"));
		return false;
	}

#if UE_BUILD_SHIPPING
	if (Settings->bRelaunchInSteam && SteamAPI_RestartAppIfNecessary(static_cast<uint32>(Settings->SteamAppId)))
	{
		UE_LOG(LogExtendedSteam, Log, TEXT("InitializeSteamClient: relaunching through the Steam client (app %d)"), Settings->SteamAppId);
		FPlatformMisc::RequestExit(false);
		return false;
	}
#endif

	// Another Steam integration may already have loaded steam_api64.dll. Windows reuses
	// modules by basename, so the loaded DLL can be newer than the SDK used to compile
	// this module. Newer Steamworks DLLs export SteamAPI_InitFlat instead of SteamAPI_Init.
	// Resolve initialization dynamically to tolerate either ABI without invoking a
	// missing delay-load import (which raises 0xc06d007f before we can fail cleanly).
	using FSteamAPIInitFlat = int32 (*)(ANSICHAR* OutError);
	using FSteamAPIInitLegacy = bool (*)();

	if (FSteamAPIInitFlat InitFlat = reinterpret_cast<FSteamAPIInitFlat>(
		FPlatformProcess::GetDllExport(SteamDllHandle, TEXT("SteamAPI_InitFlat"))))
	{
		ANSICHAR InitError[1024] = {};
		if (InitFlat(InitError) != 0)
		{
			UE_LOG(LogExtendedSteam, Warning, TEXT("InitializeSteamClient: SteamAPI_InitFlat failed: %s"),
				UTF8_TO_TCHAR(InitError));
			return false;
		}
	}
	else if (FSteamAPIInitLegacy InitLegacy = reinterpret_cast<FSteamAPIInitLegacy>(
		FPlatformProcess::GetDllExport(SteamDllHandle, TEXT("SteamAPI_Init"))))
	{
		if (!InitLegacy())
		{
			UE_LOG(LogExtendedSteam, Warning,
				TEXT("InitializeSteamClient: SteamAPI_Init failed (is the Steam client running?)"));
			return false;
		}
	}
	else
	{
		UE_LOG(LogExtendedSteam, Error,
			TEXT("InitializeSteamClient: loaded Steam API library exports neither SteamAPI_InitFlat nor SteamAPI_Init"));
		return false;
	}

	bSteamClientInitialized = true;
	EnsureCallbackPump();

	UE_LOG(LogExtendedSteam, Log, TEXT("Steam client API initialized (app %u)"),
		SteamUtils() ? SteamUtils()->GetAppID() : static_cast<uint32>(Settings->SteamAppId));

	OnSteamClientInitialized.Broadcast();
	return true;
#else
	UE_LOG(LogExtendedSteam, Verbose, TEXT("InitializeSteamClient: built without Steamworks SDK support"));
	return false;
#endif
}

void FExtendedSteamSharedModule::ShutdownSteamClient()
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!bSteamClientInitialized)
	{
		return;
	}

	SteamAPI_Shutdown();
	bSteamClientInitialized = false;
	ReleaseCallbackPumpIfIdle();

	UE_LOG(LogExtendedSteam, Log, TEXT("Steam client API shut down"));
	OnSteamClientShutdown.Broadcast();
#endif
}

bool FExtendedSteamSharedModule::IsSteamClientRunning() const
{
#if WITH_EXTENDEDSTEAM_SDK
	// The delay-loaded library must be present before any flat API call.
	if (!SteamDllHandle && !const_cast<FExtendedSteamSharedModule*>(this)->LoadSteamDll())
	{
		return false;
	}
	return SteamAPI_IsSteamRunning();
#else
	return false;
#endif
}

bool FExtendedSteamSharedModule::InitializeSteamGameServer(int32 GamePort, int32 QueryPort, EESteamServerMode ServerMode, const FString& GameVersion)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (bSteamGameServerInitialized)
	{
		return true;
	}

	ApplyAppIdEnvironment();

	if (!LoadSteamDll())
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("InitializeSteamGameServer: Steam API library could not be loaded"));
		return false;
	}

	EServerMode SdkServerMode = eServerModeNoAuthentication;
	switch (ServerMode)
	{
	case EESteamServerMode::Authentication:
		SdkServerMode = eServerModeAuthentication;
		break;
	case EESteamServerMode::AuthenticationAndSecure:
		SdkServerMode = eServerModeAuthenticationAndSecure;
		break;
	default:
		break;
	}

#if ESTEAM_SDK_AT_LEAST(159)
	SteamErrMsg InitError;
	FMemory::Memzero(InitError);
	if (SteamGameServer_InitEx(0 /*any ip*/, static_cast<uint16>(GamePort), static_cast<uint16>(QueryPort),
		SdkServerMode, TCHAR_TO_UTF8(*GameVersion), &InitError) != k_ESteamAPIInitResult_OK)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("InitializeSteamGameServer: SteamGameServer_InitEx failed: %s"), UTF8_TO_TCHAR(InitError));
		return false;
	}
#else
	if (!SteamGameServer_Init(0 /*any ip*/, static_cast<uint16>(GamePort), static_cast<uint16>(QueryPort),
		SdkServerMode, TCHAR_TO_UTF8(*GameVersion)))
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("InitializeSteamGameServer: SteamGameServer_Init failed"));
		return false;
	}
#endif

	bSteamGameServerInitialized = true;
	EnsureCallbackPump();

	UE_LOG(LogExtendedSteam, Log, TEXT("Steam game server API initialized (game port %d, query port %d)"), GamePort, QueryPort);
	OnSteamGameServerInitialized.Broadcast();
	return true;
#else
	UE_LOG(LogExtendedSteam, Verbose, TEXT("InitializeSteamGameServer: built without Steamworks SDK support"));
	return false;
#endif
}

void FExtendedSteamSharedModule::ShutdownSteamGameServer()
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!bSteamGameServerInitialized)
	{
		return;
	}

	SteamGameServer_Shutdown();
	bSteamGameServerInitialized = false;
	ReleaseCallbackPumpIfIdle();

	UE_LOG(LogExtendedSteam, Log, TEXT("Steam game server API shut down"));
	OnSteamGameServerShutdown.Broadcast();
#endif
}

bool FExtendedSteamSharedModule::LoadSteamDll()
{
#if !WITH_EXTENDEDSTEAM_SDK
	return false;
#elif PLATFORM_WINDOWS
	if (SteamDllHandle)
	{
		return true;
	}

#ifdef STEAM_SDK_VER_PATH
	// Engine-provided SDK: binaries live under the engine third-party folder.
	const FString EngineDllPath = FPaths::EngineDir()
		/ TEXT("Binaries/ThirdParty/Steamworks")
		/ FString(STEAM_SDK_VER_PATH)
		/ TEXT("Win64/steam_api64.dll");
	SteamDllHandle = FPlatformProcess::GetDllHandle(*EngineDllPath);
#endif

	if (!SteamDllHandle)
	{
		// Drop-in SDK (staged next to the target binary) or a PATH-visible library.
		SteamDllHandle = FPlatformProcess::GetDllHandle(TEXT("steam_api64.dll"));
	}

	return SteamDllHandle != nullptr;
#else
	// Mac/Linux link or stage libsteam_api directly; no manual load step is needed.
	return true;
#endif
}

void FExtendedSteamSharedModule::ApplyAppIdEnvironment() const
{
#if !UE_BUILD_SHIPPING
	// Lets Steam resolve the app id in development without a steam_appid.txt next to the executable.
	const FString AppId = LexToString(UESteamSettings::Get()->SteamAppId);
	FPlatformMisc::SetEnvironmentVar(TEXT("SteamAppId"), *AppId);
	FPlatformMisc::SetEnvironmentVar(TEXT("SteamGameId"), *AppId);
#endif
}

void FExtendedSteamSharedModule::EnsureCallbackPump()
{
	if (!CallbackPumpHandle.IsValid())
	{
		CallbackPumpHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateRaw(this, &FExtendedSteamSharedModule::TickCallbacks));
	}
}

void FExtendedSteamSharedModule::ReleaseCallbackPumpIfIdle()
{
	if (CallbackPumpHandle.IsValid() && !bSteamClientInitialized && !bSteamGameServerInitialized)
	{
		FTSTicker::GetCoreTicker().RemoveTicker(CallbackPumpHandle);
		CallbackPumpHandle.Reset();
	}
}

bool FExtendedSteamSharedModule::TickCallbacks(float DeltaTime)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (bSteamClientInitialized)
	{
		SteamAPI_RunCallbacks();
	}
	if (bSteamGameServerInitialized)
	{
		SteamGameServer_RunCallbacks();
	}
#endif
	return true;
}

IMPLEMENT_MODULE(FExtendedSteamSharedModule, ExtendedSteamShared)
