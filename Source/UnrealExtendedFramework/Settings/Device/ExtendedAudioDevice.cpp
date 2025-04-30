// Fill out your copyright notice in the Description page of Project Settings.


#include "ExtendedAudioDevice.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <endpointvolume.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

UExtendedAudioDeviceManager::UExtendedAudioDeviceManager()
{
}

void UExtendedAudioDeviceManager::Initialize()
{
    RefreshDeviceList();
    StartDeviceMonitoring();
}

void UExtendedAudioDeviceManager::RefreshDeviceList()
{
    OutputDevices.Empty();
    InputDevices.Empty();
    PlatformEnumerateAudioDevices();
    OnAudioDevicesChanged.Broadcast(OutputDevices);
}

#if PLATFORM_WINDOWS
void UExtendedAudioDeviceManager::PlatformEnumerateAudioDevices()
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
                                    FExtendedAudioDeviceInfo DeviceInfo;
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
                                    FExtendedAudioDeviceInfo DeviceInfo;
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

void UExtendedAudioDeviceManager::StartDeviceMonitoring()
{
    if (UWorld* World = GEngine->GetWorld())
    {
        World->GetTimerManager().SetTimer(
            DeviceMonitorTimerHandle,
            FTimerDelegate::CreateUObject(this, &UExtendedAudioDeviceManager::CheckDeviceConnectivity),
            2.0f, // Check every 2 seconds
            true);
    }
}

void UExtendedAudioDeviceManager::CheckDeviceConnectivity()
{
    // Store previous device lists to compare against
    TArray<FExtendedAudioDeviceInfo> PreviousOutputDevices = OutputDevices;
    TArray<FExtendedAudioDeviceInfo> PreviousInputDevices = InputDevices;
    
    // Refresh the device lists
    PlatformEnumerateAudioDevices();
    
    // Check for disconnected output devices
    for (const FExtendedAudioDeviceInfo& PreviousDevice : PreviousOutputDevices)
    {
        bool bStillConnected = false;
        
        for (const FExtendedAudioDeviceInfo& CurrentDevice : OutputDevices)
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
            FExtendedAudioDeviceInfo DisconnectedDevice = PreviousDevice;
            DisconnectedDevice.bIsConnected = false;
            OnDeviceDisconnected.Broadcast(DisconnectedDevice);
        }
    }
    
    // Check for disconnected input devices
    for (const FExtendedAudioDeviceInfo& PreviousDevice : PreviousInputDevices)
    {
        bool bStillConnected = false;
        
        for (const FExtendedAudioDeviceInfo& CurrentDevice : InputDevices)
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
            FExtendedAudioDeviceInfo DisconnectedDevice = PreviousDevice;
            DisconnectedDevice.bIsConnected = false;
            OnDeviceDisconnected.Broadcast(DisconnectedDevice);
        }
    }
    
    // Notify about any changes to the device lists
    if (OnAudioDevicesChanged.IsBound())
    {
        // Combine output and input devices for the event
        TArray<FExtendedAudioDeviceInfo> AllDevices;
        AllDevices.Append(OutputDevices);
        AllDevices.Append(InputDevices);
        OnAudioDevicesChanged.Broadcast(AllDevices);
    }
}

TArray<FExtendedAudioDeviceInfo> UExtendedAudioDeviceManager::GetOutputDevices() const
{
    return OutputDevices;
}

TArray<FExtendedAudioDeviceInfo> UExtendedAudioDeviceManager::GetInputDevices() const
{
    return InputDevices;
}

FExtendedAudioDeviceInfo UExtendedAudioDeviceManager::GetDefaultOutputDevice() const
{
    return DefaultOutputDevice;
}

FExtendedAudioDeviceInfo UExtendedAudioDeviceManager::GetDefaultInputDevice() const
{
    return DefaultInputDevice;
}

bool UExtendedAudioDeviceManager::SetOutputDevice(const FString& DeviceID)
{
#if PLATFORM_WINDOWS
    return PlatformSetOutputDevice(DeviceID);
#else
    return false;
#endif
}

bool UExtendedAudioDeviceManager::SetInputDevice(const FString& DeviceID)
{
#if PLATFORM_WINDOWS
    return PlatformSetInputDevice(DeviceID);
#else
    return false;
#endif
}

#if PLATFORM_WINDOWS
bool UExtendedAudioDeviceManager::PlatformSetOutputDevice(const FString& DeviceID)
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

bool UExtendedAudioDeviceManager::PlatformSetInputDevice(const FString& DeviceID)
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