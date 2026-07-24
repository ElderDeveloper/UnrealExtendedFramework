// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Utils/ESteamUtilsSubsystem.h"
#include "Shared/ESteamLog.h"
#include "Shared/ESteamSDK.h"

#if WITH_EXTENDEDSTEAM_SDK
THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
THIRD_PARTY_INCLUDES_END

namespace ESteamUtilsHelpers
{
	/** Reads the buffered gamepad text entry into an FString (empty when none). */
	FString ReadEnteredGamepadText()
	{
		if (!SteamUtils())
		{
			return FString();
		}

		const uint32 Length = SteamUtils()->GetEnteredGamepadTextLength();
		if (Length == 0)
		{
			return FString();
		}

		TArray<ANSICHAR> Buffer;
		Buffer.SetNumZeroed(static_cast<int32>(Length) + 1);
		if (SteamUtils()->GetEnteredGamepadTextInput(Buffer.GetData(), static_cast<uint32>(Buffer.Num())))
		{
			return UTF8_TO_TCHAR(Buffer.GetData());
		}
		return FString();
	}
}

/** Native Steam callback listeners; alive only while the Steam client API is initialized. */
class FESteamUtilsCallbacks
{
public:
	explicit FESteamUtilsCallbacks(UESteamUtilsSubsystem* InOwner)
		: Owner(InOwner)
		, SteamShutdown(this, &FESteamUtilsCallbacks::HandleSteamShutdown)
		, LowBattery(this, &FESteamUtilsCallbacks::HandleLowBattery)
		, IPCountry(this, &FESteamUtilsCallbacks::HandleIPCountry)
		, GamepadTextDismissed(this, &FESteamUtilsCallbacks::HandleGamepadTextDismissed)
		, FloatingGamepadTextDismissed(this, &FESteamUtilsCallbacks::HandleFloatingGamepadTextDismissed)
		, AppResumingFromSuspend(this, &FESteamUtilsCallbacks::HandleAppResumingFromSuspend)
	{
	}

private:
	void HandleSteamShutdown(SteamShutdown_t* Data)
	{
		if (UESteamUtilsSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnSteamShutdownRequested.Broadcast();
		}
	}

	void HandleLowBattery(LowBatteryPower_t* Data)
	{
		if (UESteamUtilsSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnLowBatteryPower.Broadcast(static_cast<int32>(Data->m_nMinutesBatteryLeft));
		}
	}

	void HandleIPCountry(IPCountry_t* Data)
	{
		if (UESteamUtilsSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnIPCountryChanged.Broadcast();
		}
	}

	void HandleGamepadTextDismissed(GamepadTextInputDismissed_t* Data)
	{
		if (UESteamUtilsSubsystem* Subsystem = Owner.Get())
		{
			const FString EnteredText = Data->m_bSubmitted ? ESteamUtilsHelpers::ReadEnteredGamepadText() : FString();
			Subsystem->OnGamepadTextInputDismissed.Broadcast(Data->m_bSubmitted, EnteredText);
		}
	}

	void HandleFloatingGamepadTextDismissed(FloatingGamepadTextInputDismissed_t* Data)
	{
		if (UESteamUtilsSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnFloatingGamepadTextInputDismissed.Broadcast();
		}
	}

	void HandleAppResumingFromSuspend(AppResumingFromSuspend_t* Data)
	{
		if (UESteamUtilsSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnAppResumingFromSuspend.Broadcast();
		}
	}

	TWeakObjectPtr<UESteamUtilsSubsystem> Owner;
	CCallback<FESteamUtilsCallbacks, SteamShutdown_t> SteamShutdown;
	CCallback<FESteamUtilsCallbacks, LowBatteryPower_t> LowBattery;
	CCallback<FESteamUtilsCallbacks, IPCountry_t> IPCountry;
	CCallback<FESteamUtilsCallbacks, GamepadTextInputDismissed_t> GamepadTextDismissed;
	CCallback<FESteamUtilsCallbacks, FloatingGamepadTextInputDismissed_t> FloatingGamepadTextDismissed;
	CCallback<FESteamUtilsCallbacks, AppResumingFromSuspend_t> AppResumingFromSuspend;
};
#else
class FESteamUtilsCallbacks
{
};
#endif // WITH_EXTENDEDSTEAM_SDK

void UESteamUtilsSubsystem::Deinitialize()
{
	Super::Deinitialize();
	Callbacks.Reset();
}

void UESteamUtilsSubsystem::HandleSteamClientInitialized()
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!Callbacks)
	{
		Callbacks = MakeShared<FESteamUtilsCallbacks>(this);
	}
#endif
}

void UESteamUtilsSubsystem::HandleSteamClientShutdown()
{
	Callbacks.Reset();
}

int32 UESteamUtilsSubsystem::GetAppId() const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamUtils())
	{
		return static_cast<int32>(SteamUtils()->GetAppID());
	}
#endif
	return 0;
}

FString UESteamUtilsSubsystem::GetIPCountry() const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamUtils())
	{
		return UTF8_TO_TCHAR(SteamUtils()->GetIPCountry());
	}
#endif
	return FString();
}

int64 UESteamUtilsSubsystem::GetServerRealTime() const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamUtils())
	{
		return static_cast<int64>(SteamUtils()->GetServerRealTime());
	}
#endif
	return 0;
}

int32 UESteamUtilsSubsystem::GetSecondsSinceAppActive() const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamUtils())
	{
		return static_cast<int32>(SteamUtils()->GetSecondsSinceAppActive());
	}
#endif
	return 0;
}

int32 UESteamUtilsSubsystem::GetSecondsSinceComputerActive() const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamUtils())
	{
		return static_cast<int32>(SteamUtils()->GetSecondsSinceComputerActive());
	}
#endif
	return 0;
}

int32 UESteamUtilsSubsystem::GetConnectedUniverse() const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamUtils())
	{
		return static_cast<int32>(SteamUtils()->GetConnectedUniverse());
	}
#endif
	return 0;
}

bool UESteamUtilsSubsystem::IsOverlayEnabled() const
{
#if WITH_EXTENDEDSTEAM_SDK
	return IsSteamAvailable() && SteamUtils() && SteamUtils()->IsOverlayEnabled();
#else
	return false;
#endif
}

bool UESteamUtilsSubsystem::OverlayNeedsPresent() const
{
#if WITH_EXTENDEDSTEAM_SDK
	return IsSteamAvailable() && SteamUtils() && SteamUtils()->BOverlayNeedsPresent();
#else
	return false;
#endif
}

bool UESteamUtilsSubsystem::IsSteamInBigPictureMode() const
{
#if WITH_EXTENDEDSTEAM_SDK
	return IsSteamAvailable() && SteamUtils() && SteamUtils()->IsSteamInBigPictureMode();
#else
	return false;
#endif
}

bool UESteamUtilsSubsystem::IsSteamRunningOnSteamDeck() const
{
#if WITH_EXTENDEDSTEAM_SDK && ESTEAM_SDK_AT_LEAST(154)
	return IsSteamAvailable() && SteamUtils() && SteamUtils()->IsSteamRunningOnSteamDeck();
#else
	return false;
#endif
}

bool UESteamUtilsSubsystem::IsSteamChinaLauncher() const
{
#if WITH_EXTENDEDSTEAM_SDK
	return IsSteamAvailable() && SteamUtils() && SteamUtils()->IsSteamChinaLauncher();
#else
	return false;
#endif
}

bool UESteamUtilsSubsystem::IsSteamRunningInVR() const
{
#if WITH_EXTENDEDSTEAM_SDK
	return IsSteamAvailable() && SteamUtils() && SteamUtils()->IsSteamRunningInVR();
#else
	return false;
#endif
}

void UESteamUtilsSubsystem::StartVRDashboard()
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamUtils())
	{
		SteamUtils()->StartVRDashboard();
	}
#endif
}

bool UESteamUtilsSubsystem::IsVRHeadsetStreamingEnabled() const
{
#if WITH_EXTENDEDSTEAM_SDK
	return IsSteamAvailable() && SteamUtils() && SteamUtils()->IsVRHeadsetStreamingEnabled();
#else
	return false;
#endif
}

void UESteamUtilsSubsystem::SetVRHeadsetStreamingEnabled(bool bEnabled)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamUtils())
	{
		SteamUtils()->SetVRHeadsetStreamingEnabled(bEnabled);
	}
#endif
}

FString UESteamUtilsSubsystem::GetSteamUILanguage() const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamUtils())
	{
		return UTF8_TO_TCHAR(SteamUtils()->GetSteamUILanguage());
	}
#endif
	return FString();
}

int32 UESteamUtilsSubsystem::GetCurrentBatteryPower() const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamUtils())
	{
		return static_cast<int32>(SteamUtils()->GetCurrentBatteryPower());
	}
#endif
	return 0;
}

void UESteamUtilsSubsystem::SetOverlayNotificationPosition(EESteamNotificationPosition Position)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUtils())
	{
		LogSteamUnavailable(TEXT("SetOverlayNotificationPosition"));
		return;
	}

	ENotificationPosition SdkPosition = k_EPositionBottomRight;
	switch (Position)
	{
	case EESteamNotificationPosition::TopLeft:     SdkPosition = k_EPositionTopLeft; break;
	case EESteamNotificationPosition::TopRight:    SdkPosition = k_EPositionTopRight; break;
	case EESteamNotificationPosition::BottomLeft:  SdkPosition = k_EPositionBottomLeft; break;
	case EESteamNotificationPosition::BottomRight: SdkPosition = k_EPositionBottomRight; break;
	}
	SteamUtils()->SetOverlayNotificationPosition(SdkPosition);
#endif
}

void UESteamUtilsSubsystem::SetOverlayNotificationInset(int32 HorizontalInset, int32 VerticalInset)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamUtils())
	{
		SteamUtils()->SetOverlayNotificationInset(HorizontalInset, VerticalInset);
	}
#endif
}

void UESteamUtilsSubsystem::SetGameLauncherMode(bool bLauncherMode)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamUtils())
	{
		SteamUtils()->SetGameLauncherMode(bLauncherMode);
	}
#endif
}

EESteamIPv6ConnectivityState UESteamUtilsSubsystem::GetIPv6ConnectivityState(EESteamIPv6ConnectivityProtocol Protocol) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamUtils())
	{
		ESteamIPv6ConnectivityProtocol SdkProtocol = k_ESteamIPv6ConnectivityProtocol_Invalid;
		switch (Protocol)
		{
		case EESteamIPv6ConnectivityProtocol::HTTP: SdkProtocol = k_ESteamIPv6ConnectivityProtocol_HTTP; break;
		case EESteamIPv6ConnectivityProtocol::UDP:  SdkProtocol = k_ESteamIPv6ConnectivityProtocol_UDP; break;
		default:                                    SdkProtocol = k_ESteamIPv6ConnectivityProtocol_Invalid; break;
		}

		switch (SteamUtils()->GetIPv6ConnectivityState(SdkProtocol))
		{
		case k_ESteamIPv6ConnectivityState_Good: return EESteamIPv6ConnectivityState::Good;
		case k_ESteamIPv6ConnectivityState_Bad:  return EESteamIPv6ConnectivityState::Bad;
		default:                                 return EESteamIPv6ConnectivityState::Unknown;
		}
	}
#endif
	return EESteamIPv6ConnectivityState::Unknown;
}

bool UESteamUtilsSubsystem::ShowGamepadTextInput(EESteamGamepadTextInputLineMode LineMode, EESteamGamepadTextInputMode CharMode, const FString& Description, int32 MaxChars, const FString& ExistingText)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUtils())
	{
		LogSteamUnavailable(TEXT("ShowGamepadTextInput"));
		return false;
	}

	const EGamepadTextInputMode SdkInputMode = (CharMode == EESteamGamepadTextInputMode::Password)
		? k_EGamepadTextInputModePassword
		: k_EGamepadTextInputModeNormal;
	const EGamepadTextInputLineMode SdkLineMode = (LineMode == EESteamGamepadTextInputLineMode::MultipleLines)
		? k_EGamepadTextInputLineModeMultipleLines
		: k_EGamepadTextInputLineModeSingleLine;

	return SteamUtils()->ShowGamepadTextInput(
		SdkInputMode,
		SdkLineMode,
		TCHAR_TO_UTF8(*Description),
		static_cast<uint32>(FMath::Max(0, MaxChars)),
		TCHAR_TO_UTF8(*ExistingText));
#else
	return false;
#endif
}

bool UESteamUtilsSubsystem::ShowFloatingGamepadTextInput(EESteamFloatingGamepadTextInputMode KeyboardMode, int32 X, int32 Y, int32 Width, int32 Height)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUtils())
	{
		LogSteamUnavailable(TEXT("ShowFloatingGamepadTextInput"));
		return false;
	}

	EFloatingGamepadTextInputMode SdkMode = k_EFloatingGamepadTextInputModeModeSingleLine;
	switch (KeyboardMode)
	{
	case EESteamFloatingGamepadTextInputMode::MultipleLines: SdkMode = k_EFloatingGamepadTextInputModeModeMultipleLines; break;
	case EESteamFloatingGamepadTextInputMode::Email:         SdkMode = k_EFloatingGamepadTextInputModeModeEmail; break;
	case EESteamFloatingGamepadTextInputMode::Numeric:       SdkMode = k_EFloatingGamepadTextInputModeModeNumeric; break;
	default:                                                 SdkMode = k_EFloatingGamepadTextInputModeModeSingleLine; break;
	}

	return SteamUtils()->ShowFloatingGamepadTextInput(SdkMode, X, Y, Width, Height);
#else
	return false;
#endif
}

int32 UESteamUtilsSubsystem::GetEnteredGamepadTextLength() const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamUtils())
	{
		return static_cast<int32>(SteamUtils()->GetEnteredGamepadTextLength());
	}
#endif
	return 0;
}

FString UESteamUtilsSubsystem::GetEnteredGamepadTextInput() const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamUtils())
	{
		return ESteamUtilsHelpers::ReadEnteredGamepadText();
	}
#endif
	return FString();
}

bool UESteamUtilsSubsystem::DismissGamepadTextInput()
{
#if ESTEAM_SDK_AT_LEAST(164)
	if (IsSteamAvailable() && SteamUtils())
	{
		return SteamUtils()->DismissGamepadTextInput();
	}
#endif
	return false;
}

bool UESteamUtilsSubsystem::DismissFloatingGamepadTextInput()
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamUtils())
	{
		return SteamUtils()->DismissFloatingGamepadTextInput();
	}
#endif
	return false;
}

bool UESteamUtilsSubsystem::InitFilterText()
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamUtils())
	{
		return SteamUtils()->InitFilterText(0);
	}
#endif
	return false;
}

FString UESteamUtilsSubsystem::FilterText(EESteamTextFilteringContext Context, FESteamId SourceSteamId, const FString& InputText) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamUtils())
	{
		ETextFilteringContext SdkContext = k_ETextFilteringContextUnknown;
		switch (Context)
		{
		case EESteamTextFilteringContext::GameContent: SdkContext = k_ETextFilteringContextGameContent; break;
		case EESteamTextFilteringContext::Chat:        SdkContext = k_ETextFilteringContextChat; break;
		case EESteamTextFilteringContext::Name:        SdkContext = k_ETextFilteringContextName; break;
		default:                                       SdkContext = k_ETextFilteringContextUnknown; break;
		}

		const auto InputUtf8 = StringCast<UTF8CHAR>(*InputText);
		// The output buffer must be at least strlen(input)+1 bytes; filtering never grows the text.
		TArray<ANSICHAR> OutBuffer;
		OutBuffer.SetNumZeroed(InputUtf8.Length() + 1);

		SteamUtils()->FilterText(
			SdkContext,
			CSteamID(SourceSteamId.Value),
			(const ANSICHAR*)InputUtf8.Get(),
			OutBuffer.GetData(),
			static_cast<uint32>(OutBuffer.Num()));
		return UTF8_TO_TCHAR(OutBuffer.GetData());
	}
#endif
	return InputText;
}

bool UESteamUtilsSubsystem::GetImageSize(int32 ImageHandle, int32& OutWidth, int32& OutHeight) const
{
	OutWidth = 0;
	OutHeight = 0;
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamUtils() && ImageHandle != 0)
	{
		uint32 Width = 0;
		uint32 Height = 0;
		if (SteamUtils()->GetImageSize(ImageHandle, &Width, &Height))
		{
			OutWidth = static_cast<int32>(Width);
			OutHeight = static_cast<int32>(Height);
			return true;
		}
	}
#endif
	return false;
}

bool UESteamUtilsSubsystem::GetImageRGBA(int32 ImageHandle, TArray<uint8>& OutRGBA, int32& OutWidth, int32& OutHeight) const
{
	OutRGBA.Reset();
	OutWidth = 0;
	OutHeight = 0;
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamUtils() && ImageHandle != 0)
	{
		uint32 Width = 0;
		uint32 Height = 0;
		if (!SteamUtils()->GetImageSize(ImageHandle, &Width, &Height) || Width == 0 || Height == 0)
		{
			return false;
		}

		// Widen before multiplying: Width*Height*4 evaluated in uint32 could wrap for a corrupt or
		// pathologically large size, yielding a bogus (possibly negative) allocation.
		const int64 BufferSize64 = static_cast<int64>(Width) * static_cast<int64>(Height) * 4;
		if (BufferSize64 <= 0 || BufferSize64 > MAX_int32)
		{
			return false;
		}
		const int32 BufferSize = static_cast<int32>(BufferSize64);
		OutRGBA.SetNumUninitialized(BufferSize);
		if (SteamUtils()->GetImageRGBA(ImageHandle, OutRGBA.GetData(), BufferSize))
		{
			OutWidth = static_cast<int32>(Width);
			OutHeight = static_cast<int32>(Height);
			return true;
		}
		OutRGBA.Reset();
	}
#endif
	return false;
}
