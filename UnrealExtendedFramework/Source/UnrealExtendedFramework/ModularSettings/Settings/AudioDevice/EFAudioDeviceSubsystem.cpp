// Copyright Moon Punch Games. All Rights Reserved.

#include "EFAudioDeviceSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "AudioMixerBlueprintLibrary.h"
#include "Async/Async.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "TimerManager.h"
#include "UnrealExtendedFramework.h"
#include "Voice.h"
#include "VoiceChat.h"
#include "Interfaces/VoiceCapture.h"
#if PLATFORM_WINDOWS && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <endpointvolume.h>
#include <mmdeviceapi.h>
#include <propsys.h>
#include <functiondiscoverykeys_devpkey.h>
#include "Microsoft/COMPointer.h"
#include "Windows/HideWindowsPlatformTypes.h"
// These pull in <windows.h>, whose RemoveDirectory macro UE's PostWindowsApi.h does not undefine (it does
// CopyFile, MoveFile and the rest). Left defined, it renames UEFFileLibrary::RemoveDirectory to
// RemoveDirectoryW in later files of the same unity blob, and the link fails.
#undef RemoveDirectory
#endif

#if PLATFORM_WINDOWS
/** The meter interfaces stay in the same game-thread apartment until invalidation/deinitialization. */
struct FEFAudioWindowsMeterCache
{
	struct FEndpoint
	{
		TComPtr<IMMDevice> Device;
		TComPtr<IAudioMeterInformation> Meter;
	};

	~FEFAudioWindowsMeterCache()
	{
		check(IsInGameThread());
		Endpoints.Empty();
		Enumerator = nullptr;
		if (bOwnsApartment) CoUninitialize();
	}

	bool EnsureEnumerator()
	{
		check(IsInGameThread());
		if (!bApartmentReady)
		{
			const HRESULT Result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
			if (FAILED(Result) && Result != RPC_E_CHANGED_MODE) return false;
			bOwnsApartment = SUCCEEDED(Result);
			bApartmentReady = true;
		}
		if (!Enumerator.IsValid())
		{
			CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&Enumerator));
		}
		return Enumerator.IsValid();
	}

	bool TryReadPeak(const FEFAudioDeviceInfo& DeviceInfo, EEFAudioDeviceType DeviceType, float& OutPeak);

	TComPtr<IMMDeviceEnumerator> Enumerator;
	TMap<FString, FEndpoint> Endpoints;
	bool bOwnsApartment = false;
	bool bApartmentReady = false;
};
#endif

namespace
{
	constexpr float DeviceMonitorIntervalSeconds = 2.0f;
	constexpr float InputLevelCaptureRetryIntervalSeconds = 0.35f;
	constexpr int32 InputLevelCaptureMaxRetryAttempts = 12;
	const FString DefaultDeviceId = TEXT("Default");
	constexpr float InputLevelBoost = 10.0f;
	constexpr float OutputLevelBoost = 6.0f;

	bool IsSameDeviceIdentity(const FEFAudioDeviceInfo& Left, const FEFAudioDeviceInfo& Right)
	{
		return Left.DeviceType == Right.DeviceType
			&& Left.DeviceID == Right.DeviceID
			&& Left.Backend == Right.Backend;
	}

	FString NormalizeEndpointDisplayName(const FString& DeviceName)
	{
		FString NormalizedName = DeviceName.ToLower().TrimStartAndEnd();
		NormalizedName.ReplaceInline(TEXT("default - "), TEXT(""));
		NormalizedName.ReplaceInline(TEXT("system default "), TEXT(""));
		NormalizedName.ReplaceInline(TEXT("  "), TEXT(" "));
		return NormalizedName;
	}

	bool AreLikelySameEndpointName(const FString& LeftName, const FString& RightName)
	{
		const FString Left = NormalizeEndpointDisplayName(LeftName);
		const FString Right = NormalizeEndpointDisplayName(RightName);
		return !Left.IsEmpty()
			&& !Right.IsEmpty()
			&& (Left.Equals(Right, ESearchCase::IgnoreCase) || Left.Contains(Right) || Right.Contains(Left));
	}

	FString MakeDeviceAliasKey(EEFAudioDeviceType DeviceType, const FString& DeviceID)
	{
		return FString::Printf(TEXT("%d:%s"), static_cast<int32>(DeviceType), *DeviceID);
	}

#if PLATFORM_WINDOWS
	struct FWindowsAudioDeviceSnapshot
	{
		TArray<FEFAudioDeviceInfo> Devices;
		bool bOutputComplete = false;
		bool bInputComplete = false;

		void RetainIncompleteFlows(const TArray<FEFAudioDeviceInfo>& Previous)
		{
			const auto IsIncomplete = [this](const FEFAudioDeviceInfo& Device)
			{
				return Device.DeviceType == EEFAudioDeviceType::Input ? !bInputComplete : !bOutputComplete;
			};
			Devices.RemoveAll(IsIncomplete);
			for (const FEFAudioDeviceInfo& Device : Previous)
			{
				if (Device.Backend == EEFAudioDeviceBackend::WindowsCoreAudio && IsIncomplete(Device)) Devices.Add(Device);
			}
		}
	};

	FString GetWindowsDeviceId(IMMDevice* Device)
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

	FString GetWindowsDeviceName(IMMDevice* Device, const FString& FallbackName)
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

	FString ResolveWindowsEndpointId(IMMDeviceEnumerator* DeviceEnumerator, const FEFAudioDeviceInfo& DeviceInfo, EEFAudioDeviceType DeviceType)
	{
		const EDataFlow DataFlow = DeviceType == EEFAudioDeviceType::Input ? eCapture : eRender;
		if (DeviceInfo.bIsDefaultDevice || DeviceInfo.DeviceID.Equals(DefaultDeviceId, ESearchCase::IgnoreCase))
		{
			IMMDevice* DefaultEndpoint = nullptr;
			if (SUCCEEDED(DeviceEnumerator->GetDefaultAudioEndpoint(DataFlow, eConsole, &DefaultEndpoint)) && DefaultEndpoint)
			{
				const FString DefaultEndpointID = GetWindowsDeviceId(DefaultEndpoint);
				DefaultEndpoint->Release();
				return DefaultEndpointID;
			}

			return FString();
		}

		if (DeviceInfo.Backend == EEFAudioDeviceBackend::WindowsCoreAudio)
		{
			return DeviceInfo.DeviceID;
		}

		IMMDeviceCollection* DeviceCollection = nullptr;
		if (FAILED(DeviceEnumerator->EnumAudioEndpoints(DataFlow, DEVICE_STATE_ACTIVE, &DeviceCollection)) || !DeviceCollection)
		{
			return FString();
		}

		FString ResolvedDeviceID;
		UINT DeviceCount = 0;
		DeviceCollection->GetCount(&DeviceCount);
		for (UINT DeviceIndex = 0; DeviceIndex < DeviceCount; ++DeviceIndex)
		{
			IMMDevice* Device = nullptr;
			if (FAILED(DeviceCollection->Item(DeviceIndex, &Device)) || !Device)
			{
				continue;
			}

			const FString CandidateID = GetWindowsDeviceId(Device);
			const FString CandidateName = GetWindowsDeviceName(Device, CandidateID);
			if (DeviceInfo.DeviceID == CandidateID || CandidateName.Equals(DeviceInfo.DeviceName, ESearchCase::IgnoreCase))
			{
				ResolvedDeviceID = CandidateID;
				Device->Release();
				break;
			}

			Device->Release();
		}

		DeviceCollection->Release();
		return ResolvedDeviceID;
	}

	/** Every COM interface and its apartment stay on the calling thread. No UObject is accessed here. */
	FWindowsAudioDeviceSnapshot ReadWindowsAudioDevices()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(EFAudioDevices_WindowsEnumeration);
		FWindowsAudioDeviceSnapshot Snapshot;
		const HRESULT CoInitializeResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		const bool bShouldUninitializeCOM = SUCCEEDED(CoInitializeResult);
		if (FAILED(CoInitializeResult) && CoInitializeResult != RPC_E_CHANGED_MODE)
		{
			return Snapshot;
		}

		IMMDeviceEnumerator* DeviceEnumerator = nullptr;
		const HRESULT Result = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&DeviceEnumerator));
		if (SUCCEEDED(Result) && DeviceEnumerator)
		{
			auto EnumerateEndpoints = [&Snapshot, DeviceEnumerator](EDataFlow DataFlow, EEFAudioDeviceType DeviceType)
			{
				FString DefaultEndpointID;
				IMMDevice* DefaultEndpoint = nullptr;
				const HRESULT DefaultResult = DeviceEnumerator->GetDefaultAudioEndpoint(DataFlow, eConsole, &DefaultEndpoint);
				// No default is a valid state, including after the last endpoint is unplugged.
				bool bDefaultKnown = DefaultResult == HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
				if (SUCCEEDED(DefaultResult) && DefaultEndpoint)
				{
					DefaultEndpointID = GetWindowsDeviceId(DefaultEndpoint);
					bDefaultKnown = !DefaultEndpointID.IsEmpty();
					DefaultEndpoint->Release();
				}

				IMMDeviceCollection* DeviceCollection = nullptr;
				if (FAILED(DeviceEnumerator->EnumAudioEndpoints(DataFlow, DEVICE_STATE_ACTIVE, &DeviceCollection)) || !DeviceCollection)
				{
					return false;
				}

				UINT DeviceCount = 0;
				bool bComplete = SUCCEEDED(DeviceCollection->GetCount(&DeviceCount));
				for (UINT DeviceIndex = 0; DeviceIndex < DeviceCount; ++DeviceIndex)
				{
					IMMDevice* Device = nullptr;
					if (FAILED(DeviceCollection->Item(DeviceIndex, &Device)) || !Device)
					{
						bComplete = false;
						continue;
					}

					const FString DeviceID = GetWindowsDeviceId(Device);
					if (DeviceID.IsEmpty())
					{
						bComplete = false;
					}
					else
					{
						FEFAudioDeviceInfo& DeviceInfo = Snapshot.Devices.AddDefaulted_GetRef();
						DeviceInfo.DeviceName = GetWindowsDeviceName(Device, DeviceID);
						DeviceInfo.DeviceID = DeviceID;
						DeviceInfo.bIsDefaultDevice = DeviceID == DefaultEndpointID;
						DeviceInfo.bIsInputDevice = DeviceType == EEFAudioDeviceType::Input;
						DeviceInfo.bIsConnected = true;
						DeviceInfo.bCanBeSelected = true;
						DeviceInfo.DeviceType = DeviceType;
						DeviceInfo.Backend = EEFAudioDeviceBackend::WindowsCoreAudio;
						DeviceInfo.BackendName = TEXT("Windows CoreAudio");
					}
					Device->Release();
				}

				DeviceCollection->Release();
				return bComplete && bDefaultKnown;
			};

			Snapshot.bOutputComplete = EnumerateEndpoints(eRender, EEFAudioDeviceType::Output);
			Snapshot.bInputComplete = EnumerateEndpoints(eCapture, EEFAudioDeviceType::Input);
			DeviceEnumerator->Release();
		}

		if (bShouldUninitializeCOM)
		{
			CoUninitialize();
		}
		return Snapshot;
	}
#endif
}

#if PLATFORM_WINDOWS
bool FEFAudioWindowsMeterCache::TryReadPeak(const FEFAudioDeviceInfo& DeviceInfo,
	EEFAudioDeviceType DeviceType, float& OutPeak)
{
	check(IsInGameThread());
	OutPeak = 0.f;
	for (int32 Attempt = 0; Attempt < 2; ++Attempt)
	{
		// Only initial backend failure uses the platform/voice fallback; a failed endpoint query stays zero.
		if (!EnsureEnumerator()) return Attempt > 0;
		// The default is resolved live, rather than keeping yesterday's endpoint until the next monitor poll.
		const FString EndpointID = ResolveWindowsEndpointId(Enumerator.Get(), DeviceInfo, DeviceType);
		FEndpoint* Endpoint = EndpointID.IsEmpty() ? nullptr : Endpoints.Find(EndpointID);
		if (!Endpoint && !EndpointID.IsEmpty())
		{
			FEndpoint Created;
			if (SUCCEEDED(Enumerator->GetDevice(*EndpointID, &Created.Device)) && Created.Device.IsValid()
				&& SUCCEEDED(Created.Device->Activate(__uuidof(IAudioMeterInformation), CLSCTX_ALL, nullptr,
					reinterpret_cast<void**>(&Created.Meter))) && Created.Meter.IsValid())
			{
				Endpoint = &Endpoints.Add(EndpointID, MoveTemp(Created));
			}
		}
		if (Endpoint)
		{
			// Do not keep a pointer into the map across COM calls that could pump a device notification.
			const FEndpoint ActiveEndpoint = *Endpoint;
			DWORD State = 0;
			float Peak = 0.f;
			if (SUCCEEDED(ActiveEndpoint.Device->GetState(&State)) && (State & DEVICE_STATE_ACTIVE)
				&& SUCCEEDED(ActiveEndpoint.Meter->GetPeakValue(&Peak)))
			{
				OutPeak = Peak;
				return true;
			}
		}
		// Device loss or a disconnected COM server invalidates interfaces, not the last measured level.
		// One fresh activation retries this query; persistent failure keeps the old zero-level result.
		Endpoints.Empty();
		Enumerator = nullptr;
	}
	return true;
}
#endif

void UEFAudioDeviceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	bIsInitialized = true;
	LastError.Reset();
	ActiveOutputDeviceID = DefaultDeviceId;
	ActiveInputDeviceID = DefaultDeviceId;

	AcquireVoiceChatUser();
	RefreshDeviceCache(false);
	RestartInputLevelCapture();
	ScheduleInputLevelCaptureRetry();
	StartDeviceMonitoring();
}

void UEFAudioDeviceSubsystem::Deinitialize()
{
	bIsInitialized = false;
#if PLATFORM_WINDOWS
	++WindowsDeviceSnapshotGeneration;
	WindowsMeterCache.Reset();
#endif
	StopDeviceMonitoring();
	StopInputLevelCaptureRetry();
	StopOutputAnalysis();
	StopInputLevelCapture();
	ReleaseVoiceChatUser();
	OutputDevices.Empty();
	InputDevices.Empty();
	PreferredDeviceIdAliases.Empty();
	ActiveOutputDeviceID = DefaultDeviceId;
	ActiveInputDeviceID = DefaultDeviceId;
	LastError.Reset();

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

float UEFAudioDeviceSubsystem::GetCurrentInputLevel()
{
	return FMath::Clamp(GetCurrentInputLevelRaw(), 0.0f, 1.0f);
}

float UEFAudioDeviceSubsystem::GetCurrentOutputLevel()
{
	return FMath::Clamp(GetCurrentOutputLevelRaw(), 0.0f, 1.0f);
}

float UEFAudioDeviceSubsystem::GetCurrentInputLevelRaw()
{
	return GetDeviceLevelRaw(EEFAudioDeviceType::Input, ActiveInputDeviceID);
}

float UEFAudioDeviceSubsystem::GetCurrentOutputLevelRaw()
{
	return GetDeviceLevelRaw(EEFAudioDeviceType::Output, ActiveOutputDeviceID);
}

float UEFAudioDeviceSubsystem::GetDeviceLevel(EEFAudioDeviceType DeviceType, const FString& DeviceID)
{
	return FMath::Clamp(GetDeviceLevelRaw(DeviceType, DeviceID), 0.0f, 1.0f);
}

float UEFAudioDeviceSubsystem::GetDeviceLevelRaw(EEFAudioDeviceType DeviceType, const FString& DeviceID)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(EFAudioDevices_QueryLevel);
	const FString PreferredDeviceID = ResolvePreferredDeviceID(DeviceType, DeviceID);
	const FEFAudioDeviceInfo* DeviceInfo = FindDevice(PreferredDeviceID, DeviceType);
	if (!DeviceInfo)
	{
		return 0.0f;
	}

	if (DeviceType == EEFAudioDeviceType::Input && PreferredDeviceID == ActiveInputDeviceID && !IsInputLevelCaptureUsable())
	{
		ScheduleInputLevelCaptureRetry();
	}

#if PLATFORM_WINDOWS
	check(IsInGameThread());
	if (!WindowsMeterCache.IsValid()) WindowsMeterCache = MakeShared<FEFAudioWindowsMeterCache>();
	// Hold the interfaces through this call even if a future reentrant device event resets the owner cache.
	const TSharedPtr<FEFAudioWindowsMeterCache> MeterCache = WindowsMeterCache;
	const FEFAudioDeviceInfo MeterDevice = *DeviceInfo;
	float PeakLevel = 0.f;
	if (MeterCache->TryReadPeak(MeterDevice, DeviceType, PeakLevel))
	{
		return PeakLevel >= 0.0f ? PeakLevel : 0.0f;
	}
#endif

	if (DeviceType == EEFAudioDeviceType::Output)
	{
		EnsureOutputAnalysisStarted();

		UWorld* World = ResolveWorld();
		if (!bIsOutputAnalysisActive || !World || OutputAnalysisFrequencies.Num() == 0)
		{
			return 0.0f;
		}

		TArray<float> Magnitudes;
		UAudioMixerBlueprintLibrary::GetMagnitudeForFrequencies(World, OutputAnalysisFrequencies, Magnitudes, nullptr);
		if (Magnitudes.Num() == 0)
		{
			return 0.0f;
		}

		float SumMagnitude = 0.0f;
		float PeakMagnitude = 0.0f;
		for (const float Magnitude : Magnitudes)
		{
			SumMagnitude += Magnitude;
			PeakMagnitude = FMath::Max(PeakMagnitude, Magnitude);
		}

		const float AverageMagnitude = SumMagnitude / static_cast<float>(Magnitudes.Num());
		const float CombinedMagnitude = FMath::Max(PeakMagnitude, AverageMagnitude * 1.5f);
		return CombinedMagnitude * OutputLevelBoost;
	}

	if (!InputLevelCapture.IsValid())
	{
		RestartInputLevelCapture();
	}

	if (!InputLevelCapture.IsValid())
	{
		return 0.0f;
	}

	uint32 AvailableVoiceData = 0;
	const EVoiceCaptureState::Type VoiceState = InputLevelCapture->GetCaptureState(AvailableVoiceData);
	if (VoiceState == EVoiceCaptureState::NotCapturing)
	{
		InputLevelCapture->Start();
		return 0.0f;
	}

	if (VoiceState != EVoiceCaptureState::Ok || AvailableVoiceData == 0)
	{
		return 0.0f;
	}

	TArray<uint8> VoiceBuffer;
	VoiceBuffer.SetNumUninitialized(AvailableVoiceData);

	uint32 ReadBytes = 0;
	uint64 SampleCount = 0;
	const EVoiceCaptureState::Type ReadState = InputLevelCapture->GetVoiceData(VoiceBuffer.GetData(), AvailableVoiceData, ReadBytes, SampleCount);
	if (ReadState != EVoiceCaptureState::Ok || ReadBytes == 0)
	{
		return 0.0f;
	}

	const int16* Samples = reinterpret_cast<const int16*>(VoiceBuffer.GetData());
	const int32 NumSamples = ReadBytes / sizeof(int16);
	if (NumSamples <= 0)
	{
		return 0.0f;
	}

	float SumSquares = 0.0f;
	for (int32 Index = 0; Index < NumSamples; ++Index)
	{
		const float Sample = static_cast<float>(Samples[Index]) / 32768.0f;
		SumSquares += Sample * Sample;
	}

	const float Rms = FMath::Sqrt(SumSquares / static_cast<float>(NumSamples));
	return Rms * InputLevelBoost;
}

FString UEFAudioDeviceSubsystem::ResolvePreferredDeviceID(EEFAudioDeviceType DeviceType, const FString& DeviceID) const
{
	const FString NormalizedDeviceID = NormalizeDeviceId(DeviceID);
	if (const FString* PreferredAlias = PreferredDeviceIdAliases.Find(MakeDeviceAliasKey(DeviceType, NormalizedDeviceID)))
	{
		return *PreferredAlias;
	}

	const FEFAudioDeviceInfo* DeviceInfo = FindDevice(NormalizedDeviceID, DeviceType);
	if (!DeviceInfo)
	{
		const TArray<FEFAudioDeviceInfo>& Devices = DeviceType == EEFAudioDeviceType::Input ? InputDevices : OutputDevices;
		if (const FEFAudioDeviceInfo* WindowsDevice = Devices.FindByPredicate([&NormalizedDeviceID](const FEFAudioDeviceInfo& Candidate)
		{
			return Candidate.Backend == EEFAudioDeviceBackend::WindowsCoreAudio
				&& AreLikelySameEndpointName(Candidate.DeviceName, NormalizedDeviceID);
		}))
		{
			return WindowsDevice->DeviceID;
		}

		return NormalizedDeviceID;
	}

#if PLATFORM_WINDOWS
	if (DeviceInfo->Backend != EEFAudioDeviceBackend::WindowsCoreAudio)
	{
		const TArray<FEFAudioDeviceInfo>& Devices = DeviceType == EEFAudioDeviceType::Input ? InputDevices : OutputDevices;
		if (const FEFAudioDeviceInfo* WindowsDevice = Devices.FindByPredicate([DeviceInfo](const FEFAudioDeviceInfo& Candidate)
		{
			return Candidate.Backend == EEFAudioDeviceBackend::WindowsCoreAudio
				&& AreLikelySameEndpointName(Candidate.DeviceName, DeviceInfo->DeviceName);
		}))
		{
			return WindowsDevice->DeviceID;
		}
	}
#endif

	return DeviceInfo->DeviceID;
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
#if PLATFORM_WINDOWS
	// Initial/manual refresh remains immediate. An older monitoring result must not replace it.
	++WindowsDeviceSnapshotGeneration;
	WindowsMeterCache.Reset();
#endif
	const TArray<FEFAudioDeviceInfo> PreviousDevices = BuildAllDevices();

	OutputDevices.Empty();
	InputDevices.Empty();
	PreferredDeviceIdAliases.Empty();

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
#if PLATFORM_WINDOWS
	if (!bIsInitialized || bWindowsDeviceSnapshotInFlight) return;
	bWindowsDeviceSnapshotInFlight = true;
	const uint64 Generation = WindowsDeviceSnapshotGeneration;
	const TWeakObjectPtr<UEFAudioDeviceSubsystem> WeakThis(this);
	Async(EAsyncExecution::ThreadPool, [WeakThis, Generation]()
	{
		FWindowsAudioDeviceSnapshot Snapshot = ReadWindowsAudioDevices();
		AsyncTask(ENamedThreads::GameThread, [WeakThis, Generation, Snapshot = MoveTemp(Snapshot)]() mutable
		{
			if (UEFAudioDeviceSubsystem* Self = WeakThis.Get())
			{
				Self->bWindowsDeviceSnapshotInFlight = false;
				if (Self->bIsInitialized && Self->WindowsDeviceSnapshotGeneration == Generation)
				{
					if (!Snapshot.bOutputComplete || !Snapshot.bInputComplete)
					{
						Snapshot.RetainIncompleteFlows(Self->BuildAllDevices());
					}
					Self->ApplyMonitoredDeviceSnapshot(Snapshot.Devices);
				}
			}
		});
	});
#else
	ApplyMonitoredDeviceSnapshot({});
#endif
}

void UEFAudioDeviceSubsystem::ApplyMonitoredDeviceSnapshot(const TArray<FEFAudioDeviceInfo>& PlatformDevices)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(EFAudioDevices_ApplyMonitoredSnapshot);
	const TArray<FEFAudioDeviceInfo> PreviousDevices = BuildAllDevices();

	OutputDevices.Empty();
	InputDevices.Empty();
	AcquireVoiceChatUser();
	EnumerateVoiceChatDevices();
#if PLATFORM_WINDOWS
	for (const FEFAudioDeviceInfo& DeviceInfo : PlatformDevices) AddOrUpdateDevice(DeviceInfo);
#else
	EnumeratePlatformDevices();
#endif
	AddPlatformDefaultFallbacks();

	const TArray<FEFAudioDeviceInfo> CurrentDevices = BuildAllDevices();
	if (!AreDeviceListsEquivalent(PreviousDevices, CurrentDevices))
	{
#if PLATFORM_WINDOWS
		WindowsMeterCache.Reset();
#endif
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

void UEFAudioDeviceSubsystem::RestartInputLevelCapture()
{
	StopInputLevelCapture();

	if (!FVoiceModule::IsAvailable())
	{
		return;
	}

	const FEFAudioDeviceInfo ActiveInputDevice = GetActiveInputDevice();
	const FString CaptureDeviceName = ActiveInputDevice.bIsDefaultDevice || IsDefaultDeviceId(ActiveInputDevice.DeviceID)
		? FString()
		: ActiveInputDevice.DeviceName;

	InputLevelCapture = FVoiceModule::Get().CreateVoiceCapture(CaptureDeviceName);
	if (InputLevelCapture.IsValid())
	{
		InputLevelCapture->Start();
	}
}

void UEFAudioDeviceSubsystem::StopInputLevelCapture()
{
	if (InputLevelCapture.IsValid())
	{
		InputLevelCapture->Stop();
		InputLevelCapture.Reset();
	}
}

void UEFAudioDeviceSubsystem::ScheduleInputLevelCaptureRetry()
{
	UWorld* World = ResolveWorld();
	if (!World)
	{
		return;
	}

	FTimerManager& TimerManager = World->GetTimerManager();
	if (TimerManager.IsTimerActive(InputLevelCaptureRetryTimerHandle)
		&& PendingInputLevelCaptureDeviceID == ActiveInputDeviceID)
	{
		return;
	}

	PendingInputLevelCaptureDeviceID = ActiveInputDeviceID;
	InputLevelCaptureRetryAttempts = 0;

	TimerManager.ClearTimer(InputLevelCaptureRetryTimerHandle);
	TimerManager.SetTimer(
		InputLevelCaptureRetryTimerHandle,
		this,
		&UEFAudioDeviceSubsystem::HandleInputLevelCaptureRetry,
		InputLevelCaptureRetryIntervalSeconds,
		true);
}

void UEFAudioDeviceSubsystem::StopInputLevelCaptureRetry()
{
	if (UWorld* World = ResolveWorld())
	{
		World->GetTimerManager().ClearTimer(InputLevelCaptureRetryTimerHandle);
	}

	PendingInputLevelCaptureDeviceID.Reset();
	InputLevelCaptureRetryAttempts = 0;
}

void UEFAudioDeviceSubsystem::HandleInputLevelCaptureRetry()
{
	if (PendingInputLevelCaptureDeviceID != ActiveInputDeviceID
		|| (InputLevelCaptureRetryAttempts > 0 && IsInputLevelCaptureUsable()))
	{
		StopInputLevelCaptureRetry();
		return;
	}

	if (InputLevelCaptureRetryAttempts >= InputLevelCaptureMaxRetryAttempts)
	{
		StopInputLevelCaptureRetry();
		return;
	}

	++InputLevelCaptureRetryAttempts;
	RestartInputLevelCapture();
}

bool UEFAudioDeviceSubsystem::IsInputLevelCaptureUsable() const
{
	if (!InputLevelCapture.IsValid())
	{
		return false;
	}

	uint32 AvailableVoiceData = 0;
	const EVoiceCaptureState::Type VoiceState = InputLevelCapture->GetCaptureState(AvailableVoiceData);
	return VoiceState == EVoiceCaptureState::Ok || VoiceState == EVoiceCaptureState::NoData;
}

void UEFAudioDeviceSubsystem::EnsureOutputAnalysisStarted()
{
	if (bIsOutputAnalysisActive)
	{
		return;
	}

	UWorld* World = ResolveWorld();
	if (!World)
	{
		return;
	}

	if (OutputAnalysisFrequencies.Num() == 0)
	{
		OutputAnalysisFrequencies = { 125.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f };
	}

	UAudioMixerBlueprintLibrary::StartAnalyzingOutput(World, nullptr, EFFTSize::Medium, EFFTPeakInterpolationMethod::Linear, EFFTWindowType::Hann, 0.0f, EAudioSpectrumType::MagnitudeSpectrum);
	bIsOutputAnalysisActive = true;
}

void UEFAudioDeviceSubsystem::StopOutputAnalysis()
{
	if (!bIsOutputAnalysisActive)
	{
		return;
	}

	if (UWorld* World = ResolveWorld())
	{
		UAudioMixerBlueprintLibrary::StopAnalyzingOutput(World, nullptr);
	}

	bIsOutputAnalysisActive = false;
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
	const FString NormalizedDeviceID = ResolvePreferredDeviceID(DeviceType, DeviceID);
	const FEFAudioDeviceInfo* DeviceInfo = FindDevice(NormalizedDeviceID, DeviceType);

	if (!DeviceInfo)
	{
		SetLastError(FString::Printf(TEXT("Audio device '%s' was not found on %s."), *NormalizedDeviceID, *GetPlatformName()));
		return EEFAudioDeviceSelectionResult::InvalidDevice;
	}

#if PLATFORM_WINDOWS
	// The mixer opens on the system default. Reapplying that selection during startup would restart its live stream.
	if (DeviceType == EEFAudioDeviceType::Output && IsDefaultDeviceId(NormalizedDeviceID) && IsDefaultDeviceId(ActiveOutputDeviceID))
		return EEFAudioDeviceSelectionResult::Success;

	WindowsMeterCache.Reset();
	if (DeviceType == EEFAudioDeviceType::Input
		&& (DeviceInfo->Backend == EEFAudioDeviceBackend::WindowsCoreAudio || DeviceInfo->Backend == EEFAudioDeviceBackend::PlatformDefault))
	{
		ActiveInputDeviceID = NormalizedDeviceID;
		RestartInputLevelCapture();
		ScheduleInputLevelCaptureRetry();
		OnActiveAudioDeviceChanged.Broadcast(DeviceType, *DeviceInfo);
		StartDeviceMonitoring();
		return EEFAudioDeviceSelectionResult::Success;
	}

	if (DeviceType == EEFAudioDeviceType::Output
		&& (DeviceInfo->Backend == EEFAudioDeviceBackend::WindowsCoreAudio || DeviceInfo->Backend == EEFAudioDeviceBackend::PlatformDefault))
	{
		UWorld* World = ResolveWorld();
		if (!World)
		{
			SetLastError(TEXT("No valid world was available to swap the active audio output device."));
			return EEFAudioDeviceSelectionResult::BackendUnavailable;
		}

		FOnCompletedDeviceSwap OnCompletedDeviceSwap;
		OnCompletedDeviceSwap.BindUFunction(this, GET_FUNCTION_NAME_CHECKED(UEFAudioDeviceSubsystem, HandleCompletedOutputDeviceSwap));
		const FString MixerDeviceID = IsDefaultDeviceId(NormalizedDeviceID) ? FString() : NormalizedDeviceID;
		UAudioMixerBlueprintLibrary::SwapAudioOutputDevice(World, MixerDeviceID, OnCompletedDeviceSwap);

		ActiveOutputDeviceID = NormalizedDeviceID;
		OnActiveAudioDeviceChanged.Broadcast(DeviceType, *DeviceInfo);
		StartDeviceMonitoring();
		return EEFAudioDeviceSelectionResult::Success;
	}
#endif

	if (DeviceInfo->Backend == EEFAudioDeviceBackend::PlatformDefault && IsDefaultDeviceId(NormalizedDeviceID))
	{
		if (DeviceType == EEFAudioDeviceType::Input)
		{
			ActiveInputDeviceID = NormalizedDeviceID;
			RestartInputLevelCapture();
			ScheduleInputLevelCaptureRetry();
		}
		else
		{
			ActiveOutputDeviceID = NormalizedDeviceID;
			EnsureOutputAnalysisStarted();
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
		RestartInputLevelCapture();
		ScheduleInputLevelCaptureRetry();
	}
	else
	{
		ActiveOutputDeviceID = NormalizedDeviceID;
		EnsureOutputAnalysisStarted();
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

void UEFAudioDeviceSubsystem::HandleCompletedOutputDeviceSwap(const FSwapAudioOutputResult& SwapResult)
{
	if (SwapResult.Result == ESwapAudioOutputDeviceResultState::Success)
	{
#if PLATFORM_WINDOWS
		WindowsMeterCache.Reset();
#endif
		ActiveOutputDeviceID = SwapResult.RequestedDeviceId.IsEmpty() ? DefaultDeviceId : NormalizeDeviceId(SwapResult.RequestedDeviceId);
		if (const FEFAudioDeviceInfo* DeviceInfo = FindDevice(ActiveOutputDeviceID, EEFAudioDeviceType::Output))
		{
			OnActiveAudioDeviceChanged.Broadcast(EEFAudioDeviceType::Output, *DeviceInfo);
		}
		return;
	}

	ActiveOutputDeviceID = SwapResult.CurrentDeviceId.IsEmpty() ? DefaultDeviceId : NormalizeDeviceId(SwapResult.CurrentDeviceId);
	SetLastError(FString::Printf(TEXT("Failed to swap the active audio output device to '%s'."), *SwapResult.RequestedDeviceId));
}

void UEFAudioDeviceSubsystem::AddOrUpdateDevice(const FEFAudioDeviceInfo& DeviceInfo)
{
	TArray<FEFAudioDeviceInfo>& Devices = DeviceInfo.DeviceType == EEFAudioDeviceType::Input ? InputDevices : OutputDevices;

	for (FEFAudioDeviceInfo& ExistingDevice : Devices)
	{
		if (ExistingDevice.DeviceID == DeviceInfo.DeviceID)
		{
			if (ExistingDevice.Backend != EEFAudioDeviceBackend::WindowsCoreAudio
				&& DeviceInfo.Backend == EEFAudioDeviceBackend::WindowsCoreAudio)
			{
				PreferredDeviceIdAliases.Add(MakeDeviceAliasKey(DeviceInfo.DeviceType, ExistingDevice.DeviceID), DeviceInfo.DeviceID);
				ExistingDevice = DeviceInfo;
			}
			else if (!ExistingDevice.bCanBeSelected && DeviceInfo.bCanBeSelected)
			{
				ExistingDevice = DeviceInfo;
			}
			return;
		}

		if (ExistingDevice.DeviceType == DeviceInfo.DeviceType
			&& AreLikelySameEndpointName(ExistingDevice.DeviceName, DeviceInfo.DeviceName)
			&& (ExistingDevice.Backend == EEFAudioDeviceBackend::WindowsCoreAudio || DeviceInfo.Backend == EEFAudioDeviceBackend::WindowsCoreAudio))
		{
			if (DeviceInfo.Backend == EEFAudioDeviceBackend::WindowsCoreAudio)
			{
				PreferredDeviceIdAliases.Add(MakeDeviceAliasKey(DeviceInfo.DeviceType, ExistingDevice.DeviceID), DeviceInfo.DeviceID);
				ExistingDevice = DeviceInfo;
			}
			else if (ExistingDevice.Backend == EEFAudioDeviceBackend::WindowsCoreAudio)
			{
				PreferredDeviceIdAliases.Add(MakeDeviceAliasKey(DeviceInfo.DeviceType, DeviceInfo.DeviceID), ExistingDevice.DeviceID);
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
	const FWindowsAudioDeviceSnapshot Snapshot = ReadWindowsAudioDevices();
	for (const FEFAudioDeviceInfo& DeviceInfo : Snapshot.Devices) AddOrUpdateDevice(DeviceInfo);
}
#endif

#if PLATFORM_WINDOWS && WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEFAudioPartialSnapshotTest, "UnrealExtendedFramework.AudioDevices.PartialSnapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEFAudioPartialSnapshotTest::RunTest(const FString&)
{
	FEFAudioDeviceInfo Microphone;
	Microphone.DeviceType = EEFAudioDeviceType::Input;
	Microphone.DeviceID = TEXT("microphone");
	Microphone.Backend = EEFAudioDeviceBackend::WindowsCoreAudio;
	FEFAudioDeviceInfo Speaker = Microphone;
	Speaker.DeviceType = EEFAudioDeviceType::Output;
	Speaker.DeviceID = TEXT("speaker");
	FEFAudioDeviceInfo Voice = Microphone;
	Voice.Backend = EEFAudioDeviceBackend::UnrealVoiceChat;
	const TArray<FEFAudioDeviceInfo> Previous{ Microphone, Speaker, Voice };

	FWindowsAudioDeviceSnapshot Snapshot;
	Snapshot.bOutputComplete = true; // Last speaker unplugged while one microphone query failed.
	Snapshot.Devices.Emplace_GetRef(Microphone).DeviceID = TEXT("partial input");
	Snapshot.RetainIncompleteFlows(Previous);
	if (!TestEqual(TEXT("A confirmed empty output list and a partial input leave one endpoint"), Snapshot.Devices.Num(), 1)) return false;
	TestEqual(TEXT("The previous Windows microphone is preserved"), Snapshot.Devices[0].DeviceID, Microphone.DeviceID);

	Snapshot.Devices.Reset();
	Snapshot.bOutputComplete = false;
	Snapshot.bInputComplete = true;
	FEFAudioDeviceInfo NewMicrophone = Microphone;
	NewMicrophone.DeviceID = TEXT("new microphone");
	Snapshot.Devices.Add(NewMicrophone);
	Snapshot.RetainIncompleteFlows(Previous);
	if (!TestEqual(TEXT("A failed speaker query still allows microphone changes"), Snapshot.Devices.Num(), 2)) return false;
	TestEqual(TEXT("Fresh input survives"), Snapshot.Devices[0].DeviceID, NewMicrophone.DeviceID);
	TestEqual(TEXT("Previous output survives"), Snapshot.Devices[1].DeviceID, Speaker.DeviceID);
	return !HasAnyErrors();
}
#endif
