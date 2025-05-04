// Fill out your copyright notice in the Description page of Project Settings.


#include "EFAudioDevice.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <endpointvolume.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

UEFAudioDeviceManager::UEFAudioDeviceManager()
{
}

void UEFAudioDeviceManager::Initialize()
{
    RefreshDeviceList();
    StartDeviceMonitoring();
}

void UEFAudioDeviceManager::Deinitialize()
{
    // Reset all timers
    GetWorld()->GetTimerManager().ClearAllTimersForObject(this);

    ConditionalBeginDestroy();
}

void UEFAudioDeviceManager::RefreshDeviceList()
{
    OutputDevices.Empty();
    InputDevices.Empty();
    PlatformEnumerateAudioDevices();
    OnAudioDevicesChanged.Broadcast(OutputDevices);
}

#if PLATFORM_WINDOWS
void UEFAudioDeviceManager::PlatformEnumerateAudioDevices()
{
    CoInitialize(nullptr);
    IMMDeviceEnumerator* pEnumerator = nullptr;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, 
                                  __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    
    if (SUCCEEDED(hr))
    {
        // Enumerate output (render) devices
        {
            IMMDeviceCollection* pCollection = nullptr;
            hr = pEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pCollection);
            
            if (SUCCEEDED(hr))
            {
                UINT count;
                pCollection->GetCount(&count);
                
                // Get default device
                IMMDevice* pDefaultDevice = nullptr;
                pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDefaultDevice);
                
                LPWSTR defaultDeviceId = nullptr;
                if (pDefaultDevice)
                {
                    pDefaultDevice->GetId(&defaultDeviceId);
                    pDefaultDevice->Release();
                }
                
                for (UINT i = 0; i < count; i++)
                {
                    IMMDevice* pDevice = nullptr;
                    hr = pCollection->Item(i, &pDevice);
                    
                    if (SUCCEEDED(hr))
                    {
                        LPWSTR pwszID = nullptr;
                        hr = pDevice->GetId(&pwszID);
                        
                        if (SUCCEEDED(hr))
                        {
                            IPropertyStore* pProps = nullptr;
                            hr = pDevice->OpenPropertyStore(STGM_READ, &pProps);
                            
                            if (SUCCEEDED(hr))
                            {
                                PROPVARIANT varName;
                                PropVariantInit(&varName);
                                
                                hr = pProps->GetValue(PKEY_Device_FriendlyName, &varName);
                                
                                if (SUCCEEDED(hr))
                                {
                                    FEFAudioDeviceInfo DeviceInfo;
                                    DeviceInfo.DeviceName = FString(varName.pwszVal);
                                    DeviceInfo.DeviceID = FString(pwszID);
                                    DeviceInfo.bIsInputDevice = false;
                                    DeviceInfo.bIsDefaultDevice = (defaultDeviceId && wcscmp(pwszID, defaultDeviceId) == 0);
                                    DeviceInfo.bIsConnected = true;
                                    
                                    OutputDevices.Add(DeviceInfo);
                                    
                                    if (DeviceInfo.bIsDefaultDevice)
                                    {
                                        DefaultOutputDevice = DeviceInfo;
                                    }
                                    
                                    PropVariantClear(&varName);
                                }
                                
                                pProps->Release();
                            }
                            
                            CoTaskMemFree(pwszID);
                        }
                        
                        pDevice->Release();
                    }
                }
                
                if (defaultDeviceId)
                {
                    CoTaskMemFree(defaultDeviceId);
                }
                
                pCollection->Release();
            }
        }
        
        // Enumerate input (capture) devices
        {
            IMMDeviceCollection* pCollection = nullptr;
            hr = pEnumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &pCollection);
            
            if (SUCCEEDED(hr))
            {
                UINT count;
                pCollection->GetCount(&count);
                
                // Get default input device
                IMMDevice* pDefaultDevice = nullptr;
                pEnumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &pDefaultDevice);
                
                LPWSTR defaultDeviceId = nullptr;
                if (pDefaultDevice)
                {
                    pDefaultDevice->GetId(&defaultDeviceId);
                    pDefaultDevice->Release();
                }
                
                for (UINT i = 0; i < count; i++)
                {
                    IMMDevice* pDevice = nullptr;
                    hr = pCollection->Item(i, &pDevice);
                    
                    if (SUCCEEDED(hr))
                    {
                        LPWSTR pwszID = nullptr;
                        hr = pDevice->GetId(&pwszID);
                        
                        if (SUCCEEDED(hr))
                        {
                            IPropertyStore* pProps = nullptr;
                            hr = pDevice->OpenPropertyStore(STGM_READ, &pProps);
                            
                            if (SUCCEEDED(hr))
                            {
                                PROPVARIANT varName;
                                PropVariantInit(&varName);
                                
                                hr = pProps->GetValue(PKEY_Device_FriendlyName, &varName);
                                
                                if (SUCCEEDED(hr))
                                {
                                    FEFAudioDeviceInfo DeviceInfo;
                                    DeviceInfo.DeviceName = FString(varName.pwszVal);
                                    DeviceInfo.DeviceID = FString(pwszID);
                                    DeviceInfo.bIsInputDevice = true;
                                    DeviceInfo.bIsDefaultDevice = (defaultDeviceId && wcscmp(pwszID, defaultDeviceId) == 0);
                                    DeviceInfo.bIsConnected = true;
                                    
                                    InputDevices.Add(DeviceInfo);
                                    
                                    if (DeviceInfo.bIsDefaultDevice)
                                    {
                                        DefaultInputDevice = DeviceInfo;
                                    }
                                    
                                    PropVariantClear(&varName);
                                }
                                
                                pProps->Release();
                            }
                            
                            CoTaskMemFree(pwszID);
                        }
                        
                        pDevice->Release();
                    }
                }
                
                if (defaultDeviceId)
                {
                    CoTaskMemFree(defaultDeviceId);
                }
                
                pCollection->Release();
            }
        }
        
        pEnumerator->Release();
    }
    
    CoUninitialize();
}
#endif

void UEFAudioDeviceManager::StartDeviceMonitoring()
{
    if (UWorld* World = GEngine->GetWorld())
    {
        World->GetTimerManager().SetTimer(
            DeviceMonitorTimerHandle,
            FTimerDelegate::CreateUObject(this, &UEFAudioDeviceManager::CheckDeviceConnectivity),
            2.0f, // Check every 2 seconds
            true);
    }
}

void UEFAudioDeviceManager::CheckDeviceConnectivity()
{
    // Store previous device lists to compare against
    TArray<FEFAudioDeviceInfo> PreviousOutputDevices = OutputDevices;
    TArray<FEFAudioDeviceInfo> PreviousInputDevices = InputDevices;
    
    // Refresh the device lists
    PlatformEnumerateAudioDevices();
    
    // Check for disconnected output devices
    for (const FEFAudioDeviceInfo& PreviousDevice : PreviousOutputDevices)
    {
        bool bStillConnected = false;
        
        for (const FEFAudioDeviceInfo& CurrentDevice : OutputDevices)
        {
            if (PreviousDevice.DeviceID == CurrentDevice.DeviceID)
            {
                bStillConnected = true;
                break;
            }
        }
        
        if (!bStillConnected && PreviousDevice.bIsConnected)
        {
            // Device was disconnected, broadcast event
            FEFAudioDeviceInfo DisconnectedDevice = PreviousDevice;
            DisconnectedDevice.bIsConnected = false;
            OnDeviceDisconnected.Broadcast(DisconnectedDevice);
        }
    }
    
    // Check for disconnected input devices
    for (const FEFAudioDeviceInfo& PreviousDevice : PreviousInputDevices)
    {
        bool bStillConnected = false;
        
        for (const FEFAudioDeviceInfo& CurrentDevice : InputDevices)
        {
            if (PreviousDevice.DeviceID == CurrentDevice.DeviceID)
            {
                bStillConnected = true;
                break;
            }
        }
        
        if (!bStillConnected && PreviousDevice.bIsConnected)
        {
            // Device was disconnected, broadcast event
            FEFAudioDeviceInfo DisconnectedDevice = PreviousDevice;
            DisconnectedDevice.bIsConnected = false;
            OnDeviceDisconnected.Broadcast(DisconnectedDevice);
        }
    }
    
    // Notify about any changes to the device lists
    if (OnAudioDevicesChanged.IsBound())
    {
        // Combine output and input devices for the event
        TArray<FEFAudioDeviceInfo> AllDevices;
        AllDevices.Append(OutputDevices);
        AllDevices.Append(InputDevices);
        OnAudioDevicesChanged.Broadcast(AllDevices);
    }
}

TArray<FEFAudioDeviceInfo> UEFAudioDeviceManager::GetOutputDevices() const
{
    return OutputDevices;
}

TArray<FEFAudioDeviceInfo> UEFAudioDeviceManager::GetInputDevices() const
{
    return InputDevices;
}

FEFAudioDeviceInfo UEFAudioDeviceManager::GetDefaultOutputDevice() const
{
    return DefaultOutputDevice;
}

FEFAudioDeviceInfo UEFAudioDeviceManager::GetDefaultInputDevice() const
{
    return DefaultInputDevice;
}

bool UEFAudioDeviceManager::SetOutputDevice(const FString& DeviceID)
{
#if PLATFORM_WINDOWS
    return PlatformSetOutputDevice(DeviceID);
#else
    return false;
#endif
}

bool UEFAudioDeviceManager::SetInputDevice(const FString& DeviceID)
{
#if PLATFORM_WINDOWS
    return PlatformSetInputDevice(DeviceID);
#else
    return false;
#endif
}

#if PLATFORM_WINDOWS
bool UEFAudioDeviceManager::PlatformSetOutputDevice(const FString& DeviceID)
{
    bool bSuccess = false;
    CoInitialize(nullptr);
    
    IMMDeviceEnumerator* pEnumerator = nullptr;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                  __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    
    if (SUCCEEDED(hr))
    {
        IMMDevice* pDevice = nullptr;
        hr = pEnumerator->GetDevice(LPCWSTR(*DeviceID), &pDevice);
        /*
        if (SUCCEEDED(hr))
        {
            IPolicyConfig* pPolicyConfig = nullptr;
            hr = CoCreateInstance(__uuidof(CPolicyConfigClient),
                                nullptr, CLSCTX_ALL,
                                __uuidof(IPolicyConfig),
                                (LPVOID*)&pPolicyConfig);
            
            if (SUCCEEDED(hr))
            {
                hr = pPolicyConfig->SetDefaultEndpoint(LPCWSTR(*DeviceID), eConsole);
                bSuccess = SUCCEEDED(hr);
                pPolicyConfig->Release();
            }
            
            pDevice->Release();
        }
        */
        
        pEnumerator->Release();
    }
    
    CoUninitialize();
    return bSuccess;
}

bool UEFAudioDeviceManager::PlatformSetInputDevice(const FString& DeviceID)
{
    bool bSuccess = false;
    CoInitialize(nullptr);
    
    IMMDeviceEnumerator* pEnumerator = nullptr;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                  __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    
    if (SUCCEEDED(hr))
    {
        IMMDevice* pDevice = nullptr;
        hr = pEnumerator->GetDevice(LPCWSTR(*DeviceID), &pDevice);
        /*
        if (SUCCEEDED(hr))
        {
            IPolicyConfig* pPolicyConfig = nullptr;
            hr = CoCreateInstance(__uuidof(CPolicyConfigClient),
                                nullptr, CLSCTX_ALL,
                                __uuidof(IPolicyConfig),
                                (LPVOID*)&pPolicyConfig);
            
            if (SUCCEEDED(hr))
            {
                hr = pPolicyConfig->SetDefaultEndpoint(LPCWSTR(*DeviceID), eCommunications);
                bSuccess = SUCCEEDED(hr);
                pPolicyConfig->Release();
            }
            
            pDevice->Release();
        }
        */
        
        pEnumerator->Release();
    }
    
    CoUninitialize();
    return bSuccess;
}
#endif