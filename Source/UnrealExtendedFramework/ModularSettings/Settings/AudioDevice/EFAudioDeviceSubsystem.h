// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "EFAudioDeviceSubsystem.generated.h"

class IVoiceChatUser;

UENUM(BlueprintType)
enum class EEFAudioDeviceType : uint8
{
	Input,
	Output
};

UENUM(BlueprintType)
enum class EEFAudioDeviceBackend : uint8
{
	Unknown,
	PlatformDefault,
	UnrealVoiceChat,
	WindowsCoreAudio
};

UENUM(BlueprintType)
enum class EEFAudioDeviceSelectionResult : uint8
{
	Success,
	InvalidDevice,
	Unsupported,
	BackendUnavailable
};

USTRUCT(BlueprintType)
struct FEFAudioDeviceInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Audio")
	FString DeviceName = TEXT("Default");

	UPROPERTY(BlueprintReadOnly, Category = "Audio")
	FString DeviceID = TEXT("Default");

	UPROPERTY(BlueprintReadOnly, Category = "Audio")
	bool bIsDefaultDevice = false;

	UPROPERTY(BlueprintReadOnly, Category = "Audio")
	bool bIsInputDevice = false;

	UPROPERTY(BlueprintReadOnly, Category = "Audio")
	bool bIsConnected = false;

	UPROPERTY(BlueprintReadOnly, Category = "Audio")
	bool bCanBeSelected = false;

	UPROPERTY(BlueprintReadOnly, Category = "Audio")
	EEFAudioDeviceType DeviceType = EEFAudioDeviceType::Output;

	UPROPERTY(BlueprintReadOnly, Category = "Audio")
	EEFAudioDeviceBackend Backend = EEFAudioDeviceBackend::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Audio")
	FString BackendName = TEXT("Unknown");
};

USTRUCT(BlueprintType)
struct FEFAudioDeviceCapabilities
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Audio")
	FString PlatformName = TEXT("Unknown");

	UPROPERTY(BlueprintReadOnly, Category = "Audio")
	bool bCanEnumerateInputDevices = false;

	UPROPERTY(BlueprintReadOnly, Category = "Audio")
	bool bCanEnumerateOutputDevices = false;

	UPROPERTY(BlueprintReadOnly, Category = "Audio")
	bool bCanSelectInputDevice = false;

	UPROPERTY(BlueprintReadOnly, Category = "Audio")
	bool bCanSelectOutputDevice = false;

	UPROPERTY(BlueprintReadOnly, Category = "Audio")
	bool bUsesUnrealVoiceChat = false;

	UPROPERTY(BlueprintReadOnly, Category = "Audio")
	bool bUsesNativePlatformEnumeration = false;

	UPROPERTY(BlueprintReadOnly, Category = "Audio")
	FString Notes;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAudioDevicesChanged, const TArray<FEFAudioDeviceInfo>&, AudioDevices);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDeviceDisconnected, const FEFAudioDeviceInfo&, DisconnectedDevice);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnActiveAudioDeviceChanged, EEFAudioDeviceType, DeviceType, const FEFAudioDeviceInfo&, DeviceInfo);

UCLASS(BlueprintType, Blueprintable)
class UNREALEXTENDEDFRAMEWORK_API UEFAudioDeviceSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category = "Audio", meta = (WorldContext = "WorldContextObject"))
	static UEFAudioDeviceSubsystem* GetAudioDeviceSubsystem(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Audio|Devices")
	void RestartDeviceMonitoring();

	UFUNCTION(BlueprintCallable, Category = "Audio|Devices")
	void RefreshDeviceList();

	UFUNCTION(BlueprintPure, Category = "Audio|Devices")
	TArray<FEFAudioDeviceInfo> GetAllDevices() const;

	UFUNCTION(BlueprintPure, Category = "Audio|Devices")
	TArray<FEFAudioDeviceInfo> GetOutputDevices() const;

	UFUNCTION(BlueprintPure, Category = "Audio|Devices")
	TArray<FEFAudioDeviceInfo> GetInputDevices() const;

	UFUNCTION(BlueprintPure, Category = "Audio|Devices")
	FEFAudioDeviceInfo GetDefaultOutputDevice() const;

	UFUNCTION(BlueprintPure, Category = "Audio|Devices")
	FEFAudioDeviceInfo GetDefaultInputDevice() const;

	UFUNCTION(BlueprintPure, Category = "Audio|Devices")
	FEFAudioDeviceInfo GetActiveOutputDevice() const;

	UFUNCTION(BlueprintPure, Category = "Audio|Devices")
	FEFAudioDeviceInfo GetActiveInputDevice() const;

	UFUNCTION(BlueprintCallable, Category = "Audio|Devices")
	bool SetOutputDevice(const FString& DeviceID);

	UFUNCTION(BlueprintCallable, Category = "Audio|Devices")
	bool SetInputDevice(const FString& DeviceID);

	UFUNCTION(BlueprintCallable, Category = "Audio|Devices")
	EEFAudioDeviceSelectionResult SelectOutputDevice(const FString& DeviceID);

	UFUNCTION(BlueprintCallable, Category = "Audio|Devices")
	EEFAudioDeviceSelectionResult SelectInputDevice(const FString& DeviceID);

	UFUNCTION(BlueprintPure, Category = "Audio|Devices")
	FEFAudioDeviceCapabilities GetCapabilities() const;

	UFUNCTION(BlueprintPure, Category = "Audio|Devices")
	FString GetLastError() const;

	UFUNCTION(BlueprintPure, Category = "Audio|Devices")
	bool IsInputDeviceSelectionSupported() const;

	UFUNCTION(BlueprintPure, Category = "Audio|Devices")
	bool IsOutputDeviceSelectionSupported() const;

	UPROPERTY(BlueprintAssignable, Category = "Audio|Devices|Events")
	FOnAudioDevicesChanged OnAudioDevicesChanged;

	UPROPERTY(BlueprintAssignable, Category = "Audio|Devices|Events")
	FOnDeviceDisconnected OnDeviceDisconnected;

	UPROPERTY(BlueprintAssignable, Category = "Audio|Devices|Events")
	FOnActiveAudioDeviceChanged OnActiveAudioDeviceChanged;

private:
	UWorld* ResolveWorld() const;
	void RefreshDeviceCache(bool bBroadcastChanges);
	void StartDeviceMonitoring();
	void StopDeviceMonitoring();
	void CheckDeviceConnectivity();

	void AcquireVoiceChatUser();
	void ReleaseVoiceChatUser();
	void EnumerateVoiceChatDevices();
	void EnumeratePlatformDevices();
	void AddPlatformDefaultFallbacks();

	EEFAudioDeviceSelectionResult SelectDevice(const FString& DeviceID, EEFAudioDeviceType DeviceType);
	bool TrySelectVoiceChatDevice(const FString& DeviceID, EEFAudioDeviceType DeviceType);

	void AddOrUpdateDevice(const FEFAudioDeviceInfo& DeviceInfo);
	const FEFAudioDeviceInfo* FindDevice(const FString& DeviceID, EEFAudioDeviceType DeviceType) const;
	FEFAudioDeviceInfo FindDefaultDevice(EEFAudioDeviceType DeviceType) const;
	FEFAudioDeviceInfo FindActiveDevice(EEFAudioDeviceType DeviceType) const;
	TArray<FEFAudioDeviceInfo> BuildAllDevices() const;
	FEFAudioDeviceCapabilities BuildCapabilities() const;

	bool HasSelectableDevice(EEFAudioDeviceType DeviceType) const;
	bool HasRealDevice(EEFAudioDeviceType DeviceType) const;
	bool AreDeviceListsEquivalent(const TArray<FEFAudioDeviceInfo>& PreviousDevices, const TArray<FEFAudioDeviceInfo>& CurrentDevices) const;
	void BroadcastDisconnectedDevices(const TArray<FEFAudioDeviceInfo>& PreviousDevices, const TArray<FEFAudioDeviceInfo>& CurrentDevices);

	FString GetPlatformName() const;
	FString GetPlatformSupportNotes() const;
	FString GetBackendName(EEFAudioDeviceBackend Backend) const;
	FString NormalizeDeviceId(const FString& DeviceID) const;
	bool IsDefaultDeviceId(const FString& DeviceID) const;
	void SetLastError(const FString& ErrorMessage);

#if PLATFORM_WINDOWS
	void EnumerateWindowsCoreAudioDevices();
#endif

	TArray<FEFAudioDeviceInfo> OutputDevices;
	TArray<FEFAudioDeviceInfo> InputDevices;
	FString ActiveOutputDeviceID = TEXT("Default");
	FString ActiveInputDeviceID = TEXT("Default");
	FString LastError;

	FTimerHandle DeviceMonitorTimerHandle;
	IVoiceChatUser* VoiceChatUser = nullptr;
	bool bOwnsVoiceChatUser = false;
	bool bIsInitialized = false;
};