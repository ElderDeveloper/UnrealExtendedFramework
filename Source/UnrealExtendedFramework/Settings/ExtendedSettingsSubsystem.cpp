// Fill out your copyright notice in the Description page of Project Settings.

#include "ExtendedSettingsSubsystem.h"
#include "AudioDevice.h"
#include "Kismet/GameplayStatics.h"
#include "Device/ExtendedAudioDevice.h"
#include "GameFramework/GameUserSettings.h"
#include "Library/ExtendedMonitorLibrary.h"


void UExtendedSettingsSubsystem::SetGameplaySettings(const FExtendedGameplaySettings& GameplaySettings)
{
	TemporaryGameplaySettings = GameplaySettings;
	OnExtendedSettingsChanged.Broadcast();
}


void UExtendedSettingsSubsystem::SetAudioSettings(const FExtendedAudioSettings& AudioSettings)
{
	TemporaryAudioSettings = AudioSettings;
	OnExtendedSettingsChanged.Broadcast();
}


void UExtendedSettingsSubsystem::SetGraphicsSettings(const FExtendedGraphicsSettings& GraphicsSettings)
{
	TemporaryGraphicsSettings = GraphicsSettings;
	OnExtendedSettingsChanged.Broadcast();
}


void UExtendedSettingsSubsystem::SetDisplaySettings(const FExtendedDisplaySettings& DisplaySettings)
{
	TemporaryDisplaySettings = DisplaySettings;
	OnExtendedSettingsChanged.Broadcast();
}


void UExtendedSettingsSubsystem::FindAndApplyBestSettings()
{
	ExtendedSettings->bCheckedBestSettings = true;
	ExtendedSettings->SaveExtendedSettings();

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
	UExtendedMonitorLibrary::GetCurrentMonitorResolution(CurrentMonitorWidth, CurrentMonitorHeight);
	FIntPoint CurrentMonitorResolution(CurrentMonitorWidth, CurrentMonitorHeight);
	
	// Update screen resolution list to ensure we have valid options
	UpdateScreenResolutionList();
	
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
	GameplaySettings.FieldOfView = 90.0f;
	GameplaySettings.bHeadBob = true;
	GameplaySettings.bCrosshair = true;
	GameplaySettings.bFPSCounter = false;
	GameplaySettings.bDepthOfField = bIsHighEndSystem;

	// Configure audio settings with reasonable defaults
	FExtendedAudioSettings AudioSettings = ExtendedSettings->AudioSettings;
	AudioSettings.MasterVolume = 1.0f;
	AudioSettings.MusicVolume = 0.8f;
	AudioSettings.EffectsVolume = 1.0f;
	AudioSettings.VoiceChatVolume = 1.0f;
	AudioSettings.bVoiceChatEnabled = true;
	AudioSettings.bMuteJumpScare = false;

	// Apply all settings
	ExtendedSettings->GraphicsSettings = GraphicsSettings;
	ExtendedSettings->DisplaySettings = DisplaySettings;
	ExtendedSettings->GameplaySettings = GameplaySettings;
	ExtendedSettings->AudioSettings = AudioSettings;
	
	// Save the configured settings
	ExtendedSettings->SaveExtendedSettings();
}


void UExtendedSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	//ExtendedSettings = GetMutableDefault<UExtendedSettingsDeveloperSettings>();

	ExtendedAudioDeviceManager = NewObject<UExtendedAudioDeviceManager>(this);
	ExtendedAudioDeviceManager->Initialize();
    
	// Bind to audio device change events
	ExtendedAudioDeviceManager->OnAudioDevicesChanged.AddDynamic(this, &UExtendedSettingsSubsystem::OnAudioDevicesChanged);
	ExtendedAudioDeviceManager->OnDeviceDisconnected.AddDynamic(this, &UExtendedSettingsSubsystem::OnAudioDeviceDisconnected);
    
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


void UExtendedSettingsSubsystem::Deinitialize()
{
	if (ExtendedAudioDeviceManager)
	{
		ExtendedAudioDeviceManager->ConditionalBeginDestroy();
		ExtendedAudioDeviceManager = nullptr;
	}
	Super::Deinitialize();
}


void UExtendedSettingsSubsystem::PostInitProperties()
{
	Super::PostInitProperties();
	
	ExtendedSettings = GetMutableDefault<UExtendedSettingsDeveloperSettings>();
	AudioMixer = GetMutableDefault<UExtendedAudioDeveloperSettings>();

	// Initialize screen resolution list in settings
	UpdateScreenResolutionList();

	StoreTemporarySettings();
}


void UExtendedSettingsSubsystem::StoreTemporarySettings()
{
	TemporaryGameplaySettings = ExtendedSettings->GameplaySettings;
	TemporaryAudioSettings = ExtendedSettings->AudioSettings;
	TemporaryGraphicsSettings = ExtendedSettings->GraphicsSettings;
	TemporaryDisplaySettings = ExtendedSettings->DisplaySettings;
}


void UExtendedSettingsSubsystem::ApplyGameplaySettings(const FExtendedGameplaySettings& Settings)
{
	// Apply gameplay settings to the game
	/*
	if (GEngine)
	{
		if (APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0))
		{
			CameraManager->SetFOV(Settings.FieldOfView);
		}
	}
	*/
	// Store and save settings
	ExtendedSettings->GameplaySettings = Settings;
	SaveExtendedSettings();
}


void UExtendedSettingsSubsystem::ApplyAudioSettings(const FExtendedAudioSettings& Settings)
{
    // Apply audio device settings
    if (ExtendedAudioDeviceManager)
    {
        if (!Settings.AudioOutputDeviceName.IsNone())
        {
            for (const auto& Device : ExtendedAudioDeviceManager->GetOutputDevices())
            {
                if (Device.DeviceName == Settings.AudioOutputDeviceName.ToString())
                {
                    ExtendedAudioDeviceManager->SetOutputDevice(Device.DeviceID);
                    break;
                }
            }
        }

        if (!Settings.AudioInputDeviceName.IsNone())
        {
            for (const auto& Device : ExtendedAudioDeviceManager->GetInputDevices())
            {
                if (Device.DeviceName == Settings.AudioInputDeviceName.ToString())
                {
                    ExtendedAudioDeviceManager->SetInputDevice(Device.DeviceID);
                    break;
                }
            }
        }
    }

    // Apply sound mix settings
    if (GetWorld() && AudioMixer->GlobalSoundMix)
    {
    	UGameplayStatics::SetSoundMixClassOverride(GetWorld(), AudioMixer->GlobalSoundMix.LoadSynchronous(),AudioMixer->MasterSoundClass.LoadSynchronous() , Settings.MasterVolume, 1.0f, 0.0f);
    	UGameplayStatics::SetSoundMixClassOverride(GetWorld(), AudioMixer->GlobalSoundMix.LoadSynchronous(), AudioMixer->MusicSoundClass.LoadSynchronous(), Settings.MusicVolume, 1.0f, 0.0f);
    	UGameplayStatics::SetSoundMixClassOverride(GetWorld(), AudioMixer->GlobalSoundMix.LoadSynchronous(), AudioMixer->EffectsSoundClass.LoadSynchronous(), Settings.EffectsVolume, 1.0f, 0.0f);
    	UGameplayStatics::SetSoundMixClassOverride(GetWorld(), AudioMixer->GlobalSoundMix.LoadSynchronous(), AudioMixer->VoiceSoundClass.LoadSynchronous(), Settings.VoiceChatVolume, 1.0f, 0.0f);
    }

    // Store and save settings
    ExtendedSettings->AudioSettings = Settings;
	SaveExtendedSettings();
}


void UExtendedSettingsSubsystem::ApplyGraphicsSettings(const FExtendedGraphicsSettings& Settings)
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


void UExtendedSettingsSubsystem::ApplyDisplaySettings(const FExtendedDisplaySettings& Settings)
{
	if (!GetWorld())
	{
		return;
	}
	
	// Apply vsync
	FString VSync = FString::Printf(TEXT("r.VSync %d"), Settings.VerticalSync ? 1 : 0);
	GEngine->Exec(GetWorld(), *VSync);

	// Apply frame rate limit
	if (Settings.FrameRateLimit == "Unlimited")
		{
			FString FrameRateLimit = FString::Printf(TEXT("t.MaxFPS 0"));
			GEngine->Exec(GetWorld(), *FrameRateLimit);
		}
	else
		{
			FString FrameRateLimit = FString::Printf(TEXT("t.MaxFPS %d"), FCString::Atoi(*Settings.FrameRateLimit.ToString()));
			GEngine->Exec(GetWorld(), *FrameRateLimit);
		}

	// Get the current monitor's resolution (the one the game window is on)
	int32 CurrentMonitorWidth, CurrentMonitorHeight;
	UExtendedMonitorLibrary::GetCurrentMonitorResolution(CurrentMonitorWidth, CurrentMonitorHeight);
	FIntPoint CurrentMonitorResolution(CurrentMonitorWidth, CurrentMonitorHeight);

	// Apply screen resolution
	if (!Settings.ScreenResolution.IsNone())
	{
		FString ResolutionStr = Settings.ScreenResolution.ToString();
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
			EWindowMode::Type WindowMode = EWindowMode::Fullscreen; // Default to fullscreen
			
			if (Settings.DisplayMode == "Windowed")
			{
				WindowMode = EWindowMode::Windowed;
				GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Windowed"));
			}
			else if (Settings.DisplayMode == "Windowed Fullscreen")
			{
				WindowMode = EWindowMode::WindowedFullscreen;
				GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Windowed Fullscreen"));
			}
			else if (Settings.DisplayMode == "Fullscreen")
			{
				WindowMode = EWindowMode::Fullscreen;
			}
			
			// Get the game user settings
			if (GEngine && GEngine->GameUserSettings)
			{
				UGameUserSettings* UserSettings = GEngine->GameUserSettings;
				
				// Set the window mode
				UserSettings->SetFullscreenMode(WindowMode);
				
				// Apply the settings
				UserSettings->ApplyResolutionSettings(false);
			}
		}

		// Apply anti-aliasing mode
		if (!Settings.AntiAliasingMode.IsNone())
		{
			FString AACommand;
			
			if (Settings.AntiAliasingMode == "None")
			{
				AACommand = TEXT("r.DefaultFeature.AntiAliasing 0"); // No anti-aliasing
			}
			else if (Settings.AntiAliasingMode == "FXAA")
			{
				AACommand = TEXT("r.DefaultFeature.AntiAliasing 1"); // FXAA
			}
			else if (Settings.AntiAliasingMode == "TAA")
			{
				AACommand = TEXT("r.DefaultFeature.AntiAliasing 2"); // Temporal AA
			}
			else if (Settings.AntiAliasingMode == "MSAA")
			{
				AACommand = TEXT("r.DefaultFeature.AntiAliasing 3"); // MSAA
			}
			
			if (!AACommand.IsEmpty())
			{
				GEngine->Exec(GetWorld(), *AACommand);
			}
		}
	}

	// Store and save settings
	ExtendedSettings->DisplaySettings = Settings;
	SaveExtendedSettings();
}


void UExtendedSettingsSubsystem::ApplySettings()
{
	ApplyGameplaySettings(TemporaryGameplaySettings);
	ApplyAudioSettings(TemporaryAudioSettings);
	ApplyGraphicsSettings(TemporaryGraphicsSettings);
	ApplyDisplaySettings(TemporaryDisplaySettings);
	SaveExtendedSettings();
}


void UExtendedSettingsSubsystem::RevertSettings()
{
	TemporaryGameplaySettings = ExtendedSettings->GameplaySettings;
	TemporaryAudioSettings = ExtendedSettings->AudioSettings;
	TemporaryGraphicsSettings = ExtendedSettings->GraphicsSettings;
	TemporaryDisplaySettings = ExtendedSettings->DisplaySettings;
}






void UExtendedSettingsSubsystem::OnAudioDevicesChanged(const TArray<FExtendedAudioDeviceInfo>& AudioDevices)
{
    // Update the audio device lists in settings
    UpdateAudioDeviceLists();
}


void UExtendedSettingsSubsystem::OnAudioDeviceDisconnected(const FExtendedAudioDeviceInfo& DisconnectedDevice)
{
    // Update the audio device lists in settings
    UpdateAudioDeviceLists();
    
    // Check if the disconnected device was the currently selected one
    if (!DisconnectedDevice.bIsInputDevice && 
        ExtendedSettings->AudioSettings.AudioOutputDeviceName.ToString() == DisconnectedDevice.DeviceName)
    {
        // Reset to default output device
        FExtendedAudioSettings CurrentSettings = ExtendedSettings->AudioSettings;
        FExtendedAudioDeviceInfo DefaultDevice = ExtendedAudioDeviceManager->GetDefaultOutputDevice();
        CurrentSettings.AudioOutputDeviceName = FName(*DefaultDevice.DeviceName);
        ApplyAudioSettings(CurrentSettings);
    }
    else if (DisconnectedDevice.bIsInputDevice && 
             ExtendedSettings->AudioSettings.AudioInputDeviceName.ToString() == DisconnectedDevice.DeviceName)
    {
        // Reset to default input device
        FExtendedAudioSettings CurrentSettings = ExtendedSettings->AudioSettings;
        FExtendedAudioDeviceInfo DefaultDevice = ExtendedAudioDeviceManager->GetDefaultInputDevice();
        CurrentSettings.AudioInputDeviceName = FName(*DefaultDevice.DeviceName);
        ApplyAudioSettings(CurrentSettings);
    }
}


void UExtendedSettingsSubsystem::UpdateAudioDeviceLists()
{
    if (!ExtendedAudioDeviceManager)
    {
        return;
    }
    
    // Get current audio settings
    FExtendedAudioSettings CurrentSettings = ExtendedSettings->AudioSettings;
    
    // Update output devices list
    TArray<FExtendedAudioDeviceInfo> OutputDevices = ExtendedAudioDeviceManager->GetOutputDevices();
    CurrentSettings.AudioOutputDevices.Empty();
    
    for (const FExtendedAudioDeviceInfo& Device : OutputDevices)
    {
        CurrentSettings.AudioOutputDevices.Add(FName(*Device.DeviceName));
    }
    
    // Update input devices list
    TArray<FExtendedAudioDeviceInfo> InputDevices = ExtendedAudioDeviceManager->GetInputDevices();
    CurrentSettings.AudioInputDevices.Empty();
    
    for (const FExtendedAudioDeviceInfo& Device : InputDevices)
    {
        CurrentSettings.AudioInputDevices.Add(FName(*Device.DeviceName));
    }
    
    // If no output device is selected, set to default
    if (CurrentSettings.AudioOutputDeviceName.IsNone() && OutputDevices.Num() > 0)
    {
        FExtendedAudioDeviceInfo DefaultDevice = ExtendedAudioDeviceManager->GetDefaultOutputDevice();
        CurrentSettings.AudioOutputDeviceName = FName(*DefaultDevice.DeviceName);
    }
    
    // If no input device is selected, set to default
    if (CurrentSettings.AudioInputDeviceName.IsNone() && InputDevices.Num() > 0)
    {
        FExtendedAudioDeviceInfo DefaultDevice = ExtendedAudioDeviceManager->GetDefaultInputDevice();
        CurrentSettings.AudioInputDeviceName = FName(*DefaultDevice.DeviceName);
    }
    
    // Update both temporary and saved settings
    TemporaryAudioSettings = CurrentSettings;
    ExtendedSettings->AudioSettings = CurrentSettings;
    ExtendedSettings->SaveExtendedSettings();
}


void UExtendedSettingsSubsystem::UpdateScreenResolutionList()
{
    // Get current display settings
    FExtendedDisplaySettings CurrentSettings = ExtendedSettings->DisplaySettings;
    
    // Clear existing resolutions
    CurrentSettings.ScreenResolutions.Empty();
    
    // Get the current monitor's resolution (the one the game window is on)
    int32 CurrentMonitorWidth, CurrentMonitorHeight;
    UExtendedMonitorLibrary::GetCurrentMonitorResolution(CurrentMonitorWidth, CurrentMonitorHeight);
    FIntPoint CurrentMonitorResolution(CurrentMonitorWidth, CurrentMonitorHeight);
    
    // Get all supported resolutions for the current monitor
    TArray<FIntPoint> SupportedResolutions = UExtendedMonitorLibrary::GetCurrentMonitorSupportedResolutions();
    
    // Add all supported resolutions to the list
    for (const FIntPoint& Resolution : SupportedResolutions)
    {
        FString ResolutionStr = FString::Printf(TEXT("%dx%d"), Resolution.X, Resolution.Y);
        CurrentSettings.ScreenResolutions.Add(FName(*ResolutionStr));
    }
    
    if (CurrentSettings.ScreenResolutions.Num() > 0)
    {
        // If current resolution is not in the list or not set, find the best match for current monitor
        if (CurrentSettings.ScreenResolution.IsNone() ||
            !CurrentSettings.ScreenResolutions.Contains(CurrentSettings.ScreenResolution))
        {
            // Try to find a resolution that matches the current monitor's native resolution
            FString CurrentMonitorResStr = FString::Printf(TEXT("%dx%d"), CurrentMonitorResolution.X, CurrentMonitorResolution.Y);
            FName CurrentMonitorResFName = FName(*CurrentMonitorResStr);
            
            if (CurrentSettings.ScreenResolutions.Contains(CurrentMonitorResFName))
            {
                // Use the current monitor's native resolution if available
                CurrentSettings.ScreenResolution = CurrentMonitorResFName;
            }
            else
            {
                // Otherwise use the highest available resolution that fits the display
                CurrentSettings.ScreenResolution = CurrentSettings.ScreenResolutions.Last();
            }
        }
    }
    else
    {
        // Handle case where no resolutions are available
        UE_LOG(LogTemp, Warning, TEXT("No supported resolutions found for the current monitor"));
        
        // Add a fallback resolution (1280x720 is generally safe)
        FString FallbackResolution = TEXT("1280x720");
        CurrentSettings.ScreenResolutions.Add(FName(*FallbackResolution));
        CurrentSettings.ScreenResolution = FName(*FallbackResolution);
    }
    
    // Update both temporary and saved settings
    TemporaryDisplaySettings = CurrentSettings;
    ExtendedSettings->DisplaySettings = CurrentSettings;
    ExtendedSettings->SaveExtendedSettings();
}
