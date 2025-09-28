// Fill out your copyright notice in the Description page of Project Settings.

#include "EFSettingsSubsystem.h"
#include "AudioDevice.h"
#include "Kismet/GameplayStatics.h"
#include "Device/EFAudioDevice.h"
#include "GameFramework/GameUserSettings.h"
#include "UnrealExtendedFramework/Libraries/Monitor/EFMonitorLibrary.h"


void UEFSettingsSubsystem::SetGameplaySettings(const FExtendedGameplaySettings& GameplaySettings)
{
	TemporaryGameplaySettings = GameplaySettings;
	OnExtendedSettingsChanged.Broadcast();
}


void UEFSettingsSubsystem::SetAudioSettings(const FExtendedAudioSettings& AudioSettings)
{
	TemporaryAudioSettings = AudioSettings;
	OnExtendedSettingsChanged.Broadcast();
}


void UEFSettingsSubsystem::SetGraphicsSettings(const FExtendedGraphicsSettings& GraphicsSettings)
{
	TemporaryGraphicsSettings = GraphicsSettings;
	OnExtendedSettingsChanged.Broadcast();
}


void UEFSettingsSubsystem::SetDisplaySettings(const FExtendedDisplaySettings& DisplaySettings)
{
	TemporaryDisplaySettings = DisplaySettings;
	OnExtendedSettingsChanged.Broadcast();
}


void UEFSettingsSubsystem::SaveExtendedSettings() const
{
	// Save the config directly
	ExtendedSettings->SaveConfig();
    
	// Also save to the game user settings
	if (GEngine && GEngine->GameUserSettings)
	{
		GEngine->GameUserSettings->SaveConfig();
		GEngine->GameUserSettings->SaveSettings();
	}
    
	OnExtendedSettingsApplied.Broadcast();
}


void UEFSettingsSubsystem::FindAndApplyBestSettings()
{
	ExtendedSettings->bCheckedBestSettings = true;
	
	// Get system information
	const bool bIsHighEndGPU = FPlatformMisc::NumberOfCores() >= 4;
	const uint32 SystemMemoryMB = FPlatformMemory::GetPhysicalGBRam() * 1024;
	const bool bIsHighEndSystem = SystemMemoryMB >= 16384; // 16GB or more RAM

	// Configure graphics settings based on system capabilities
	FExtendedGraphicsSettings GraphicsSettings = ExtendedSettings->GraphicsSettings;
	GraphicsSettings.OverallQuality = bIsHighEndSystem ? 3 : 2; // High or Medium
	GraphicsSettings.ViewDistance = bIsHighEndSystem ? 3 : 2;
	GraphicsSettings.PostProcessing = bIsHighEndSystem ? 3 : 2;
	GraphicsSettings.EffectsQuality = bIsHighEndSystem ? 3 : 2;
	GraphicsSettings.ShadowQuality = bIsHighEndSystem ? 3 : 1;
	GraphicsSettings.FoliageQuality = bIsHighEndSystem ? 3 : 1;
	GraphicsSettings.TextureQuality = bIsHighEndSystem ? 3 : 2;
	GraphicsSettings.ShaderQuality = bIsHighEndSystem ? 3 : 2;
	GraphicsSettings.ResolutionScale = 100;

	// Configure display settings
	FExtendedDisplaySettings DisplaySettings = ExtendedSettings->DisplaySettings;
	DisplaySettings.VerticalSync = true;
	DisplaySettings.FrameRateLimit = bIsHighEndSystem ? "144" : "60";

	// Get the current monitor's resolution (the one the game window is on)
	int32 CurrentMonitorWidth, CurrentMonitorHeight;
	UEFMonitorLibrary::GetCurrentMonitorResolution(CurrentMonitorWidth, CurrentMonitorHeight);
	FIntPoint CurrentMonitorResolution(CurrentMonitorWidth, CurrentMonitorHeight);
	
	// Update screen resolution list to ensure we have valid options
	UpdateScreenResolutionList(true);
	
	// Set the initial resolution to match the current monitor if possible
	if (DisplaySettings.ScreenResolutions.Num() > 0)
	{
		// Try to find a resolution that matches the current monitor's native resolution
		FString CurrentMonitorResStr = FString::Printf(TEXT("%dx%d"), CurrentMonitorResolution.X, CurrentMonitorResolution.Y);
		FName CurrentMonitorResFName = FName(*CurrentMonitorResStr);
		
		if (DisplaySettings.ScreenResolutions.Contains(CurrentMonitorResFName))
		{
			// Use the current monitor's native resolution if available
			DisplaySettings.ScreenResolution = CurrentMonitorResFName;
		}
		else
		{
			// Otherwise use the highest available resolution that fits the display
			DisplaySettings.ScreenResolution = DisplaySettings.ScreenResolutions.Last();
		}
	}

	// Configure gameplay settings with reasonable defaults
	FExtendedGameplaySettings GameplaySettings = ExtendedSettings->GameplaySettings;
	GameplaySettings.bDepthOfField = bIsHighEndSystem;

	// Configure audio settings with reasonable defaults
	FExtendedAudioSettings AudioSettings = ExtendedSettings->AudioSettings;

	// Apply all settings
	ExtendedSettings->GraphicsSettings = GraphicsSettings;
	ExtendedSettings->DisplaySettings = DisplaySettings;
	ExtendedSettings->GameplaySettings = GameplaySettings;
	ExtendedSettings->AudioSettings = AudioSettings;
	
	// Save the configured settings
	SaveExtendedSettings();
}


void UEFSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (!ExtendedSettings->bAutoApplySettings)
	{
		return;
	}
	
	EFAudioDeviceManager = NewObject<UEFAudioDeviceManager>(this);
	EFAudioDeviceManager->Initialize();
    
	// Bind to audio device change events
	EFAudioDeviceManager->OnAudioDevicesChanged.AddDynamic(this, &UEFSettingsSubsystem::OnAudioDevicesChanged);
	EFAudioDeviceManager->OnDeviceDisconnected.AddDynamic(this, &UEFSettingsSubsystem::OnAudioDeviceDisconnected);
    
	// Initialize audio device lists in settings
	UpdateAudioDeviceLists();
	
	if (ExtendedSettings->bCheckedBestSettings == false)
	{
		FindAndApplyBestSettings();
	}
	
	if (GetWorld())
	{
		FTimerHandle TimerHandle;
		GetWorld()->GetTimerManager().SetTimer(TimerHandle, [this]()
		{
			UGameplayStatics::SetBaseSoundMix(GetWorld(), AudioMixer->GlobalSoundMix.LoadSynchronous());
			UGameplayStatics::PushSoundMixModifier(GetWorld(), AudioMixer->GlobalSoundMix.LoadSynchronous());
			ApplySettings();
		}, 0.2f, false);
	}
}


void UEFSettingsSubsystem::Deinitialize()
{
	if (EFAudioDeviceManager)
	{
		// Unbind from audio device change events
		EFAudioDeviceManager->OnAudioDevicesChanged.RemoveDynamic(this, &UEFSettingsSubsystem::OnAudioDevicesChanged);
		EFAudioDeviceManager->OnDeviceDisconnected.RemoveDynamic(this, &UEFSettingsSubsystem::OnAudioDeviceDisconnected);
		EFAudioDeviceManager->Deinitialize();
		EFAudioDeviceManager = nullptr;
	}

	// Reset all timers
	GetWorld()->GetTimerManager().ClearAllTimersForObject(this);

	// Save settings before deinitializing
	SaveExtendedSettings();
	
	Super::Deinitialize();
}


void UEFSettingsSubsystem::PostInitProperties()
{
	Super::PostInitProperties();
	
	ExtendedSettings = GetMutableDefault<UEFDeveloperSettings>();
	AudioMixer = GetMutableDefault<UEFAudioDeveloperSettings>();

	// Initialize screen resolution list in settings
	UpdateScreenResolutionList();

	StoreTemporarySettings();
}


void UEFSettingsSubsystem::StoreTemporarySettings()
{
	TemporaryGameplaySettings = ExtendedSettings->GameplaySettings;
	TemporaryAudioSettings = ExtendedSettings->AudioSettings;
	TemporaryGraphicsSettings = ExtendedSettings->GraphicsSettings;
	TemporaryDisplaySettings = ExtendedSettings->DisplaySettings;
}


void UEFSettingsSubsystem::ApplyGameplaySettings(const FExtendedGameplaySettings& Settings)
{
	// Store and save settings
	ExtendedSettings->GameplaySettings = Settings;
	SaveExtendedSettings();
}


void UEFSettingsSubsystem::ApplyAudioSettings(const FExtendedAudioSettings& Settings)
{
    // Apply audio device settings
    if (EFAudioDeviceManager)
    {
        if (!Settings.AudioOutputDeviceName.IsNone())
        {
            for (const auto& Device : EFAudioDeviceManager->GetOutputDevices())
            {
                if (Device.DeviceName == Settings.AudioOutputDeviceName.ToString())
                {
                    EFAudioDeviceManager->SetOutputDevice(Device.DeviceID);
                    break;
                }
            }
        }

        if (!Settings.AudioInputDeviceName.IsNone())
        {
            for (const auto& Device : EFAudioDeviceManager->GetInputDevices())
            {
                if (Device.DeviceName == Settings.AudioInputDeviceName.ToString())
                {
                    EFAudioDeviceManager->SetInputDevice(Device.DeviceID);
                    break;
                }
            }
        }
    }

	AudioMixer = GetMutableDefault<UEFAudioDeveloperSettings>();

    // Apply sound mix settings
    if (GetWorld() && AudioMixer->GlobalSoundMix.IsValid())
    {
	    if (AudioMixer->MasterSoundClass.IsValid())
	    	UGameplayStatics::SetSoundMixClassOverride(GetWorld(), AudioMixer->GlobalSoundMix.LoadSynchronous(),AudioMixer->MasterSoundClass.LoadSynchronous() , Settings.MasterVolume, 1.0f, 0.0f);

    	if (AudioMixer->MusicSoundClass.IsValid())
    		UGameplayStatics::SetSoundMixClassOverride(GetWorld(), AudioMixer->GlobalSoundMix.LoadSynchronous(), AudioMixer->MusicSoundClass.LoadSynchronous(), Settings.MusicVolume, 1.0f, 0.0f);

    	if (AudioMixer->EffectsSoundClass.IsValid())
    		UGameplayStatics::SetSoundMixClassOverride(GetWorld(), AudioMixer->GlobalSoundMix.LoadSynchronous(), AudioMixer->EffectsSoundClass.LoadSynchronous(), Settings.EffectsVolume, 1.0f, 0.0f);

		if (AudioMixer->VoiceSoundClass.IsValid())
    		UGameplayStatics::SetSoundMixClassOverride(GetWorld(), AudioMixer->GlobalSoundMix.LoadSynchronous(), AudioMixer->VoiceSoundClass.LoadSynchronous(), Settings.VoiceChatVolume, 1.0f, 0.0f);
    }

    // Store and save settings
    ExtendedSettings->AudioSettings = Settings;
	SaveExtendedSettings();
}


void UEFSettingsSubsystem::ApplyGraphicsSettings(const FExtendedGraphicsSettings& Settings)
{
	// Apply various graphics settings through console commands
	if (GetWorld())
	{
		FString ResolutionQuality = FString::Printf(TEXT("sg.ResolutionQuality %d"), Settings.ResolutionScale);
		GEngine->Exec(GetWorld(), *ResolutionQuality);

		FString ViewDistance = FString::Printf(TEXT("sg.ViewDistanceQuality %d"), Settings.ViewDistance);
		GEngine->Exec(GetWorld(), *ViewDistance);

		//FString AntiAliasing = FString::Printf(TEXT("sg.AntiAliasingQuality %d"), Settings.AntiAliasing);
		//GEngine->Exec(GetWorld(), *AntiAliasing);

		FString PostProcessing = FString::Printf(TEXT("sg.PostProcessQuality %d"), Settings.PostProcessing);
		GEngine->Exec(GetWorld(), *PostProcessing);

		FString ShadowQuality = FString::Printf(TEXT("sg.ShadowQuality %d"), Settings.ShadowQuality);
		GEngine->Exec(GetWorld(), *ShadowQuality);

		FString TextureQuality = FString::Printf(TEXT("sg.TextureQuality %d"), Settings.TextureQuality);
		GEngine->Exec(GetWorld(), *TextureQuality);

		FString EffectsQuality = FString::Printf(TEXT("sg.EffectsQuality %d"), Settings.EffectsQuality);
		GEngine->Exec(GetWorld(), *EffectsQuality);

		FString FoliageQuality = FString::Printf(TEXT("sg.FoliageQuality %d"), Settings.FoliageQuality);
		GEngine->Exec(GetWorld(), *FoliageQuality);
	}

	// Store and save settings
	ExtendedSettings->GraphicsSettings = Settings;
	SaveExtendedSettings();
}


void UEFSettingsSubsystem::ApplyDisplaySettings(const FExtendedDisplaySettings& Settings)
{
	if (!GetWorld())
	{
		return;
	}
	
	// Apply vsync
	FString VSync = FString::Printf(TEXT("r.VSync %d"), Settings.VerticalSync ? 1 : 0);
	GEngine->Exec(GetWorld(), *VSync);

	// Apply frame rate limit
	FString FrameRateLimit = FString::Printf(TEXT("t.MaxFPS %d"), Settings.FrameRateLimit == "Unlimited" ?
		0 : FCString::Atoi(*Settings.FrameRateLimit.ToString()));
	GEngine->Exec(GetWorld(), *FrameRateLimit);

	
	// Get the current monitor's resolution (the one the game window is on)
	int32 CurrentMonitorWidth, CurrentMonitorHeight;
	UEFMonitorLibrary::GetCurrentMonitorResolution(CurrentMonitorWidth, CurrentMonitorHeight);
	FString NativeResStr = FString::Printf(TEXT("%dx%d"), CurrentMonitorWidth, CurrentMonitorHeight);

	FName SelectedResolution = Settings.ScreenResolution;

	// If no resolution is set, use the current monitor's native resolution
	if (SelectedResolution.IsNone()) SelectedResolution = FName(*NativeResStr);

	// Apply screen resolution
	FString ResolutionStr = SelectedResolution.ToString();
	FString WidthStr, HeightStr;
	if (ResolutionStr.Split(TEXT("x"), &WidthStr, &HeightStr))
	{
		int32 Width = FCString::Atoi(*WidthStr);
		int32 Height = FCString::Atoi(*HeightStr);
				
		// Check if the requested resolution exceeds the current monitor's capabilities
		if (Width > CurrentMonitorWidth || Height > CurrentMonitorHeight)
		{
			UE_LOG(LogTemp, Warning, TEXT("Requested resolution (%dx%d) exceeds current monitor capabilities (%dx%d). Adjusting to current monitor resolution."),
			Width, Height, CurrentMonitorWidth, CurrentMonitorHeight);
					
			// Adjust to current monitor resolution
			Width = CurrentMonitorWidth;
			Height = CurrentMonitorHeight;
		}
				
		if (Width > 0 && Height > 0)
		{
			// Get the game user settings
			if (GEngine && GEngine->GameUserSettings)
			{
				UGameUserSettings* UserSettings = GEngine->GameUserSettings;
				// Set the resolution
				UserSettings->SetScreenResolution(FIntPoint(Width, Height));
				UserSettings->ApplyResolutionSettings(false);
			}
		}
	}
	
	// Apply display mode
	if (!Settings.DisplayMode.IsNone())
	{
		EWindowMode::Type WindowMode = EWindowMode::Type::WindowedFullscreen; // Default to WindowedFullscreen
			
		if (Settings.DisplayMode.ToString() == "Windowed")
		{
			WindowMode = EWindowMode::Windowed;
		}
		else if (Settings.DisplayMode.ToString() == "Windowed Fullscreen")
		{
			WindowMode = EWindowMode::WindowedFullscreen;
		}
		else if (Settings.DisplayMode.ToString() == "Fullscreen")
		{
			WindowMode = EWindowMode::Fullscreen;

			if (Settings.ScreenResolution.ToString() != NativeResStr)
			{
				// Optionally: warn or auto-set
				GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Yellow, TEXT("For exclusive fullscreen, using native monitor resolution for proper stretching."));
				if (GEngine && GEngine->GameUserSettings)
				{
					GEngine->GameUserSettings->SetScreenResolution(FIntPoint(CurrentMonitorWidth, CurrentMonitorHeight));
					GEngine->GameUserSettings->ApplyResolutionSettings(false);
				}
			}
		}
			
		// Get the game user settings
		if (GEngine && GEngine->GameUserSettings)
		{
			UGameUserSettings* UserSettings = GEngine->GameUserSettings;
			UserSettings->SetFullscreenMode(WindowMode);
			UserSettings->ApplyResolutionSettings(false);
		}
	}


	// Apply anti-aliasing mode
	if (!Settings.AntiAliasingMode.IsNone())
	{
		FString AACommand;

		if (Settings.AntiAliasingMode == FName("None"))
		{
			AACommand = TEXT("r.DefaultFeature.AntiAliasing 0"); // No anti-aliasing
		}
		else if (Settings.AntiAliasingMode == FName("FXAA"))
		{
			AACommand = TEXT("r.DefaultFeature.AntiAliasing 1"); // FXAA
		}
		else if (Settings.AntiAliasingMode == FName("TAA"))
		{
			AACommand = TEXT("r.DefaultFeature.AntiAliasing 2"); // Temporal AA
		}
		else if (Settings.AntiAliasingMode == FName("MSAA"))
		{
			AACommand = TEXT("r.DefaultFeature.AntiAliasing 3"); // MSAA
		}
		
		if (!AACommand.IsEmpty())
		{
			GEngine->Exec(GetWorld(), *AACommand);
		}
	}
	

	// Store and save settings
	ExtendedSettings->DisplaySettings = Settings;
	SaveExtendedSettings();
}


void UEFSettingsSubsystem::ApplySettings()
{
	ApplyGameplaySettings(TemporaryGameplaySettings);
	ApplyAudioSettings(TemporaryAudioSettings);
	ApplyGraphicsSettings(TemporaryGraphicsSettings);
	ApplyDisplaySettings(TemporaryDisplaySettings);
	SaveExtendedSettings();
}


void UEFSettingsSubsystem::RevertSettings()
{
	TemporaryGameplaySettings = ExtendedSettings->GameplaySettings;
	TemporaryAudioSettings = ExtendedSettings->AudioSettings;
	TemporaryGraphicsSettings = ExtendedSettings->GraphicsSettings;
	TemporaryDisplaySettings = ExtendedSettings->DisplaySettings;
}






void UEFSettingsSubsystem::OnAudioDevicesChanged(const TArray<FEFAudioDeviceInfo>& AudioDevices)
{
    // Update the audio device lists in settings
    UpdateAudioDeviceLists();
}


void UEFSettingsSubsystem::OnAudioDeviceDisconnected(const FEFAudioDeviceInfo& DisconnectedDevice)
{
    // Update the audio device lists in settings
    UpdateAudioDeviceLists();
    
    // Check if the disconnected device was the currently selected one
    if (!DisconnectedDevice.bIsInputDevice && 
        ExtendedSettings->AudioSettings.AudioOutputDeviceName.ToString() == DisconnectedDevice.DeviceName)
    {
        // Reset to default output device
        FExtendedAudioSettings CurrentSettings = ExtendedSettings->AudioSettings;
        FEFAudioDeviceInfo DefaultDevice = EFAudioDeviceManager->GetDefaultOutputDevice();
        CurrentSettings.AudioOutputDeviceName = FName(*DefaultDevice.DeviceName);
        ApplyAudioSettings(CurrentSettings);
    }
    else if (DisconnectedDevice.bIsInputDevice && 
             ExtendedSettings->AudioSettings.AudioInputDeviceName.ToString() == DisconnectedDevice.DeviceName)
    {
        // Reset to default input device
        FExtendedAudioSettings CurrentSettings = ExtendedSettings->AudioSettings;
        FEFAudioDeviceInfo DefaultDevice = EFAudioDeviceManager->GetDefaultInputDevice();
        CurrentSettings.AudioInputDeviceName = FName(*DefaultDevice.DeviceName);
        ApplyAudioSettings(CurrentSettings);
    }
}


void UEFSettingsSubsystem::UpdateAudioDeviceLists()
{
    if (!EFAudioDeviceManager)
    {
        return;
    }
    
    // Get current audio settings
    FExtendedAudioSettings CurrentSettings = ExtendedSettings->AudioSettings;
    
    // Update output devices list
    TArray<FEFAudioDeviceInfo> OutputDevices = EFAudioDeviceManager->GetOutputDevices();
    CurrentSettings.AudioOutputDevices.Empty();
    
    for (const FEFAudioDeviceInfo& Device : OutputDevices)
    {
        CurrentSettings.AudioOutputDevices.Add(FName(*Device.DeviceName));
    }
    
    // Update input devices list
    TArray<FEFAudioDeviceInfo> InputDevices = EFAudioDeviceManager->GetInputDevices();
    CurrentSettings.AudioInputDevices.Empty();
    
    for (const FEFAudioDeviceInfo& Device : InputDevices)
    {
        CurrentSettings.AudioInputDevices.Add(FName(*Device.DeviceName));
    }
    
    // If no output device is selected, set to default
    if (CurrentSettings.AudioOutputDeviceName.IsNone() && OutputDevices.Num() > 0)
    {
        FEFAudioDeviceInfo DefaultDevice = EFAudioDeviceManager->GetDefaultOutputDevice();
        CurrentSettings.AudioOutputDeviceName = FName(*DefaultDevice.DeviceName);
    }
    
    // If no input device is selected, set to default
    if (CurrentSettings.AudioInputDeviceName.IsNone() && InputDevices.Num() > 0)
    {
        FEFAudioDeviceInfo DefaultDevice = EFAudioDeviceManager->GetDefaultInputDevice();
        CurrentSettings.AudioInputDeviceName = FName(*DefaultDevice.DeviceName);
    }
    
    // Update both temporary and saved settings
    TemporaryAudioSettings = CurrentSettings;
    ExtendedSettings->AudioSettings = CurrentSettings;
    SaveExtendedSettings();
}


void UEFSettingsSubsystem::UpdateScreenResolutionList(bool bForceBestResolution)
{
    FExtendedDisplaySettings CurrentSettings = ExtendedSettings->DisplaySettings;
    CurrentSettings.ScreenResolutions.Empty();

    int32 CurrentMonitorWidth, CurrentMonitorHeight;
    UEFMonitorLibrary::GetCurrentMonitorResolution(CurrentMonitorWidth, CurrentMonitorHeight);
    FIntPoint CurrentMonitorResolution(CurrentMonitorWidth, CurrentMonitorHeight);

    TArray<FIntPoint> SupportedResolutions = UEFMonitorLibrary::GetCurrentMonitorSupportedResolutions();

    // Acceptable aspect ratios (width / height)
    TArray<float> AllowedAspectRatios = { 16.0f / 9.0f, 16.0f / 10.0f, 4.0f / 3.0f };

    for (const FIntPoint& Resolution : SupportedResolutions)
    {
        if (Resolution.X > CurrentMonitorWidth || Resolution.Y > CurrentMonitorHeight)
            continue; // Skip resolutions larger than the monitor

        float Aspect = float(Resolution.X) / float(Resolution.Y);
        bool bAspectOk = false;
        for (float Allowed : AllowedAspectRatios)
        {
            if (FMath::Abs(Aspect - Allowed) < 0.05f)
            {
                bAspectOk = true;
                break;
            }
        }
        if (!bAspectOk)
            continue; // Skip unusual aspect ratios

        FString ResolutionStr = FString::Printf(TEXT("%dx%d"), Resolution.X, Resolution.Y);
        CurrentSettings.ScreenResolutions.AddUnique(FName(*ResolutionStr));
    }

    // Fallback if none found
    if (CurrentSettings.ScreenResolutions.Num() == 0)
    {
        FString FallbackResolution = TEXT("1280x720");
        CurrentSettings.ScreenResolutions.Add(FName(*FallbackResolution));
        CurrentSettings.ScreenResolution = FName(*FallbackResolution);
    }
    else
    {
        // Set to native or highest available
        FString NativeResStr = FString::Printf(TEXT("%dx%d"), CurrentMonitorResolution.X, CurrentMonitorResolution.Y);
        FName NativeResFName = FName(*NativeResStr);

    	if (!CurrentSettings.ScreenResolutions.Contains(CurrentSettings.ScreenResolution))
    	{
    		if (CurrentSettings.ScreenResolutions.Contains(NativeResFName))
    			CurrentSettings.ScreenResolution = NativeResFName;
    		else
    			CurrentSettings.ScreenResolution = CurrentSettings.ScreenResolutions.Last();
    	}
    	else if (bForceBestResolution)
	    {
    		if (CurrentSettings.ScreenResolutions.Contains(NativeResFName))
    			CurrentSettings.ScreenResolution = NativeResFName;
    		else
    			CurrentSettings.ScreenResolution = CurrentSettings.ScreenResolutions.Last();
	    }
    }

    TemporaryDisplaySettings = CurrentSettings;
    ExtendedSettings->DisplaySettings = CurrentSettings;
    SaveExtendedSettings();
}
