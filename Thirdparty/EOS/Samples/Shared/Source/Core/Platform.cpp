// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"
#include "DebugLog.h"
#include "CommandLine.h"
#include "StringUtils.h"
#include "SampleConstants.h"
#include "Platform.h"
#include "Game.h"
#include "GameEvent.h"
#include "Utils/Utils.h"

#if ALLOW_RESERVED_OPTIONS
#include "ReservedPlatformOptions.h"
#endif

#ifdef _WIN32
#include "Windows/eos_Windows.h"
#endif

constexpr char SampleConstants::ProductId[];
constexpr char SampleConstants::SandboxId[];
constexpr char SampleConstants::DeploymentId[];
constexpr char SampleConstants::ClientCredentialsId[];
constexpr char SampleConstants::ClientCredentialsSecret[];
constexpr char SampleConstants::EncryptionKey[];

EOS_HPlatform FPlatform::PlatformHandle = nullptr;
bool FPlatform::bIsInit = false;
bool FPlatform::bIsShuttingDown = false;
bool FPlatform::bHasShownCreateFailedError = false;
bool FPlatform::bHasShownInvalidParamsErrors = false;
bool FPlatform::bHasInvalidParamProductId = false;
bool FPlatform::bHasInvalidParamSandboxId = false;
bool FPlatform::bHasInvalidParamDeploymentId = false;
bool FPlatform::bHasInvalidParamClientCreds = false;

bool FPlatform::Create()
{
	bIsInit = false;

	const std::wstring CmdLocale = FCommandLine::Get().GetParamValue(CommandLineConstants::Locale);
	const std::string OverrideLocaleCode = FStringUtils::Narrow(CmdLocale);

	// Create platform instance
	EOS_Platform_Options PlatformOptions = {};
	PlatformOptions.ApiVersion = EOS_PLATFORM_OPTIONS_API_LATEST;
	PlatformOptions.bIsServer = FCommandLine::Get().HasFlagParam(CommandLineConstants::Server);
	PlatformOptions.EncryptionKey = SampleConstants::EncryptionKey;
	PlatformOptions.OverrideCountryCode = nullptr;
	PlatformOptions.OverrideLocaleCode = OverrideLocaleCode.empty() ? nullptr : OverrideLocaleCode.c_str();
	if (PlatformOptions.bIsServer)
	{
		PlatformOptions.Flags = EOS_PF_DISABLE_OVERLAY; // no overlay needed for server
	}
	else
	{
		PlatformOptions.Flags = EOS_PF_WINDOWS_ENABLE_OVERLAY_D3D9 | EOS_PF_WINDOWS_ENABLE_OVERLAY_D3D10 | EOS_PF_WINDOWS_ENABLE_OVERLAY_OPENGL; // Enable overlay support for D3D9/10 and OpenGL. This sample uses D3D11 or SDL.
	}	
	PlatformOptions.CacheDirectory = FUtils::GetTempDirectory();
	std::string ProductId = SampleConstants::ProductId;
	std::string SandboxId = SampleConstants::SandboxId;
	std::string DeploymentId = SampleConstants::DeploymentId;

	// Use Command Line vars to populate vars if they exist
	std::wstring CmdProductID = FCommandLine::Get().GetParamValue(CommandLineConstants::ProductId);
	if (!CmdProductID.empty())
	{
		ProductId = FStringUtils::Narrow(CmdProductID).c_str();
	}
	bHasInvalidParamProductId = ProductId.empty();
	if (bHasInvalidParamProductId)
	{
		FDebugLog::LogError(L"[EOS SDK] Product Id is empty, add your product id from Epic Games DevPortal to SampleConstants");
	}

	std::wstring CmdSandboxID = FCommandLine::Get().GetParamValue(CommandLineConstants::SandboxId);
	if (!CmdSandboxID.empty())
	{
		SandboxId = FStringUtils::Narrow(CmdSandboxID).c_str();
	}
	bHasInvalidParamSandboxId = SandboxId.empty();
	if (bHasInvalidParamSandboxId)
	{
		FDebugLog::LogError(L"[EOS SDK] Sandbox Id is empty, add your sandbox id from Epic Games DevPortal to SampleConstants");
	}

	std::wstring CmdDeploymentID = FCommandLine::Get().GetParamValue(CommandLineConstants::DeploymentId);
	if (!CmdDeploymentID.empty())
	{
		DeploymentId = FStringUtils::Narrow(CmdDeploymentID).c_str();
	}
	bHasInvalidParamDeploymentId = DeploymentId.empty();
	if (bHasInvalidParamDeploymentId)
	{
		FDebugLog::LogError(L"[EOS SDK] Deployment Id is empty, add your deployment id from Epic Games DevPortal to SampleConstants");
	}

	PlatformOptions.ProductId = ProductId.c_str();
	PlatformOptions.SandboxId = SandboxId.c_str();
	PlatformOptions.DeploymentId = DeploymentId.c_str();

	std::string ClientId = SampleConstants::ClientCredentialsId;
	std::string ClientSecret = SampleConstants::ClientCredentialsSecret;

	// Use Command Line vars to populate vars if they exist
	std::wstring CmdClientID = FCommandLine::Get().GetParamValue(CommandLineConstants::ClientId);
	if (!CmdClientID.empty())
	{
		ClientId = FStringUtils::Narrow(CmdClientID).c_str();
	}
	std::wstring CmdClientSecret = FCommandLine::Get().GetParamValue(CommandLineConstants::ClientSecret);
	if (!CmdClientSecret.empty())
	{
		ClientSecret = FStringUtils::Narrow(CmdClientSecret).c_str();
	}

	bHasInvalidParamClientCreds = false;
	if (!ClientId.empty() && !ClientSecret.empty())
	{
		PlatformOptions.ClientCredentials.ClientId = ClientId.c_str();
		PlatformOptions.ClientCredentials.ClientSecret = ClientSecret.c_str();
	}
	else if (!ClientId.empty() || !ClientSecret.empty())
	{
		bHasInvalidParamClientCreds = true;

		FDebugLog::LogError(L"[EOS SDK] Client credentials are invalid, check clientid and clientsecret in SampleConstants");
	}
	else
	{
		PlatformOptions.ClientCredentials.ClientId = nullptr;
		PlatformOptions.ClientCredentials.ClientSecret = nullptr;
	}

	if (bHasInvalidParamProductId ||
		bHasInvalidParamSandboxId ||
		bHasInvalidParamDeploymentId ||
		bHasInvalidParamClientCreds)
	{
		return false;
	}

	EOS_Platform_RTCOptions RtcOptions = { 0 };
	RtcOptions.ApiVersion = EOS_PLATFORM_RTCOPTIONS_API_LATEST;
	RtcOptions.BackgroundMode = EOS_ERTCBackgroundMode::EOS_RTCBM_LeaveRooms;

#ifdef _WIN32
	// Get absolute path for xaudio2_9redist.dll file
#ifdef DXTK
	wchar_t CurDir[MAX_PATH + 1] = {};
	::GetCurrentDirectoryW(MAX_PATH + 1u, CurDir);
	std::wstring BasePath = std::wstring(CurDir);
	std::string XAudio29DllPath = FStringUtils::Narrow(BasePath);
#endif
#ifdef EOS_DEMO_SDL
	std::string XAudio29DllPath = SDL_GetBasePath();
#endif // EOS_DEMO_SDL
	XAudio29DllPath.append("/xaudio2_9redist.dll");

	EOS_Windows_RTCOptions WindowsRtcOptions = { 0 };
	WindowsRtcOptions.ApiVersion = EOS_WINDOWS_RTCOPTIONS_API_LATEST;
	WindowsRtcOptions.XAudio29DllPath = XAudio29DllPath.c_str();
	RtcOptions.PlatformSpecificOptions = &WindowsRtcOptions;
#else
	RtcOptions.PlatformSpecificOptions = NULL;
#endif // _WIN32

	PlatformOptions.RTCOptions = &RtcOptions;

	if (!PlatformOptions.IntegratedPlatformOptionsContainerHandle)
	{
		EOS_IntegratedPlatform_CreateIntegratedPlatformOptionsContainerOptions CreateIntegratedPlatformOptionsContainerOptions = {};
		CreateIntegratedPlatformOptionsContainerOptions.ApiVersion = EOS_INTEGRATEDPLATFORM_CREATEINTEGRATEDPLATFORMOPTIONSCONTAINER_API_LATEST;

		EOS_EResult Result = EOS_IntegratedPlatform_CreateIntegratedPlatformOptionsContainer(&CreateIntegratedPlatformOptionsContainerOptions, &PlatformOptions.IntegratedPlatformOptionsContainerHandle);
		if (Result != EOS_EResult::EOS_Success)
		{
			FDebugLog::Log(L"Failed to create integrated platform options container: %ls", FStringUtils::Widen(EOS_EResult_ToString(Result)).c_str());
		}
	}

#if ALLOW_RESERVED_OPTIONS
	SetReservedPlatformOptions(PlatformOptions);
#else
	PlatformOptions.Reserved = NULL;
#endif // ALLOW_RESERVED_OPTIONS

	PlatformHandle = EOS_Platform_Create(&PlatformOptions);

	if (PlatformOptions.IntegratedPlatformOptionsContainerHandle)
	{
		EOS_IntegratedPlatformOptionsContainer_Release(PlatformOptions.IntegratedPlatformOptionsContainerHandle);
	}

	if (PlatformHandle == nullptr)
	{
		return false;
	}

	bIsInit = true;

	return true;
}

void FPlatform::Release()
{
	bIsInit = false;
	PlatformHandle = nullptr;
	bIsShuttingDown = true;
}

void FPlatform::Update()
{
	if (PlatformHandle)
	{
		EOS_Platform_Tick(PlatformHandle);
	}

	if (!bIsInit && !bIsShuttingDown)
	{
		if (!bHasShownCreateFailedError)
		{
			bHasShownCreateFailedError = true;
			FDebugLog::LogError(L"[EOS SDK] Platform Create Failed!");
		}
	}

	if (bHasInvalidParamProductId ||
		bHasInvalidParamSandboxId ||
		bHasInvalidParamDeploymentId ||
		bHasInvalidParamClientCreds)
	{
		if (!bHasShownInvalidParamsErrors)
		{
			bHasShownInvalidParamsErrors = true;

			FGameEvent PopupEvent(EGameEventType::ShowPopupDialog, L"One or more parameters required for EOS_Platform_Create are invalid. Check SampleConstants have been set up correctly.");
			FGame::Get().OnGameEvent(PopupEvent);
		}
	}
}
