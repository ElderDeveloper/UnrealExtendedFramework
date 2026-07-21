// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Input/ESteamInputSubsystem.h"
#include "Shared/ESteamLog.h"
#include "Shared/ESteamSDK.h"

#if WITH_EXTENDEDSTEAM_SDK
THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
THIRD_PARTY_INCLUDES_END

namespace
{
	/** Maps the Steamworks analog source mode enum to the Blueprint-facing enum. */
	EESteamInputSourceMode ToESteamSourceMode(EInputSourceMode Mode)
	{
		switch (Mode)
		{
		case k_EInputSourceMode_Dpad:           return EESteamInputSourceMode::Dpad;
		case k_EInputSourceMode_Buttons:        return EESteamInputSourceMode::Buttons;
		case k_EInputSourceMode_FourButtons:    return EESteamInputSourceMode::FourButtons;
		case k_EInputSourceMode_AbsoluteMouse:  return EESteamInputSourceMode::AbsoluteMouse;
		case k_EInputSourceMode_RelativeMouse:  return EESteamInputSourceMode::RelativeMouse;
		case k_EInputSourceMode_JoystickMove:   return EESteamInputSourceMode::JoystickMove;
		case k_EInputSourceMode_JoystickMouse:  return EESteamInputSourceMode::JoystickMouse;
		case k_EInputSourceMode_JoystickCamera: return EESteamInputSourceMode::JoystickCamera;
		case k_EInputSourceMode_ScrollWheel:    return EESteamInputSourceMode::ScrollWheel;
		case k_EInputSourceMode_Trigger:        return EESteamInputSourceMode::Trigger;
		case k_EInputSourceMode_TouchMenu:      return EESteamInputSourceMode::TouchMenu;
		case k_EInputSourceMode_MouseJoystick:  return EESteamInputSourceMode::MouseJoystick;
		case k_EInputSourceMode_MouseRegion:    return EESteamInputSourceMode::MouseRegion;
		case k_EInputSourceMode_RadialMenu:     return EESteamInputSourceMode::RadialMenu;
		case k_EInputSourceMode_SingleButton:   return EESteamInputSourceMode::SingleButton;
		case k_EInputSourceMode_Switches:       return EESteamInputSourceMode::Switches;
		default:                                return EESteamInputSourceMode::None;
		}
	}

	/** Maps the Blueprint haptic location enum to the Steamworks bitmask enum. */
	EControllerHapticLocation ToSteamHapticLocation(EESteamControllerHapticLocation Location)
	{
		switch (Location)
		{
		case EESteamControllerHapticLocation::Left:  return k_EControllerHapticLocation_Left;
		case EESteamControllerHapticLocation::Right: return k_EControllerHapticLocation_Right;
		default:                                     return k_EControllerHapticLocation_Both;
		}
	}
}

/**
 * Native Steam callback listeners; alive only while the Steam client API is initialized.
 * Registered unconditionally — device connect/disconnect callbacks only start firing after
 * ISteamInput::Init + EnableDeviceCallbacks (done in InitSteamInput), and are dispatched
 * from RunFrame / SteamAPI_RunCallbacks.
 */
class FESteamInputCallbacks
{
public:
	explicit FESteamInputCallbacks(UESteamInputSubsystem* InOwner)
		: Owner(InOwner)
		, DeviceConnectedCallback(this, &FESteamInputCallbacks::HandleDeviceConnected)
		, DeviceDisconnectedCallback(this, &FESteamInputCallbacks::HandleDeviceDisconnected)
	{
	}

private:
	void HandleDeviceConnected(SteamInputDeviceConnected_t* Data)
	{
		if (UESteamInputSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnDeviceConnected.Broadcast(static_cast<int64>(Data->m_ulConnectedDeviceHandle));
		}
	}

	void HandleDeviceDisconnected(SteamInputDeviceDisconnected_t* Data)
	{
		if (UESteamInputSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnDeviceDisconnected.Broadcast(static_cast<int64>(Data->m_ulDisconnectedDeviceHandle));
		}
	}

	TWeakObjectPtr<UESteamInputSubsystem> Owner;
	CCallback<FESteamInputCallbacks, SteamInputDeviceConnected_t> DeviceConnectedCallback;
	CCallback<FESteamInputCallbacks, SteamInputDeviceDisconnected_t> DeviceDisconnectedCallback;
};
#else
class FESteamInputCallbacks
{
};
#endif // WITH_EXTENDEDSTEAM_SDK

void UESteamInputSubsystem::Deinitialize()
{
	Super::Deinitialize();
	// HandleSteamClientShutdown already ran when Steam was up; this covers the Steam-down path.
	ShutdownSteamInput();
	Callbacks.Reset();
}

void UESteamInputSubsystem::HandleSteamClientInitialized()
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!Callbacks)
	{
		Callbacks = MakeShared<FESteamInputCallbacks>(this);
	}
#endif
}

void UESteamInputSubsystem::HandleSteamClientShutdown()
{
	ShutdownSteamInput();
	Callbacks.Reset();
}

bool UESteamInputSubsystem::InitSteamInput()
{
#if WITH_EXTENDEDSTEAM_SDK
	if (bSteamInputInitialized)
	{
		return true;
	}

	if (!IsSteamAvailable() || !SteamInput())
	{
		LogSteamUnavailable(TEXT("InitSteamInput"));
		return false;
	}

	// bExplicitlyCallRunFrame = false: the shared module's SteamAPI_RunCallbacks pump keeps
	// Steam Input updated even without our ticker; the explicit RunFrame ticker below refreshes
	// action data right before game code reads it each frame.
	if (!SteamInput()->Init(false))
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("InitSteamInput: ISteamInput::Init failed"));
		return false;
	}

	bSteamInputInitialized = true;

	// Opt into SteamInputDeviceConnected_t / SteamInputDeviceDisconnected_t; each controller
	// that is already connected immediately generates a connected callback.
	SteamInput()->EnableDeviceCallbacks();

	if (!RunFrameTickerHandle.IsValid())
	{
		RunFrameTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateUObject(this, &UESteamInputSubsystem::TickRunFrame));
	}

	UE_LOG(LogExtendedSteam, Log, TEXT("Steam Input initialized"));
	return true;
#else
	LogSteamUnavailable(TEXT("InitSteamInput"));
	return false;
#endif
}

void UESteamInputSubsystem::ShutdownSteamInput()
{
	RemoveRunFrameTicker();

#if WITH_EXTENDEDSTEAM_SDK
	if (bSteamInputInitialized && IsSteamAvailable() && SteamInput())
	{
		SteamInput()->Shutdown();
		UE_LOG(LogExtendedSteam, Log, TEXT("Steam Input shut down"));
	}
#endif

	bSteamInputInitialized = false;
}

bool UESteamInputSubsystem::TickRunFrame(float /*DeltaTime*/)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (bSteamInputInitialized && IsSteamAvailable() && SteamInput())
	{
		SteamInput()->RunFrame();
	}
#endif
	return true;
}

void UESteamInputSubsystem::RemoveRunFrameTicker()
{
	if (RunFrameTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(RunFrameTickerHandle);
		RunFrameTickerHandle.Reset();
	}
}

int32 UESteamInputSubsystem::GetConnectedControllers(TArray<int64>& OutHandles) const
{
	OutHandles.Reset();
#if WITH_EXTENDEDSTEAM_SDK
	if (bSteamInputInitialized && IsSteamAvailable() && SteamInput())
	{
		InputHandle_t Handles[STEAM_INPUT_MAX_COUNT];
		const int32 Count = SteamInput()->GetConnectedControllers(Handles);
		OutHandles.Reserve(Count);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			OutHandles.Add(static_cast<int64>(Handles[Index]));
		}
		return Count;
	}
#endif
	return 0;
}

EESteamInputType UESteamInputSubsystem::GetInputTypeForHandle(int64 ControllerHandle) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (bSteamInputInitialized && IsSteamAvailable() && SteamInput())
	{
		switch (SteamInput()->GetInputTypeForHandle(static_cast<InputHandle_t>(ControllerHandle)))
		{
		case k_ESteamInputType_SteamController:     return EESteamInputType::SteamController;
		case k_ESteamInputType_XBox360Controller:   return EESteamInputType::XBox360Controller;
		case k_ESteamInputType_XBoxOneController:   return EESteamInputType::XBoxOneController;
		case k_ESteamInputType_GenericGamepad:      return EESteamInputType::GenericGamepad;
		case k_ESteamInputType_PS4Controller:       return EESteamInputType::PS4Controller;
		case k_ESteamInputType_PS5Controller:       return EESteamInputType::PS5Controller;
		case k_ESteamInputType_SwitchProController: return EESteamInputType::SwitchProController;
		case k_ESteamInputType_MobileTouch:         return EESteamInputType::MobileTouch;
		case k_ESteamInputType_SteamDeckController: return EESteamInputType::SteamDeckController;
		default:                                    return EESteamInputType::Unknown;
		}
	}
#endif
	return EESteamInputType::Unknown;
}

int64 UESteamInputSubsystem::GetActionSetHandle(const FString& ActionSetName) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (bSteamInputInitialized && IsSteamAvailable() && SteamInput())
	{
		return static_cast<int64>(SteamInput()->GetActionSetHandle(TCHAR_TO_UTF8(*ActionSetName)));
	}
#endif
	return 0;
}

void UESteamInputSubsystem::ActivateActionSet(int64 ControllerHandle, int64 ActionSetHandle)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (bSteamInputInitialized && IsSteamAvailable() && SteamInput())
	{
		SteamInput()->ActivateActionSet(
			static_cast<InputHandle_t>(ControllerHandle),
			static_cast<InputActionSetHandle_t>(ActionSetHandle));
	}
#endif
}

int64 UESteamInputSubsystem::GetCurrentActionSet(int64 ControllerHandle) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (bSteamInputInitialized && IsSteamAvailable() && SteamInput())
	{
		return static_cast<int64>(SteamInput()->GetCurrentActionSet(static_cast<InputHandle_t>(ControllerHandle)));
	}
#endif
	return 0;
}

int64 UESteamInputSubsystem::GetDigitalActionHandle(const FString& ActionName) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (bSteamInputInitialized && IsSteamAvailable() && SteamInput())
	{
		return static_cast<int64>(SteamInput()->GetDigitalActionHandle(TCHAR_TO_UTF8(*ActionName)));
	}
#endif
	return 0;
}

FESteamDigitalActionData UESteamInputSubsystem::GetDigitalActionData(int64 ControllerHandle, int64 ActionHandle) const
{
	FESteamDigitalActionData Result;
#if WITH_EXTENDEDSTEAM_SDK
	if (bSteamInputInitialized && IsSteamAvailable() && SteamInput())
	{
		const InputDigitalActionData_t Data = SteamInput()->GetDigitalActionData(
			static_cast<InputHandle_t>(ControllerHandle),
			static_cast<InputDigitalActionHandle_t>(ActionHandle));
		Result.bState = Data.bState;
		Result.bActive = Data.bActive;
	}
#endif
	return Result;
}

int64 UESteamInputSubsystem::GetAnalogActionHandle(const FString& ActionName) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (bSteamInputInitialized && IsSteamAvailable() && SteamInput())
	{
		return static_cast<int64>(SteamInput()->GetAnalogActionHandle(TCHAR_TO_UTF8(*ActionName)));
	}
#endif
	return 0;
}

FESteamAnalogActionData UESteamInputSubsystem::GetAnalogActionData(int64 ControllerHandle, int64 ActionHandle) const
{
	FESteamAnalogActionData Result;
#if WITH_EXTENDEDSTEAM_SDK
	if (bSteamInputInitialized && IsSteamAvailable() && SteamInput())
	{
		const InputAnalogActionData_t Data = SteamInput()->GetAnalogActionData(
			static_cast<InputHandle_t>(ControllerHandle),
			static_cast<InputAnalogActionHandle_t>(ActionHandle));
		Result.Mode = ToESteamSourceMode(Data.eMode);
		Result.X = Data.x;
		Result.Y = Data.y;
		Result.bActive = Data.bActive;
	}
#endif
	return Result;
}

void UESteamInputSubsystem::TriggerVibration(int64 ControllerHandle, int32 LeftSpeed, int32 RightSpeed)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (bSteamInputInitialized && IsSteamAvailable() && SteamInput())
	{
		SteamInput()->TriggerVibration(
			static_cast<InputHandle_t>(ControllerHandle),
			static_cast<unsigned short>(FMath::Clamp(LeftSpeed, 0, 65535)),
			static_cast<unsigned short>(FMath::Clamp(RightSpeed, 0, 65535)));
	}
#endif
}

void UESteamInputSubsystem::ActivateActionSetLayer(int64 ControllerHandle, int64 ActionSetLayerHandle)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (bSteamInputInitialized && IsSteamAvailable() && SteamInput())
	{
		SteamInput()->ActivateActionSetLayer(
			static_cast<InputHandle_t>(ControllerHandle),
			static_cast<InputActionSetHandle_t>(ActionSetLayerHandle));
	}
#endif
}

void UESteamInputSubsystem::DeactivateActionSetLayer(int64 ControllerHandle, int64 ActionSetLayerHandle)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (bSteamInputInitialized && IsSteamAvailable() && SteamInput())
	{
		SteamInput()->DeactivateActionSetLayer(
			static_cast<InputHandle_t>(ControllerHandle),
			static_cast<InputActionSetHandle_t>(ActionSetLayerHandle));
	}
#endif
}

void UESteamInputSubsystem::DeactivateAllActionSetLayers(int64 ControllerHandle)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (bSteamInputInitialized && IsSteamAvailable() && SteamInput())
	{
		SteamInput()->DeactivateAllActionSetLayers(static_cast<InputHandle_t>(ControllerHandle));
	}
#endif
}

int32 UESteamInputSubsystem::GetActiveActionSetLayers(int64 ControllerHandle, TArray<int64>& OutLayerHandles) const
{
	OutLayerHandles.Reset();
#if WITH_EXTENDEDSTEAM_SDK
	if (bSteamInputInitialized && IsSteamAvailable() && SteamInput())
	{
		InputActionSetHandle_t Handles[STEAM_INPUT_MAX_ACTIVE_LAYERS];
		const int32 Count = SteamInput()->GetActiveActionSetLayers(static_cast<InputHandle_t>(ControllerHandle), Handles);
		OutLayerHandles.Reserve(Count);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			OutLayerHandles.Add(static_cast<int64>(Handles[Index]));
		}
		return Count;
	}
#endif
	return 0;
}

int32 UESteamInputSubsystem::GetDigitalActionOrigins(int64 ControllerHandle, int64 ActionSetHandle, int64 ActionHandle, TArray<int32>& OutOrigins) const
{
	OutOrigins.Reset();
#if WITH_EXTENDEDSTEAM_SDK
	if (bSteamInputInitialized && IsSteamAvailable() && SteamInput())
	{
		EInputActionOrigin Origins[STEAM_INPUT_MAX_ORIGINS];
		const int32 Count = SteamInput()->GetDigitalActionOrigins(
			static_cast<InputHandle_t>(ControllerHandle),
			static_cast<InputActionSetHandle_t>(ActionSetHandle),
			static_cast<InputDigitalActionHandle_t>(ActionHandle),
			Origins);
		OutOrigins.Reserve(Count);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			OutOrigins.Add(static_cast<int32>(Origins[Index]));
		}
		return Count;
	}
#endif
	return 0;
}

FString UESteamInputSubsystem::GetStringForDigitalActionName(int64 ActionHandle) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (bSteamInputInitialized && IsSteamAvailable() && SteamInput())
	{
		if (const char* Name = SteamInput()->GetStringForDigitalActionName(static_cast<InputDigitalActionHandle_t>(ActionHandle)))
		{
			return UTF8_TO_TCHAR(Name);
		}
	}
#endif
	return FString();
}

int32 UESteamInputSubsystem::GetAnalogActionOrigins(int64 ControllerHandle, int64 ActionSetHandle, int64 ActionHandle, TArray<int32>& OutOrigins) const
{
	OutOrigins.Reset();
#if WITH_EXTENDEDSTEAM_SDK
	if (bSteamInputInitialized && IsSteamAvailable() && SteamInput())
	{
		EInputActionOrigin Origins[STEAM_INPUT_MAX_ORIGINS];
		const int32 Count = SteamInput()->GetAnalogActionOrigins(
			static_cast<InputHandle_t>(ControllerHandle),
			static_cast<InputActionSetHandle_t>(ActionSetHandle),
			static_cast<InputAnalogActionHandle_t>(ActionHandle),
			Origins);
		OutOrigins.Reserve(Count);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			OutOrigins.Add(static_cast<int32>(Origins[Index]));
		}
		return Count;
	}
#endif
	return 0;
}

FString UESteamInputSubsystem::GetStringForAnalogActionName(int64 ActionHandle) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (bSteamInputInitialized && IsSteamAvailable() && SteamInput())
	{
		if (const char* Name = SteamInput()->GetStringForAnalogActionName(static_cast<InputAnalogActionHandle_t>(ActionHandle)))
		{
			return UTF8_TO_TCHAR(Name);
		}
	}
#endif
	return FString();
}

void UESteamInputSubsystem::StopAnalogActionMomentum(int64 ControllerHandle, int64 ActionHandle)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (bSteamInputInitialized && IsSteamAvailable() && SteamInput())
	{
		SteamInput()->StopAnalogActionMomentum(
			static_cast<InputHandle_t>(ControllerHandle),
			static_cast<InputAnalogActionHandle_t>(ActionHandle));
	}
#endif
}

FString UESteamInputSubsystem::GetGlyphPNGForActionOrigin(int32 Origin, EESteamInputGlyphSize Size, int32 Flags) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (bSteamInputInitialized && IsSteamAvailable() && SteamInput())
	{
		ESteamInputGlyphSize GlyphSize = k_ESteamInputGlyphSize_Small;
		switch (Size)
		{
		case EESteamInputGlyphSize::Medium: GlyphSize = k_ESteamInputGlyphSize_Medium; break;
		case EESteamInputGlyphSize::Large:  GlyphSize = k_ESteamInputGlyphSize_Large;  break;
		default:                            GlyphSize = k_ESteamInputGlyphSize_Small;  break;
		}
		if (const char* Path = SteamInput()->GetGlyphPNGForActionOrigin(
			static_cast<EInputActionOrigin>(Origin), GlyphSize, static_cast<uint32>(Flags)))
		{
			return UTF8_TO_TCHAR(Path);
		}
	}
#endif
	return FString();
}

FString UESteamInputSubsystem::GetGlyphSVGForActionOrigin(int32 Origin, int32 Flags) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (bSteamInputInitialized && IsSteamAvailable() && SteamInput())
	{
		if (const char* Path = SteamInput()->GetGlyphSVGForActionOrigin(
			static_cast<EInputActionOrigin>(Origin), static_cast<uint32>(Flags)))
		{
			return UTF8_TO_TCHAR(Path);
		}
	}
#endif
	return FString();
}

FString UESteamInputSubsystem::GetStringForActionOrigin(int32 Origin) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (bSteamInputInitialized && IsSteamAvailable() && SteamInput())
	{
		if (const char* Text = SteamInput()->GetStringForActionOrigin(static_cast<EInputActionOrigin>(Origin)))
		{
			return UTF8_TO_TCHAR(Text);
		}
	}
#endif
	return FString();
}

int32 UESteamInputSubsystem::GetGamepadIndexForController(int64 ControllerHandle) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (bSteamInputInitialized && IsSteamAvailable() && SteamInput())
	{
		return SteamInput()->GetGamepadIndexForController(static_cast<InputHandle_t>(ControllerHandle));
	}
#endif
	return -1;
}

int64 UESteamInputSubsystem::GetControllerForGamepadIndex(int32 Index) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (bSteamInputInitialized && IsSteamAvailable() && SteamInput())
	{
		return static_cast<int64>(SteamInput()->GetControllerForGamepadIndex(Index));
	}
#endif
	return 0;
}

FESteamInputMotionData UESteamInputSubsystem::GetMotionData(int64 ControllerHandle) const
{
	FESteamInputMotionData Result;
#if WITH_EXTENDEDSTEAM_SDK
	if (bSteamInputInitialized && IsSteamAvailable() && SteamInput())
	{
		const InputMotionData_t Data = SteamInput()->GetMotionData(static_cast<InputHandle_t>(ControllerHandle));
		Result.RotQuat = FQuat(Data.rotQuatX, Data.rotQuatY, Data.rotQuatZ, Data.rotQuatW);
		Result.Accel = FVector(Data.posAccelX, Data.posAccelY, Data.posAccelZ);
		Result.RotVel = FVector(Data.rotVelX, Data.rotVelY, Data.rotVelZ);
	}
#endif
	return Result;
}

bool UESteamInputSubsystem::GetDeviceBindingRevision(int64 ControllerHandle, int32& OutMajor, int32& OutMinor) const
{
	OutMajor = 0;
	OutMinor = 0;
#if WITH_EXTENDEDSTEAM_SDK
	if (bSteamInputInitialized && IsSteamAvailable() && SteamInput())
	{
		int Major = 0;
		int Minor = 0;
		if (SteamInput()->GetDeviceBindingRevision(static_cast<InputHandle_t>(ControllerHandle), &Major, &Minor))
		{
			OutMajor = Major;
			OutMinor = Minor;
			return true;
		}
	}
#endif
	return false;
}

int32 UESteamInputSubsystem::GetRemotePlaySessionID(int64 ControllerHandle) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (bSteamInputInitialized && IsSteamAvailable() && SteamInput())
	{
		return static_cast<int32>(SteamInput()->GetRemotePlaySessionID(static_cast<InputHandle_t>(ControllerHandle)));
	}
#endif
	return 0;
}

void UESteamInputSubsystem::TriggerVibrationExtended(int64 ControllerHandle, int32 LeftSpeed, int32 RightSpeed, int32 LeftTriggerSpeed, int32 RightTriggerSpeed)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (bSteamInputInitialized && IsSteamAvailable() && SteamInput())
	{
		SteamInput()->TriggerVibrationExtended(
			static_cast<InputHandle_t>(ControllerHandle),
			static_cast<unsigned short>(FMath::Clamp(LeftSpeed, 0, 65535)),
			static_cast<unsigned short>(FMath::Clamp(RightSpeed, 0, 65535)),
			static_cast<unsigned short>(FMath::Clamp(LeftTriggerSpeed, 0, 65535)),
			static_cast<unsigned short>(FMath::Clamp(RightTriggerSpeed, 0, 65535)));
	}
#endif
}

void UESteamInputSubsystem::TriggerSimpleHapticEvent(int64 ControllerHandle, EESteamControllerHapticLocation Location, int32 Intensity, int32 GainDB, int32 OtherIntensity, int32 OtherGainDB)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (bSteamInputInitialized && IsSteamAvailable() && SteamInput())
	{
		SteamInput()->TriggerSimpleHapticEvent(
			static_cast<InputHandle_t>(ControllerHandle),
			ToSteamHapticLocation(Location),
			static_cast<uint8>(FMath::Clamp(Intensity, 0, 255)),
			static_cast<char>(FMath::Clamp(GainDB, -127, 127)),
			static_cast<uint8>(FMath::Clamp(OtherIntensity, 0, 255)),
			static_cast<char>(FMath::Clamp(OtherGainDB, -127, 127)));
	}
#endif
}

void UESteamInputSubsystem::SetLEDColor(int64 ControllerHandle, uint8 R, uint8 G, uint8 B, int32 Flags)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (bSteamInputInitialized && IsSteamAvailable() && SteamInput())
	{
		SteamInput()->SetLEDColor(static_cast<InputHandle_t>(ControllerHandle), R, G, B, static_cast<unsigned int>(Flags));
	}
#endif
}

void UESteamInputSubsystem::Legacy_TriggerHapticPulse(int64 ControllerHandle, EESteamControllerPad Pad, int32 DurationMicroSec)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (bSteamInputInitialized && IsSteamAvailable() && SteamInput())
	{
		const ESteamControllerPad TargetPad = (Pad == EESteamControllerPad::Right) ? k_ESteamControllerPad_Right : k_ESteamControllerPad_Left;
		SteamInput()->Legacy_TriggerHapticPulse(
			static_cast<InputHandle_t>(ControllerHandle),
			TargetPad,
			static_cast<unsigned short>(FMath::Clamp(DurationMicroSec, 0, 65535)));
	}
#endif
}

bool UESteamInputSubsystem::ShowBindingPanel(int64 ControllerHandle)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!bSteamInputInitialized || !IsSteamAvailable() || !SteamInput())
	{
		LogSteamUnavailable(TEXT("ShowBindingPanel"));
		return false;
	}
	return SteamInput()->ShowBindingPanel(static_cast<InputHandle_t>(ControllerHandle));
#else
	LogSteamUnavailable(TEXT("ShowBindingPanel"));
	return false;
#endif
}
