// Fill out your copyright notice in the Description page of Project Settings.

#include "EFAudioDeviceSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UnrealExtendedFramework.h"
#include "VoiceChat.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <mmdeviceapi.h>
#include <propsys.h>
#include <functiondiscoverykeys_devpkey.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

namespace
{
	constexpr float DeviceMonitorIntervalSeconds = 2.0f;
	const FString DefaultDeviceId = TEXT("Default");

	bool IsSameDeviceIdentity(const FEFAudioDeviceInfo& Left, const FEFAudioDeviceInfo& Right)
	{
		return Left.DeviceType == Right.DeviceType
			&& Left.DeviceID == Right.DeviceID
			&& Left.Backend == Right.Backend;
	}
}

void UEFAudioDeviceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	bIsInitialized = true;
	LastError.Reset();
	ActiveOutputDeviceID = DefaultDeviceId;
	ActiveInputDeviceID = DefaultDeviceId;

	AcquireVoiceChatUser();
	RefreshDeviceCache(false);
	StartDeviceMonitoring();
}

void UEFAudioDeviceSubsystem::Deinitialize()
{
	StopDeviceMonitoring();
	ReleaseVoiceChatUser();
	OutputDevices.Empty();
	InputDevices.Empty();
	ActiveOutputDeviceID = DefaultDeviceId;
	ActiveInputDeviceID = DefaultDeviceId;
	LastError.Reset();
	bIsInitialized = false;

	Super::Deinitialize();
}

UEFAudioDeviceSubsystem* UEFAudioDeviceSubsystem::GetAudioDeviceSubsystem(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	if (const UGameInstance* GameInstance = Cast<UGameInstance>(WorldContextObject))
	{
		return GameInstance->GetSubsystem<UEFAudioDeviceSubsystem>();
	}

	if (!GEngine)
	{
		return nullptr;
	}

	if (const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull))
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			return GameInstance->GetSubsystem<UEFAudioDeviceSubsystem>();
		}
	}

	return nullptr;
}

void UEFAudioDeviceSubsystem::RestartDeviceMonitoring()
{
	StopDeviceMonitoring();
	StartDeviceMonitoring();
}

void UEFAudioDeviceSubsystem::RefreshDeviceList()
{
	RefreshDeviceCache(true);
	StartDeviceMonitoring();
}

TArray<FEFAudioDeviceInfo> UEFAudioDeviceSubsystem::GetAllDevices() const
{
	return BuildAllDevices();
}

TArray<FEFAudioDeviceInfo> UEFAudioDeviceSubsystem::GetOutputDevices() const
{
	return OutputDevices;
}

TArray<FEFAudioDeviceInfo> UEFAudioDeviceSubsystem::GetInputDevices() const
{
	return InputDevices;
}

FEFAudioDeviceInfo UEFAudioDeviceSubsystem::GetDefaultOutputDevice() const
{
	return FindDefaultDevice(EEFAudioDeviceType::Output);
}

FEFAudioDeviceInfo UEFAudioDeviceSubsystem::GetDefaultInputDevice() const
{
	return FindDefaultDevice(EEFAudioDeviceType::Input);
}

FEFAudioDeviceInfo UEFAudioDeviceSubsystem::GetActiveOutputDevice() const
{
	return FindActiveDevice(EEFAudioDeviceType::Output);
}

FEFAudioDeviceInfo UEFAudioDeviceSubsystem::GetActiveInputDevice() const
{
	return FindActiveDevice(EEFAudioDeviceType::Input);
}

bool UEFAudioDeviceSubsystem::SetOutputDevice(const FString& DeviceID)
{
	return SelectOutputDevice(DeviceID) == EEFAudioDeviceSelectionResult::Success;
}

bool UEFAudioDeviceSubsystem::SetInputDevice(const FString& DeviceID)
{
	return SelectInputDevice(DeviceID) == EEFAudioDeviceSelectionResult::Success;
}

EEFAudioDeviceSelectionResult UEFAudioDeviceSubsystem::SelectOutputDevice(const FString& DeviceID)
{
	return SelectDevice(DeviceID, EEFAudioDeviceType::Output);
}

EEFAudioDeviceSelectionResult UEFAudioDeviceSubsystem::SelectInputDevice(const FString& DeviceID)
{
	return SelectDevice(DeviceID, EEFAudioDeviceType::Input);
}

FEFAudioDeviceCapabilities UEFAudioDeviceSubsystem::GetCapabilities() const
{
	return BuildCapabilities();
}

FString UEFAudioDeviceSubsystem::GetLastError() const
{
	return LastError;
}

bool UEFAudioDeviceSubsystem::IsInputDeviceSelectionSupported() const
{
	return HasSelectableDevice(EEFAudioDeviceType::Input);
}

bool UEFAudioDeviceSubsystem::IsOutputDeviceSelectionSupported() const
{
	return HasSelectableDevice(EEFAudioDeviceType::Output);
}

UWorld* UEFAudioDeviceSubsystem::ResolveWorld() const
{
	if (UWorld* World = GetWorld())
	{
		return World;
	}

	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		return GameInstance->GetWorld();
	}

	return nullptr;
}

void UEFAudioDeviceSubsystem::RefreshDeviceCache(bool bBroadcastChanges)
{
	const TArray<FEFAudioDeviceInfo> PreviousDevices = BuildAllDevices();

	OutputDevices.Empty();
	InputDevices.Empty();

	AcquireVoiceChatUser();
	EnumerateVoiceChatDevices();
	EnumeratePlatformDevices();
	AddPlatformDefaultFallbacks();

	const TArray<FEFAudioDeviceInfo> CurrentDevices = BuildAllDevices();
	if (bBroadcastChanges)
	{
		BroadcastDisconnectedDevices(PreviousDevices, CurrentDevices);
		OnAudioDevicesChanged.Broadcast(CurrentDevices);
	}
}

void UEFAudioDeviceSubsystem::StartDeviceMonitoring()
{
	if (UWorld* World = ResolveWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		if (!TimerManager.IsTimerActive(DeviceMonitorTimerHandle))
		{
			TimerManager.SetTimer(
				DeviceMonitorTimerHandle,
				this,
				&UEFAudioDeviceSubsystem::CheckDeviceConnectivity,
				DeviceMonitorIntervalSeconds,
				true);
		}
	}
}

void UEFAudioDeviceSubsystem::StopDeviceMonitoring()
{
	if (UWorld* World = ResolveWorld())
	{
		World->GetTimerManager().ClearTimer(DeviceMonitorTimerHandle);
	}
}

void UEFAudioDeviceSubsystem::CheckDeviceConnectivity()
{
	const TArray<FEFAudioDeviceInfo> PreviousDevices = BuildAllDevices();

	OutputDevices.Empty();
	InputDevices.Empty();
	AcquireVoiceChatUser();
	EnumerateVoiceChatDevices();
	EnumeratePlatformDevices();
	AddPlatformDefaultFallbacks();

	const TArray<FEFAudioDeviceInfo> CurrentDevices = BuildAllDevices();
	if (!AreDeviceListsEquivalent(PreviousDevices, CurrentDevices))
	{
		BroadcastDisconnectedDevices(PreviousDevices, CurrentDevices);
		OnAudioDevicesChanged.Broadcast(CurrentDevices);
	}
}

void UEFAudioDeviceSubsystem::AcquireVoiceChatUser()
{
	if (VoiceChatUser)
	{
		return;
	}

	IVoiceChat* VoiceChat = IVoiceChat::Get();
	if (!VoiceChat)
	{
		return;
	}

	VoiceChatUser = VoiceChat->CreateUser();
	bOwnsVoiceChatUser = VoiceChatUser != nullptr;
}

void UEFAudioDeviceSubsystem::ReleaseVoiceChatUser()
{
	if (!VoiceChatUser || !bOwnsVoiceChatUser)
	{
		VoiceChatUser = nullptr;
		bOwnsVoiceChatUser = false;
		return;
	}

	if (IVoiceChat* VoiceChat = IVoiceChat::Get())
	{
		VoiceChat->ReleaseUser(VoiceChatUser);
	}

	VoiceChatUser = nullptr;
	bOwnsVoiceChatUser = false;
}

void UEFAudioDeviceSubsystem::EnumerateVoiceChatDevices()
{
	if (!VoiceChatUser)
	{
		return;
	}

	for (const FVoiceChatDeviceInfo& RawDevice : VoiceChatUser->GetAvailableInputDeviceInfos())
	{
		FEFAudioDeviceInfo DeviceInfo;
		DeviceInfo.DeviceName = RawDevice.DisplayName.IsEmpty() ? RawDevice.Id : RawDevice.DisplayName;
		DeviceInfo.DeviceID = NormalizeDeviceId(RawDevice.Id);
		DeviceInfo.bIsDefaultDevice = IsDefaultDeviceId(DeviceInfo.DeviceID);
		DeviceInfo.bIsInputDevice = true;
		DeviceInfo.bIsConnected = true;
		DeviceInfo.bCanBeSelected = true;
		DeviceInfo.DeviceType = EEFAudioDeviceType::Input;
		DeviceInfo.Backend = EEFAudioDeviceBackend::UnrealVoiceChat;
		DeviceInfo.BackendName = GetBackendName(DeviceInfo.Backend);
		AddOrUpdateDevice(DeviceInfo);
	}

	for (const FVoiceChatDeviceInfo& RawDevice : VoiceChatUser->GetAvailableOutputDeviceInfos())
	{
		FEFAudioDeviceInfo DeviceInfo;
		DeviceInfo.DeviceName = RawDevice.DisplayName.IsEmpty() ? RawDevice.Id : RawDevice.DisplayName;
		DeviceInfo.DeviceID = NormalizeDeviceId(RawDevice.Id);
		DeviceInfo.bIsDefaultDevice = IsDefaultDeviceId(DeviceInfo.DeviceID);
		DeviceInfo.bIsInputDevice = false;
		DeviceInfo.bIsConnected = true;
		DeviceInfo.bCanBeSelected = true;
		DeviceInfo.DeviceType = EEFAudioDeviceType::Output;
		DeviceInfo.Backend = EEFAudioDeviceBackend::UnrealVoiceChat;
		DeviceInfo.BackendName = GetBackendName(DeviceInfo.Backend);
		AddOrUpdateDevice(DeviceInfo);
	}
}

void UEFAudioDeviceSubsystem::EnumeratePlatformDevices()
{
#if PLATFORM_WINDOWS
	EnumerateWindowsCoreAudioDevices();
#endif
}

void UEFAudioDeviceSubsystem::AddPlatformDefaultFallbacks()
{
	if (!FindDevice(DefaultDeviceId, EEFAudioDeviceType::Output))
	{
		FEFAudioDeviceInfo OutputDefault;
		OutputDefault.DeviceName = TEXT("System Default Output");
		OutputDefault.DeviceID = DefaultDeviceId;
		OutputDefault.bIsDefaultDevice = true;
		OutputDefault.bIsInputDevice = false;
		OutputDefault.bIsConnected = true;
		OutputDefault.bCanBeSelected = true;
		OutputDefault.DeviceType = EEFAudioDeviceType::Output;
		OutputDefault.Backend = EEFAudioDeviceBackend::PlatformDefault;
		OutputDefault.BackendName = GetBackendName(OutputDefault.Backend);
		AddOrUpdateDevice(OutputDefault);
	}

	if (!FindDevice(DefaultDeviceId, EEFAudioDeviceType::Input))
	{
		FEFAudioDeviceInfo InputDefault;
		InputDefault.DeviceName = TEXT("System Default Input");
		InputDefault.DeviceID = DefaultDeviceId;
		InputDefault.bIsDefaultDevice = true;
		InputDefault.bIsInputDevice = true;
		InputDefault.bIsConnected = true;
		InputDefault.bCanBeSelected = true;
		InputDefault.DeviceType = EEFAudioDeviceType::Input;
		InputDefault.Backend = EEFAudioDeviceBackend::PlatformDefault;
		InputDefault.BackendName = GetBackendName(InputDefault.Backend);
		AddOrUpdateDevice(InputDefault);
	}
}

EEFAudioDeviceSelectionResult UEFAudioDeviceSubsystem::SelectDevice(const FString& DeviceID, EEFAudioDeviceType DeviceType)
{
	LastError.Reset();
	const FString NormalizedDeviceID = NormalizeDeviceId(DeviceID);
	const FEFAudioDeviceInfo* DeviceInfo = FindDevice(NormalizedDeviceID, DeviceType);

	if (!DeviceInfo)
	{
		SetLastError(FString::Printf(TEXT("Audio device '%s' was not found on %s."), *NormalizedDeviceID, *GetPlatformName()));
		return EEFAudioDeviceSelectionResult::InvalidDevice;
	}

	if (DeviceInfo->Backend == EEFAudioDeviceBackend::PlatformDefault && IsDefaultDeviceId(NormalizedDeviceID))
	{
		if (DeviceType == EEFAudioDeviceType::Input)
		{
			ActiveInputDeviceID = NormalizedDeviceID;
		}
		else
		{
			ActiveOutputDeviceID = NormalizedDeviceID;
		}

		OnActiveAudioDeviceChanged.Broadcast(DeviceType, *DeviceInfo);
		StartDeviceMonitoring();
		return EEFAudioDeviceSelectionResult::Success;
	}

	if (!DeviceInfo->bCanBeSelected)
	{
		SetLastError(FString::Printf(
			TEXT("Audio device '%s' is discoverable through %s, but this backend cannot switch devices at runtime."),
			*DeviceInfo->DeviceName,
			*DeviceInfo->BackendName));
		return EEFAudioDeviceSelectionResult::Unsupported;
	}

	if (!TrySelectVoiceChatDevice(NormalizedDeviceID, DeviceType))
	{
		SetLastError(TEXT("Unreal VoiceChat is not available for audio device selection on this platform or configuration."));
		return EEFAudioDeviceSelectionResult::BackendUnavailable;
	}

	if (DeviceType == EEFAudioDeviceType::Input)
	{
		ActiveInputDeviceID = NormalizedDeviceID;
	}
	else
	{
		ActiveOutputDeviceID = NormalizedDeviceID;
	}

	OnActiveAudioDeviceChanged.Broadcast(DeviceType, *DeviceInfo);
	StartDeviceMonitoring();
	return EEFAudioDeviceSelectionResult::Success;
}

bool UEFAudioDeviceSubsystem::TrySelectVoiceChatDevice(const FString& DeviceID, EEFAudioDeviceType DeviceType)
{
	AcquireVoiceChatUser();
	if (!VoiceChatUser)
	{
		return false;
	}

	const FString VoiceChatDeviceID = IsDefaultDeviceId(DeviceID) ? FString() : DeviceID;
	if (DeviceType == EEFAudioDeviceType::Input)
	{
		VoiceChatUser->SetInputDeviceId(VoiceChatDeviceID);
	}
	else
	{
		VoiceChatUser->SetOutputDeviceId(VoiceChatDeviceID);
	}

	return true;
}

void UEFAudioDeviceSubsystem::AddOrUpdateDevice(const FEFAudioDeviceInfo& DeviceInfo)
{
	TArray<FEFAudioDeviceInfo>& Devices = DeviceInfo.DeviceType == EEFAudioDeviceType::Input ? InputDevices : OutputDevices;

	for (FEFAudioDeviceInfo& ExistingDevice : Devices)
	{
		if (ExistingDevice.DeviceID == DeviceInfo.DeviceID)
		{
			if (!ExistingDevice.bCanBeSelected && DeviceInfo.bCanBeSelected)
			{
				ExistingDevice = DeviceInfo;
			}
			return;
		}
	}

	Devices.Add(DeviceInfo);
}

const FEFAudioDeviceInfo* UEFAudioDeviceSubsystem::FindDevice(const FString& DeviceID, EEFAudioDeviceType DeviceType) const
{
	const TArray<FEFAudioDeviceInfo>& Devices = DeviceType == EEFAudioDeviceType::Input ? InputDevices : OutputDevices;
	const FString NormalizedDeviceID = NormalizeDeviceId(DeviceID);

	return Devices.FindByPredicate([&NormalizedDeviceID](const FEFAudioDeviceInfo& DeviceInfo)
	{
		return DeviceInfo.DeviceID == NormalizedDeviceID;
	});
}

FEFAudioDeviceInfo UEFAudioDeviceSubsystem::FindDefaultDevice(EEFAudioDeviceType DeviceType) const
{
	const TArray<FEFAudioDeviceInfo>& Devices = DeviceType == EEFAudioDeviceType::Input ? InputDevices : OutputDevices;

	if (const FEFAudioDeviceInfo* DefaultDevice = Devices.FindByPredicate([](const FEFAudioDeviceInfo& DeviceInfo)
	{
		return DeviceInfo.bIsDefaultDevice;
	}))
	{
		return *DefaultDevice;
	}

	FEFAudioDeviceInfo EmptyDevice;
	EmptyDevice.DeviceType = DeviceType;
	EmptyDevice.bIsInputDevice = DeviceType == EEFAudioDeviceType::Input;
	return EmptyDevice;
}

FEFAudioDeviceInfo UEFAudioDeviceSubsystem::FindActiveDevice(EEFAudioDeviceType DeviceType) const
{
	const FString& ActiveDeviceID = DeviceType == EEFAudioDeviceType::Input ? ActiveInputDeviceID : ActiveOutputDeviceID;
	if (const FEFAudioDeviceInfo* ActiveDevice = FindDevice(ActiveDeviceID, DeviceType))
	{
		return *ActiveDevice;
	}

	return FindDefaultDevice(DeviceType);
}

TArray<FEFAudioDeviceInfo> UEFAudioDeviceSubsystem::BuildAllDevices() const
{
	TArray<FEFAudioDeviceInfo> AllDevices;
	AllDevices.Reserve(OutputDevices.Num() + InputDevices.Num());
	AllDevices.Append(OutputDevices);
	AllDevices.Append(InputDevices);
	return AllDevices;
}

FEFAudioDeviceCapabilities UEFAudioDeviceSubsystem::BuildCapabilities() const
{
	FEFAudioDeviceCapabilities Capabilities;
	Capabilities.PlatformName = GetPlatformName();
	Capabilities.bCanEnumerateInputDevices = HasRealDevice(EEFAudioDeviceType::Input);
	Capabilities.bCanEnumerateOutputDevices = HasRealDevice(EEFAudioDeviceType::Output);
	Capabilities.bCanSelectInputDevice = HasSelectableDevice(EEFAudioDeviceType::Input);
	Capabilities.bCanSelectOutputDevice = HasSelectableDevice(EEFAudioDeviceType::Output);
	Capabilities.bUsesUnrealVoiceChat = VoiceChatUser != nullptr;
	Capabilities.bUsesNativePlatformEnumeration = OutputDevices.ContainsByPredicate([](const FEFAudioDeviceInfo& DeviceInfo)
	{
		return DeviceInfo.Backend == EEFAudioDeviceBackend::WindowsCoreAudio;
	}) || InputDevices.ContainsByPredicate([](const FEFAudioDeviceInfo& DeviceInfo)
	{
		return DeviceInfo.Backend == EEFAudioDeviceBackend::WindowsCoreAudio;
	});
	Capabilities.Notes = GetPlatformSupportNotes();
	return Capabilities;
}

bool UEFAudioDeviceSubsystem::HasSelectableDevice(EEFAudioDeviceType DeviceType) const
{
	const TArray<FEFAudioDeviceInfo>& Devices = DeviceType == EEFAudioDeviceType::Input ? InputDevices : OutputDevices;
	return Devices.ContainsByPredicate([](const FEFAudioDeviceInfo& DeviceInfo)
	{
		return DeviceInfo.bCanBeSelected;
	});
}

bool UEFAudioDeviceSubsystem::HasRealDevice(EEFAudioDeviceType DeviceType) const
{
	const TArray<FEFAudioDeviceInfo>& Devices = DeviceType == EEFAudioDeviceType::Input ? InputDevices : OutputDevices;
	return Devices.ContainsByPredicate([](const FEFAudioDeviceInfo& DeviceInfo)
	{
		return DeviceInfo.Backend != EEFAudioDeviceBackend::PlatformDefault;
	});
}

bool UEFAudioDeviceSubsystem::AreDeviceListsEquivalent(const TArray<FEFAudioDeviceInfo>& PreviousDevices, const TArray<FEFAudioDeviceInfo>& CurrentDevices) const
{
	if (PreviousDevices.Num() != CurrentDevices.Num())
	{
		return false;
	}

	for (const FEFAudioDeviceInfo& PreviousDevice : PreviousDevices)
	{
		if (!CurrentDevices.ContainsByPredicate([&PreviousDevice](const FEFAudioDeviceInfo& CurrentDevice)
		{
			return IsSameDeviceIdentity(PreviousDevice, CurrentDevice)
				&& PreviousDevice.bIsConnected == CurrentDevice.bIsConnected
				&& PreviousDevice.bCanBeSelected == CurrentDevice.bCanBeSelected;
		}))
		{
			return false;
		}
	}

	return true;
}

void UEFAudioDeviceSubsystem::BroadcastDisconnectedDevices(const TArray<FEFAudioDeviceInfo>& PreviousDevices, const TArray<FEFAudioDeviceInfo>& CurrentDevices)
{
	for (const FEFAudioDeviceInfo& PreviousDevice : PreviousDevices)
	{
		if (!CurrentDevices.ContainsByPredicate([&PreviousDevice](const FEFAudioDeviceInfo& CurrentDevice)
		{
			return IsSameDeviceIdentity(PreviousDevice, CurrentDevice);
		}))
		{
			OnDeviceDisconnected.Broadcast(PreviousDevice);
		}
	}
}

FString UEFAudioDeviceSubsystem::GetPlatformName() const
{
#if PLATFORM_WINDOWS
	return TEXT("Windows");
#elif PLATFORM_MAC
	return TEXT("macOS");
#elif PLATFORM_IOS
	return TEXT("iOS");
#elif PLATFORM_ANDROID
	return TEXT("Android");
#elif PLATFORM_XBOXONE || PLATFORM_XSX
	return TEXT("Xbox");
#elif PLATFORM_PS4 || PLATFORM_PS5
	return TEXT("PlayStation");
#elif PLATFORM_SWITCH
	return TEXT("Nintendo Switch");
#elif PLATFORM_LINUX
	return TEXT("Linux / Steam Deck");
#else
	return FPlatformProperties::PlatformName();
#endif
}

FString UEFAudioDeviceSubsystem::GetPlatformSupportNotes() const
{
#if PLATFORM_WINDOWS
	return TEXT("Windows supports native CoreAudio discovery. Runtime device selection is routed through Unreal VoiceChat when a provider exposes it; changing the global Windows default device is intentionally not attempted.");
#elif PLATFORM_MAC
	return TEXT("macOS routing is treated as platform default unless an Unreal VoiceChat provider exposes selectable devices.");
#elif PLATFORM_IOS
	return TEXT("iPhone/iPad audio routes are controlled by AVAudioSession and user hardware state. The manager exposes platform default plus VoiceChat provider devices when available.");
#elif PLATFORM_ANDROID
	return TEXT("Android audio routing is controlled by AudioManager and connected hardware policy. The manager exposes platform default plus VoiceChat provider devices when available.");
#elif PLATFORM_XBOXONE || PLATFORM_XSX
	return TEXT("Xbox audio device routing is platform controlled. Game code should use the platform default or devices exposed by Unreal VoiceChat.");
#elif PLATFORM_PS4 || PLATFORM_PS5
	return TEXT("PlayStation audio device routing is platform controlled. Game code should use the platform default or devices exposed by Unreal VoiceChat.");
#elif PLATFORM_SWITCH
	return TEXT("Nintendo Switch audio device routing is platform controlled. Game code should use the platform default or devices exposed by Unreal VoiceChat.");
#elif PLATFORM_LINUX
	return TEXT("Linux and Steam Deck routing varies between PulseAudio, PipeWire, and ALSA. The manager exposes platform default plus VoiceChat provider devices when available.");
#else
	return TEXT("This platform is treated as platform-default audio unless an Unreal VoiceChat provider exposes selectable devices.");
#endif
}

FString UEFAudioDeviceSubsystem::GetBackendName(EEFAudioDeviceBackend Backend) const
{
	switch (Backend)
	{
	case EEFAudioDeviceBackend::PlatformDefault:
		return TEXT("Platform Default");
	case EEFAudioDeviceBackend::UnrealVoiceChat:
		return TEXT("Unreal VoiceChat");
	case EEFAudioDeviceBackend::WindowsCoreAudio:
		return TEXT("Windows CoreAudio");
	default:
		return TEXT("Unknown");
	}
}

FString UEFAudioDeviceSubsystem::NormalizeDeviceId(const FString& DeviceID) const
{
	return DeviceID.IsEmpty() ? DefaultDeviceId : DeviceID;
}

bool UEFAudioDeviceSubsystem::IsDefaultDeviceId(const FString& DeviceID) const
{
	return DeviceID.IsEmpty() || DeviceID.Equals(DefaultDeviceId, ESearchCase::IgnoreCase);
}

void UEFAudioDeviceSubsystem::SetLastError(const FString& ErrorMessage)
{
	LastError = ErrorMessage;
	UE_LOG(LogExtendedFramework, Warning, TEXT("EFAudioDeviceSubsystem: %s"), *LastError);
}

#if PLATFORM_WINDOWS
void UEFAudioDeviceSubsystem::EnumerateWindowsCoreAudioDevices()
{
	HRESULT CoInitializeResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	const bool bShouldUninitializeCOM = SUCCEEDED(CoInitializeResult);
	if (FAILED(CoInitializeResult) && CoInitializeResult != RPC_E_CHANGED_MODE)
	{
		return;
	}

	IMMDeviceEnumerator* DeviceEnumerator = nullptr;
	HRESULT Result = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&DeviceEnumerator));
	if (SUCCEEDED(Result) && DeviceEnumerator)
	{
		struct FWindowsEndpointEnumeration
		{
			static FString GetDeviceId(IMMDevice* Device)
			{
				LPWSTR RawDeviceID = nullptr;
				if (FAILED(Device->GetId(&RawDeviceID)) || !RawDeviceID)
				{
					return FString();
				}

				FString DeviceID(RawDeviceID);
				CoTaskMemFree(RawDeviceID);
				return DeviceID;
			}

			static FString GetDeviceName(IMMDevice* Device, const FString& FallbackName)
			{
				IPropertyStore* PropertyStore = nullptr;
				if (FAILED(Device->OpenPropertyStore(STGM_READ, &PropertyStore)) || !PropertyStore)
				{
					return FallbackName;
				}

				PROPVARIANT FriendlyName;
				PropVariantInit(&FriendlyName);
				FString DeviceName = FallbackName;
				if (SUCCEEDED(PropertyStore->GetValue(PKEY_Device_FriendlyName, &FriendlyName)) && FriendlyName.vt == VT_LPWSTR && FriendlyName.pwszVal)
				{
					DeviceName = FriendlyName.pwszVal;
				}

				PropVariantClear(&FriendlyName);
				PropertyStore->Release();
				return DeviceName;
			}
		};

		auto EnumerateEndpoints = [this, DeviceEnumerator](EDataFlow DataFlow, EEFAudioDeviceType DeviceType)
		{
			FString DefaultEndpointID;
			IMMDevice* DefaultEndpoint = nullptr;
			if (SUCCEEDED(DeviceEnumerator->GetDefaultAudioEndpoint(DataFlow, eConsole, &DefaultEndpoint)) && DefaultEndpoint)
			{
				DefaultEndpointID = FWindowsEndpointEnumeration::GetDeviceId(DefaultEndpoint);
				DefaultEndpoint->Release();
			}

			IMMDeviceCollection* DeviceCollection = nullptr;
			if (FAILED(DeviceEnumerator->EnumAudioEndpoints(DataFlow, DEVICE_STATE_ACTIVE, &DeviceCollection)) || !DeviceCollection)
			{
				return;
			}

			UINT DeviceCount = 0;
			DeviceCollection->GetCount(&DeviceCount);
			for (UINT DeviceIndex = 0; DeviceIndex < DeviceCount; ++DeviceIndex)
			{
				IMMDevice* Device = nullptr;
				if (FAILED(DeviceCollection->Item(DeviceIndex, &Device)) || !Device)
				{
					continue;
				}

				const FString DeviceID = FWindowsEndpointEnumeration::GetDeviceId(Device);
				if (!DeviceID.IsEmpty())
				{
					FEFAudioDeviceInfo DeviceInfo;
					DeviceInfo.DeviceName = FWindowsEndpointEnumeration::GetDeviceName(Device, DeviceID);
					DeviceInfo.DeviceID = DeviceID;
					DeviceInfo.bIsDefaultDevice = DeviceID == DefaultEndpointID;
					DeviceInfo.bIsInputDevice = DeviceType == EEFAudioDeviceType::Input;
					DeviceInfo.bIsConnected = true;
					DeviceInfo.bCanBeSelected = false;
					DeviceInfo.DeviceType = DeviceType;
					DeviceInfo.Backend = EEFAudioDeviceBackend::WindowsCoreAudio;
					DeviceInfo.BackendName = GetBackendName(DeviceInfo.Backend);
					AddOrUpdateDevice(DeviceInfo);
				}

				Device->Release();
			}

			DeviceCollection->Release();
		};

		EnumerateEndpoints(eRender, EEFAudioDeviceType::Output);
		EnumerateEndpoints(eCapture, EEFAudioDeviceType::Input);
		DeviceEnumerator->Release();
	}

	if (bShouldUninitializeCOM)
	{
		CoUninitialize();
	}
}
#endif