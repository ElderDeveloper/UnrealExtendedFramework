// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ExtendedSteamSharedModule.h"
#include "Shared/ESteamLog.h"
#include "Shared/ESteamSDK.h"
#include "Shared/ESteamSettings.h"

#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreDelegates.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UObject/UObjectGlobals.h"

namespace ESteamSettingsConfig
{
	static const TCHAR* Section = TEXT("/Script/ExtendedSteamShared.ESteamSettings");

	/**
	 * This module loads at PostConfigInit — the phase the engine reserves for "platform services that
	 * need D3D hooks like Steam" — which is before the UObject system exists, so UESteamSettings::Get()
	 * cannot be used on the early path. GConfig is guaranteed valid there (it is what defines the
	 * phase), so read the handful of values the early path needs straight from it.
	 * Defaults must mirror the UPROPERTY initialisers in UESteamSettings.
	 */
	static bool GetBool(const TCHAR* Key, bool Default)
	{
		bool Value = Default;
		if (GConfig)
		{
			GConfig->GetBool(Section, Key, Value, GGameIni);
		}
		return Value;
	}

	static int32 GetInt(const TCHAR* Key, int32 Default)
	{
		int32 Value = Default;
		if (GConfig)
		{
			GConfig->GetInt(Section, Key, Value, GGameIni);
		}
		return Value;
	}
}

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

	// SteamAPI_Init is what pulls Steam's overlay renderer (gameoverlayrenderer64.dll) into the
	// process, and that renderer can only hook rendering if it lands before the RHI creates the D3D
	// device — LaunchEngineLoop's own comment on the PostConfigInit phase says it must run "before
	// Render/RHI subsystem D3DCreate() for platform services that need D3D hooks like Steam".
	// PostEngineInit (and even PreDefault, measured) is far too late: dxgi.dll and d3d12.dll are
	// already loaded, the hook finds nothing, Steam never spawns its overlay UI, and Shift+Tab does
	// nothing. Hence this module's PostConfigInit loading phase and this call.
	if (ShouldInitializeClientEarly())
	{
		InitializeSteamClient();
	}

	// Still bound even after an early attempt: it is idempotent, and it retries the case where the
	// Steam client was not ready yet this early in startup.
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

bool FExtendedSteamSharedModule::IsClientAutoInitAllowed() const
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

bool FExtendedSteamSharedModule::ShouldInitializeClientEarly() const
{
	// Runs at PostConfigInit: GIsEditor, FApp::IsUnattended() and IsRunningCommandlet() are not
	// resolved yet, and there is no UObject system. Everything here therefore comes from the command
	// line and GConfig — deliberately NOT from IsClientAutoInitAllowed(), which reads all of those.
	const TCHAR* CmdLine = FCommandLine::Get();

	if (FParse::Param(CmdLine, TEXT("NoSteam")) || FParse::Param(CmdLine, TEXT("unattended")))
	{
		return false;
	}
	// Dedicated servers use the game server API, orchestrated explicitly.
	if (FParse::Param(CmdLine, TEXT("server")) || FPlatformProperties::IsServerOnly())
	{
		return false;
	}
	// Commandlets are launched as -run=<Name>.
	FString CommandletName;
	if (FParse::Value(CmdLine, TEXT("-run="), CommandletName))
	{
		return false;
	}
	if (!ESteamSettingsConfig::GetBool(TEXT("bInitializeSteamOnStartup"), true))
	{
		return false;
	}

#if WITH_EDITOR
	// An editor build produces both the editor and the Standalone Game process from the same
	// executable; only the latter carries -game. The editor itself must NOT take this path: its RHI
	// is up before any plugin loads, so it could never gain an overlay, and initializing here would
	// re-create the "editor counts as playing" problem bDeferEditorInitToPIE exists to solve.
	if (!FParse::Param(CmdLine, TEXT("game")))
	{
		return false;
	}
#endif

	return true;
}

bool FExtendedSteamSharedModule::ShouldAutoInitializeClient() const
{
	if (!IsClientAutoInitAllowed())
	{
		return false;
	}

	// In the editor the default is to wait for PIE: initializing here would register the editor
	// process itself with the Steam client, which reports the account as in-game for the whole
	// session. ExtendedSteamEditor initializes on BeginPIE instead.
	return !GIsEditor || !UESteamSettings::Get()->bDeferEditorInitToPIE;
}

bool FExtendedSteamSharedModule::ShouldInitializeClientForPIE() const
{
	return GIsEditor
		&& IsClientAutoInitAllowed()
		&& UESteamSettings::Get()->bDeferEditorInitToPIE;
}

bool FExtendedSteamSharedModule::InitializeSteamClient()
{
#if WITH_EXTENDEDSTEAM_SDK
	if (bSteamClientInitialized)
	{
		return true;
	}

	// Reachable from the PostConfigInit early path, where there is no UObject system yet, so every
	// setting here goes through GetConfiguredAppId/ESteamSettingsConfig rather than the CDO.
	const int32 ConfiguredAppId = GetConfiguredAppId();

	ApplyAppIdEnvironment();

	if (!LoadSteamDll())
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("InitializeSteamClient: Steam API library could not be loaded"));
		return false;
	}

#if UE_BUILD_SHIPPING
	if (ESteamSettingsConfig::GetBool(TEXT("bRelaunchInSteam"), false)
		&& SteamAPI_RestartAppIfNecessary(static_cast<uint32>(ConfiguredAppId)))
	{
		UE_LOG(LogExtendedSteam, Log, TEXT("InitializeSteamClient: relaunching through the Steam client (app %d)"), ConfiguredAppId);
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
		SteamUtils() ? SteamUtils()->GetAppID() : static_cast<uint32>(ConfiguredAppId));

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

	// Drop-in SDK: STEAM_SDK_VER_PATH is defined only by the engine's Steamworks module, so a
	// drop-in build reaches this point with no full path to try. A bare-name LoadLibrary searches
	// only the executable directory, the CWD, the system directories and PATH — the plugin tree is
	// none of those, so the drop-in SDK's own redistributable would never be found and Steam would
	// silently stay uninitialized. Resolve it against the plugin base dir instead.
	if (!SteamDllHandle)
	{
		if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("UnrealExtendedSteam")))
		{
			const FString BaseDir = Plugin->GetBaseDir();
			const TCHAR* RelativeCandidates[] =
			{
				// Where a drop-in SDK keeps it, and where RuntimeDependencies stages it for a package.
				TEXT("Source/ThirdParty/ExtendedSteamLibrary/SDK/redistributable_bin/win64/steam_api64.dll"),
				// Hand-placed next to the plugin's own binaries.
				TEXT("Binaries/Win64/steam_api64.dll"),
			};

			for (const TCHAR* RelativeCandidate : RelativeCandidates)
			{
				const FString CandidatePath = BaseDir / RelativeCandidate;
				// Probe first: GetDllHandle logs a warning for every miss, and a missing optional
				// candidate is not worth a scary line in the log.
				if (FPaths::FileExists(CandidatePath))
				{
					SteamDllHandle = FPlatformProcess::GetDllHandle(*CandidatePath);
					if (SteamDllHandle)
					{
						break;
					}
				}
			}
		}
	}

	if (!SteamDllHandle)
	{
		// Staged next to the target binary, or a PATH-visible library.
		SteamDllHandle = FPlatformProcess::GetDllHandle(TEXT("steam_api64.dll"));
	}

	return SteamDllHandle != nullptr;
#else
	// Mac/Linux link or stage libsteam_api directly; no manual load step is needed.
	return true;
#endif
}

int32 FExtendedSteamSharedModule::GetConfiguredAppId() const
{
	// The CDO is the source of truth once it exists (it honours any runtime override); before the
	// UObject system is up — the PostConfigInit early path — fall back to the same config it loads from.
	if (UObjectInitialized())
	{
		return UESteamSettings::Get()->SteamAppId;
	}
	return ESteamSettingsConfig::GetInt(TEXT("SteamAppId"), 480);
}

void FExtendedSteamSharedModule::ApplyAppIdEnvironment() const
{
#if !UE_BUILD_SHIPPING
	// Lets Steam resolve the app id in development without a steam_appid.txt next to the executable.
	const FString AppId = LexToString(GetConfiguredAppId());
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
