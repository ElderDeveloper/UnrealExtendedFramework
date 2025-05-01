// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "EFAudioDevice.generated.h"

USTRUCT(BlueprintType)
struct FEFAudioDeviceInfo
{
    GENERATED_BODY()
    
    UPROPERTY(BlueprintReadOnly, Category = "Audio")
    FString DeviceName = TEXT("Unknown");
    
    UPROPERTY(BlueprintReadOnly, Category = "Audio")
    FString DeviceID = TEXT("Unknown");
    
    UPROPERTY(BlueprintReadOnly, Category = "Audio")
    bool bIsDefaultDevice = false;
    
    UPROPERTY(BlueprintReadOnly, Category = "Audio")
    bool bIsInputDevice = false;
    
    UPROPERTY(BlueprintReadOnly, Category = "Audio")
    bool bIsConnected = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAudioDevicesChanged, const TArray<FEFAudioDeviceInfo>&, AudioDevices);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDeviceDisconnected, const FEFAudioDeviceInfo&, DisconnectedDevice);

UCLASS(BlueprintType, Blueprintable)
class UNREALEXTENDEDFRAMEWORK_API UEFAudioDeviceManager : public UObject
{
    GENERATED_BODY()
    
public:
    UEFAudioDeviceManager();
    
    // Initialize the audio device manager
    UFUNCTION(BlueprintCallable, Category = "Audio|Devices")
    void Initialize();
    
    // Get all available audio output devices
    UFUNCTION(BlueprintCallable, Category = "Audio|Devices")
    TArray<FEFAudioDeviceInfo> GetOutputDevices() const;
    
    // Get all available audio input devices
    UFUNCTION(BlueprintCallable, Category = "Audio|Devices")
    TArray<FEFAudioDeviceInfo> GetInputDevices() const;
    
    // Get the current default output device
    UFUNCTION(BlueprintCallable, Category = "Audio|Devices")
    FEFAudioDeviceInfo GetDefaultOutputDevice() const;
    
    // Get the current default input device
    UFUNCTION(BlueprintCallable, Category = "Audio|Devices")
    FEFAudioDeviceInfo GetDefaultInputDevice() const;
    
    // Set the current output device
    UFUNCTION(BlueprintCallable, Category = "Audio|Devices")
    bool SetOutputDevice(const FString& DeviceID);
    
    // Set the current input device
    UFUNCTION(BlueprintCallable, Category = "Audio|Devices")
    bool SetInputDevice(const FString& DeviceID);
    
    // Refresh the list of audio devices
    UFUNCTION(BlueprintCallable, Category = "Audio|Devices")
    void RefreshDeviceList();
    
    // Events
    UPROPERTY(BlueprintAssignable, Category = "Audio|Devices|Events")
    FOnAudioDevicesChanged OnAudioDevicesChanged;
    
    UPROPERTY(BlueprintAssignable, Category = "Audio|Devices|Events")
    FOnDeviceDisconnected OnDeviceDisconnected;
    
private:
    void PlatformEnumerateAudioDevices();
    bool PlatformSetOutputDevice(const FString& DeviceID);
    bool PlatformSetInputDevice(const FString& DeviceID);
    void StartDeviceMonitoring();
    void CheckDeviceConnectivity();
    
    TArray<FEFAudioDeviceInfo> OutputDevices;
    TArray<FEFAudioDeviceInfo> InputDevices;
    FEFAudioDeviceInfo DefaultOutputDevice;
    FEFAudioDeviceInfo DefaultInputDevice;
    
    FTimerHandle DeviceMonitorTimerHandle;
    bool bIsInitialized;
};
