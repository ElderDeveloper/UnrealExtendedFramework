#pragma once

#include "CoreMinimal.h"
#include "UnrealExtendedFramework/ModularSettings/Settings/EFModularSettingsBase.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/GameUserSettings.h"
#include "EFDisplaySettings.generated.h"

// Display Mode Setting (formerly Fullscreen)
UCLASS(Blueprintable,EditInlineNew,  DisplayName = "Extended Display Mode")
class UNREALEXTENDEDFRAMEWORK_API UEFDisplayModeSetting : public UEFModularSettingsMultiSelect
{
	GENERATED_BODY()

public:
	UEFDisplayModeSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Display.DisplayMode"));
		DisplayName = NSLOCTEXT("Settings", "DisplayMode", "Display Mode");
		ConfigCategory = TEXT("Display");
		DefaultValue = TEXT("Windowed");

		Values = {
			TEXT("Windowed"),
			TEXT("Borderless"),
			TEXT("Fullscreen")
		};

		DisplayNames = {
			NSLOCTEXT("Settings", "Windowed", "Windowed"),
			NSLOCTEXT("Settings", "Borderless", "Borderless Window"),
			NSLOCTEXT("Settings", "Fullscreen", "Fullscreen")
		};

		int32 DefaultIndex = Values.Find(DefaultValue);
		SelectedIndex = DefaultIndex != INDEX_NONE ? DefaultIndex : 0;
	}

	virtual void Apply_Implementation() override
	{
		if (!Values.IsValidIndex(SelectedIndex))
		{
			return;
		}

		UWorld* World = GetWorld();
		if (!World || IsRunningDedicatedServer())
		{
			return;
		}

#if WITH_EDITOR
		if (GIsEditor && World->WorldType == EWorldType::PIE)
		{
			return;
		}
#endif

		const FString Mode = Values[SelectedIndex];
		EWindowMode::Type WindowMode = EWindowMode::Windowed;

		if (Mode == TEXT("Windowed"))
		{
			WindowMode = EWindowMode::Windowed;
		}
		else if (Mode == TEXT("Borderless"))
		{
			WindowMode = EWindowMode::WindowedFullscreen;
		}
		else if (Mode == TEXT("Fullscreen"))
		{
			WindowMode = EWindowMode::Fullscreen;
		}

		if (GEngine)
		{
			// Prefer GameUserSettings: persists across runs and applies using UE's platform pipeline.
			if (UGameUserSettings* UserSettings = GEngine->GetGameUserSettings())
			{
				UserSettings->SetFullscreenMode(WindowMode);

				// false = don't check "command line overrides" as authoritative
				UserSettings->ApplySettings(false);

				// Writes to GameUserSettings.ini
				UserSettings->SaveSettings();

				UE_LOG(LogTemp, Log, TEXT("Applied & saved Display Mode via GameUserSettings: %s"), *Mode);
				return;
			}

			// Fallback (non-persistent): only if GameUserSettings isn't available for some reason.
			if (GEngine->GameViewport && GEngine->GameViewport->GetWindow().IsValid())
			{
				GEngine->GameViewport->GetWindow()->SetWindowMode(WindowMode);
				UE_LOG(LogTemp, Warning, TEXT("Applied Display Mode via Slate window (non-persistent): %s"), *Mode);
			}
		}
	}

	UFUNCTION(BlueprintCallable, Category = "Display Settings")
	FString GetCurrentWindowMode() const
	{
		if (GEngine)
		{
			if (UGameUserSettings* UserSettings = GEngine->GetGameUserSettings())
			{
				switch (UserSettings->GetFullscreenMode())
				{
					case EWindowMode::Windowed: return TEXT("Windowed");
					case EWindowMode::WindowedFullscreen: return TEXT("Borderless");
					case EWindowMode::Fullscreen: return TEXT("Fullscreen");
					default: return TEXT("Windowed");
				}
			}

			if (GetWorld() && GEngine->GameViewport && GEngine->GameViewport->GetWindow().IsValid())
			{
				switch (GEngine->GameViewport->GetWindow()->GetWindowMode())
				{
					case EWindowMode::Windowed: return TEXT("Windowed");
					case EWindowMode::WindowedFullscreen: return TEXT("Borderless");
					case EWindowMode::Fullscreen: return TEXT("Fullscreen");
					default: return TEXT("Windowed");
				}
			}
		}

		return TEXT("Windowed");
	}
};

// Display Monitor Setting (formerly Display Mode)
UCLASS(Blueprintable,EditInlineNew,  DisplayName = "Extended Display Monitor")
class UNREALEXTENDEDFRAMEWORK_API UEFDisplayMonitorSetting : public UEFModularSettingsMultiSelect
{
	GENERATED_BODY()
	
public:
	UEFDisplayMonitorSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Display.DisplayMonitor"));
		DisplayName = NSLOCTEXT("Settings", "DisplayMonitor", "Display Monitor");
		ConfigCategory = TEXT("Display");
		DefaultValue = TEXT("Primary");
		
		Values = { 
			TEXT("Primary"), 
			TEXT("Secondary"), 
			TEXT("Extended"),
			TEXT("Mirrored")
		};
		
		DisplayNames = {
			NSLOCTEXT("Settings", "DisplayPrimary", "Primary Monitor"),
			NSLOCTEXT("Settings", "DisplaySecondary", "Secondary Monitor"),
			NSLOCTEXT("Settings", "DisplayExtended", "Extended Display"),
			NSLOCTEXT("Settings", "DisplayMirrored", "Mirrored Display")
		};
		
		int32 DefaultIndex = Values.Find(DefaultValue);
		SelectedIndex = DefaultIndex != INDEX_NONE ? DefaultIndex : 0;
	}
	
	virtual void Apply_Implementation() override
	{
		if (Values.IsValidIndex(SelectedIndex))
		{
			FString MonitorMode = Values[SelectedIndex];
			
			// This would need platform-specific implementation
			// For now, we'll just log the intended behavior
			UE_LOG(LogTemp, Log, TEXT("Applied Display Monitor: %s"), *MonitorMode);
		}
	}
};

// Brightness Setting
UCLASS(Blueprintable,EditInlineNew,  DisplayName = "Extended Brightness")
class UNREALEXTENDEDFRAMEWORK_API UEFBrightnessSetting : public UEFModularSettingsFloat
{
	GENERATED_BODY()
	
public:
	UEFBrightnessSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Display.Brightness"));
		DisplayName = NSLOCTEXT("Settings", "Brightness", "Brightness");
		ConfigCategory = TEXT("Display");
		DefaultValue = 1.0f;
		
		Value = 1.0f;
		Min = 0.5f;
		Max = 2.0f;
	}
	
	virtual void Apply_Implementation() override
	{
		if (GetWorld())
		{
			FString Command = FString::Printf(TEXT("r.Tonemapper.Sharpen %f"), Value);
			GEngine->Exec(GetWorld(), *Command);
			
			// Also apply gamma correction
			FString GammaCommand = FString::Printf(TEXT("r.Gamma %f"), Value);
			GEngine->Exec(GetWorld(), *GammaCommand);
			
			UE_LOG(LogTemp, Log, TEXT("Applied Brightness: %.2f"), Value);
		}
	}
};

// Contrast Setting
UCLASS(Blueprintable,EditInlineNew,  DisplayName = "Extended Contrast")
class UNREALEXTENDEDFRAMEWORK_API UEFContrastSetting : public UEFModularSettingsFloat
{
	GENERATED_BODY()
	
public:
	UEFContrastSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Display.Contrast"));
		DisplayName = NSLOCTEXT("Settings", "Contrast", "Contrast");
		ConfigCategory = TEXT("Display");
		DefaultValue = 1.0f;
		
		Value = 1.0f;
		Min = 0.5f;
		Max = 2.0f;
	}
	
	virtual void Apply_Implementation() override
	{
		if (GetWorld())
		{
			// Apply contrast through post-process settings
			FString Command = FString::Printf(TEXT("r.Color.Contrast %f"), Value);
			GEngine->Exec(GetWorld(), *Command);
			
			UE_LOG(LogTemp, Log, TEXT("Applied Contrast: %.2f"), Value);
		}
	}
};

// Saturation Setting
UCLASS(Blueprintable,EditInlineNew,  DisplayName = "Extended Saturation")
class UNREALEXTENDEDFRAMEWORK_API UEFSaturationSetting : public UEFModularSettingsFloat
{
	GENERATED_BODY()
	
public:
	UEFSaturationSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Display.Saturation"));
		DisplayName = NSLOCTEXT("Settings", "Saturation", "Saturation");
		ConfigCategory = TEXT("Display");
		DefaultValue = 1.0f;
		
		Value = 1.0f;
		Min = 0.0f;
		Max = 2.0f;
	}
	
	virtual void Apply_Implementation() override
	{
		if (GetWorld())
		{
			// Apply saturation through post-process settings
			FString Command = FString::Printf(TEXT("r.Color.Saturation %f"), Value);
			GEngine->Exec(GetWorld(), *Command);
			
			UE_LOG(LogTemp, Log, TEXT("Applied Saturation: %.2f"), Value);
		}
	}
};

// Frame Rate Limit Setting
UCLASS(Blueprintable,EditInlineNew,  DisplayName = "Extended Frame Rate Limit")
class UNREALEXTENDEDFRAMEWORK_API UEFFrameRateLimitSetting : public UEFModularSettingsMultiSelect
{
	GENERATED_BODY()
	
public:
	UEFFrameRateLimitSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Display.FrameRateLimit"));
		DisplayName = NSLOCTEXT("Settings", "FrameRateLimit", "Frame Rate Limit");
		ConfigCategory = TEXT("Display");
		DefaultValue = TEXT("Unlimited");
		
		Values = { 
			TEXT("30"), 
			TEXT("60"), 
			TEXT("120"),
			TEXT("144"),
			TEXT("Unlimited")
		};
		
		DisplayNames = {
			NSLOCTEXT("Settings", "FPS30", "30 FPS"),
			NSLOCTEXT("Settings", "FPS60", "60 FPS"),
			NSLOCTEXT("Settings", "FPS120", "120 FPS"),
			NSLOCTEXT("Settings", "FPS144", "144 FPS"),
			NSLOCTEXT("Settings", "FPSUnlimited", "Unlimited")
		};
		
		int32 DefaultIndex = Values.Find(DefaultValue);
		SelectedIndex = DefaultIndex != INDEX_NONE ? DefaultIndex : 0;
	}
	
	virtual void Apply_Implementation() override
	{
		if (Values.IsValidIndex(SelectedIndex))
		{
			FString FPSValue = Values[SelectedIndex];
			float MaxFPS = 60.0f;
			
			if (FPSValue != TEXT("Unlimited"))
			{
				MaxFPS = FCString::Atof(*FPSValue);
			}
			
			if (UGameUserSettings* UserSettings = GEngine->GetGameUserSettings())
			{
				UserSettings->SetFrameRateLimit(MaxFPS);
				UserSettings->ApplySettings(false);
				UserSettings->SaveSettings();

				UE_LOG(LogTemp, Log, TEXT("Applied & saved Frame Rate Limit via GameUserSettings: %s"), *FPSValue);
			}
		}
	}
};

// Anti-Aliasing Type Setting
UCLASS(Blueprintable,EditInlineNew,  DisplayName = "Extended Anti-Aliasing Type")
class UNREALEXTENDEDFRAMEWORK_API UEFAntiAliasingTypeSetting : public UEFModularSettingsMultiSelect
{
	GENERATED_BODY()
	
public:
	UEFAntiAliasingTypeSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Display.AntiAliasingType"));
		DisplayName = NSLOCTEXT("Settings", "AntiAliasingType", "Anti-Aliasing Method");
		ConfigCategory = TEXT("Display");
		DefaultValue = TEXT("TSR");
		
		// 0: None, 1: FXAA, 2: TAA, 3: MSAA, 4: TSR
		Values = { 
			TEXT("None"), 
			TEXT("FXAA"), 
			TEXT("TAA"),
			TEXT("MSAA"),
			TEXT("TSR")
		};
		
		DisplayNames = {
			NSLOCTEXT("Settings", "AANone", "None"),
			NSLOCTEXT("Settings", "AAFXAA", "FXAA"),
			NSLOCTEXT("Settings", "AATAA", "TAA"),
			NSLOCTEXT("Settings", "AAMSAA", "MSAA"),
			NSLOCTEXT("Settings", "AATSR", "TSR")
		};
		
		int32 DefaultIndex = Values.Find(DefaultValue);
		SelectedIndex = DefaultIndex != INDEX_NONE ? DefaultIndex : 0;
	}
	
	virtual void Apply_Implementation() override
	{
		if (Values.IsValidIndex(SelectedIndex))
		{
			FString AAType = Values[SelectedIndex];
			int32 AAValue = 0;
			
			if (AAType == TEXT("None")) AAValue = 0;
			else if (AAType == TEXT("FXAA")) AAValue = 1;
			else if (AAType == TEXT("TAA")) AAValue = 2;
			else if (AAType == TEXT("MSAA")) AAValue = 3;
			else if (AAType == TEXT("TSR")) AAValue = 4;
			
			if (GetWorld())
			{
				FString Command = FString::Printf(TEXT("r.AntiAliasingMethod %d"), AAValue);
				GEngine->Exec(GetWorld(), *Command);
				UE_LOG(LogTemp, Log, TEXT("Applied Anti-Aliasing Type: %s"), *AAType);
			}
		}
	}
};
