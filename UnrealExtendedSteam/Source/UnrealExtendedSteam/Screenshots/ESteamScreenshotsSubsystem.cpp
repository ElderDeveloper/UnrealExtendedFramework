// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Screenshots/ESteamScreenshotsSubsystem.h"
#include "Shared/ESteamLog.h"
#include "Shared/ESteamSDK.h"

#if WITH_EXTENDEDSTEAM_SDK
THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
THIRD_PARTY_INCLUDES_END

/** Native Steam callback listeners; alive only while the Steam client API is initialized. */
class FESteamScreenshotsCallbacks
{
public:
	explicit FESteamScreenshotsCallbacks(UESteamScreenshotsSubsystem* InOwner)
		: Owner(InOwner)
		, ScreenshotRequested(this, &FESteamScreenshotsCallbacks::HandleScreenshotRequested)
		, ScreenshotReady(this, &FESteamScreenshotsCallbacks::HandleScreenshotReady)
	{
	}

private:
	void HandleScreenshotRequested(ScreenshotRequested_t* Data)
	{
		if (UESteamScreenshotsSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnScreenshotRequested.Broadcast();
		}
	}

	void HandleScreenshotReady(ScreenshotReady_t* Data)
	{
		if (UESteamScreenshotsSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnScreenshotReady.Broadcast(Data->m_eResult == k_EResultOK, static_cast<int32>(Data->m_hLocal));
		}
	}

	TWeakObjectPtr<UESteamScreenshotsSubsystem> Owner;
	CCallback<FESteamScreenshotsCallbacks, ScreenshotRequested_t> ScreenshotRequested;
	CCallback<FESteamScreenshotsCallbacks, ScreenshotReady_t> ScreenshotReady;
};
#else
class FESteamScreenshotsCallbacks
{
};
#endif // WITH_EXTENDEDSTEAM_SDK

void UESteamScreenshotsSubsystem::Deinitialize()
{
	Super::Deinitialize();
	Callbacks.Reset();
}

void UESteamScreenshotsSubsystem::HandleSteamClientInitialized()
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!Callbacks)
	{
		Callbacks = MakeShared<FESteamScreenshotsCallbacks>(this);
	}
#endif
}

void UESteamScreenshotsSubsystem::HandleSteamClientShutdown()
{
	Callbacks.Reset();
}

void UESteamScreenshotsSubsystem::TriggerScreenshot()
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamScreenshots())
	{
		LogSteamUnavailable(TEXT("TriggerScreenshot"));
		return;
	}
	SteamScreenshots()->TriggerScreenshot();
#else
	LogSteamUnavailable(TEXT("TriggerScreenshot"));
#endif
}

void UESteamScreenshotsSubsystem::HookScreenshots(bool bHook)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamScreenshots())
	{
		LogSteamUnavailable(TEXT("HookScreenshots"));
		return;
	}
	SteamScreenshots()->HookScreenshots(bHook);
#else
	LogSteamUnavailable(TEXT("HookScreenshots"));
#endif
}

bool UESteamScreenshotsSubsystem::IsScreenshotsHooked() const
{
#if WITH_EXTENDEDSTEAM_SDK
	return IsSteamAvailable() && SteamScreenshots() && SteamScreenshots()->IsScreenshotsHooked();
#else
	return false;
#endif
}

int32 UESteamScreenshotsSubsystem::AddScreenshotToLibrary(const FString& PngOrJpgFilePath, const FString& ThumbnailPath, int32 Width, int32 Height)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamScreenshots())
	{
		LogSteamUnavailable(TEXT("AddScreenshotToLibrary"));
		return 0;
	}

	const FTCHARToUTF8 FileUtf8(*PngOrJpgFilePath);
	const FTCHARToUTF8 ThumbnailUtf8(*ThumbnailPath);
	const ScreenshotHandle Handle = SteamScreenshots()->AddScreenshotToLibrary(
		FileUtf8.Get(),
		ThumbnailPath.IsEmpty() ? nullptr : ThumbnailUtf8.Get(),
		Width,
		Height);

	if (Handle == INVALID_SCREENSHOT_HANDLE)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("AddScreenshotToLibrary failed for '%s'"), *PngOrJpgFilePath);
		return 0;
	}
	return static_cast<int32>(Handle);
#else
	LogSteamUnavailable(TEXT("AddScreenshotToLibrary"));
	return 0;
#endif
}

int32 UESteamScreenshotsSubsystem::WriteScreenshot(const TArray<uint8>& RGB, int32 Width, int32 Height)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamScreenshots())
	{
		LogSteamUnavailable(TEXT("WriteScreenshot"));
		return 0;
	}

	// Steam expects tightly packed 24-bit RGB: exactly Width * Height * 3 bytes.
	const int64 Expected = static_cast<int64>(Width) * static_cast<int64>(Height) * 3;
	if (Width <= 0 || Height <= 0 || RGB.Num() != Expected)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("WriteScreenshot: RGB buffer is %d bytes, expected %lld (Width*Height*3)"), RGB.Num(), Expected);
		return 0;
	}

	const ScreenshotHandle Handle = SteamScreenshots()->WriteScreenshot(
		const_cast<uint8*>(RGB.GetData()),
		static_cast<uint32>(RGB.Num()),
		Width,
		Height);

	if (Handle == INVALID_SCREENSHOT_HANDLE)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("WriteScreenshot failed"));
		return 0;
	}
	return static_cast<int32>(Handle);
#else
	LogSteamUnavailable(TEXT("WriteScreenshot"));
	return 0;
#endif
}

int32 UESteamScreenshotsSubsystem::AddVRScreenshotToLibrary(EESteamVRScreenshotType Type, const FString& Filename, const FString& VRFilename)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamScreenshots())
	{
		LogSteamUnavailable(TEXT("AddVRScreenshotToLibrary"));
		return 0;
	}

	EVRScreenshotType VRType = k_EVRScreenshotType_None;
	switch (Type)
	{
	case EESteamVRScreenshotType::Mono:           VRType = k_EVRScreenshotType_Mono;           break;
	case EESteamVRScreenshotType::Stereo:         VRType = k_EVRScreenshotType_Stereo;         break;
	case EESteamVRScreenshotType::MonoCubemap:    VRType = k_EVRScreenshotType_MonoCubemap;    break;
	case EESteamVRScreenshotType::MonoPanorama:   VRType = k_EVRScreenshotType_MonoPanorama;   break;
	case EESteamVRScreenshotType::StereoPanorama: VRType = k_EVRScreenshotType_StereoPanorama; break;
	default:                                      VRType = k_EVRScreenshotType_None;           break;
	}

	const FTCHARToUTF8 FileUtf8(*Filename);
	const FTCHARToUTF8 VRFileUtf8(*VRFilename);
	const ScreenshotHandle Handle = SteamScreenshots()->AddVRScreenshotToLibrary(VRType, FileUtf8.Get(), VRFileUtf8.Get());

	if (Handle == INVALID_SCREENSHOT_HANDLE)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("AddVRScreenshotToLibrary failed for '%s'"), *Filename);
		return 0;
	}
	return static_cast<int32>(Handle);
#else
	LogSteamUnavailable(TEXT("AddVRScreenshotToLibrary"));
	return 0;
#endif
}

bool UESteamScreenshotsSubsystem::SetLocation(int32 ScreenshotHandle, const FString& Location)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamScreenshots() || ScreenshotHandle == 0)
	{
		return false;
	}
	return SteamScreenshots()->SetLocation(static_cast<::ScreenshotHandle>(ScreenshotHandle), TCHAR_TO_UTF8(*Location));
#else
	return false;
#endif
}

bool UESteamScreenshotsSubsystem::TagUser(int32 ScreenshotHandle, FESteamId UserId)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamScreenshots() || ScreenshotHandle == 0)
	{
		return false;
	}
	return SteamScreenshots()->TagUser(static_cast<::ScreenshotHandle>(ScreenshotHandle), CSteamID(UserId.Value));
#else
	return false;
#endif
}

bool UESteamScreenshotsSubsystem::TagPublishedFile(int32 ScreenshotHandle, int64 PublishedFileId)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamScreenshots() || ScreenshotHandle == 0)
	{
		return false;
	}
	return SteamScreenshots()->TagPublishedFile(
		static_cast<::ScreenshotHandle>(ScreenshotHandle),
		static_cast<PublishedFileId_t>(PublishedFileId));
#else
	return false;
#endif
}
