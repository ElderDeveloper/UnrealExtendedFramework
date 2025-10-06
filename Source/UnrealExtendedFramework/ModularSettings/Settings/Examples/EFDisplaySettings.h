#pragma once

#include "CoreMinimal.h"
#include "UnrealExtendedFramework/ModularSettings/Settings/EFModularSettingsBase.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "EFDisplaySettings.generated.h"

// Fullscreen Mode Setting
UCLASS(Blueprintable, DisplayName = "Fullscreen Mode")
class UNREALEXTENDEDFRAMEWORK_API UEFFullscreenSetting : public UEFModularSettingsMultiSelect
{
	GENERATED_BODY()
	
public:
	UEFFullscreenSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Display.Fullscreen"));
		DisplayName = NSLOCTEXT("Settings", "FullscreenMode", "Fullscreen Mode");
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
	
	virtual void Apply() override
	{
		if (!Values.IsValidIndex(SelectedIndex))
		{
			return;
		}
		
		FString Mode = Values[SelectedIndex];
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
		
		if (GEngine && GEngine->GameViewport)
		{
			UGameViewportClient* GameViewport = GEngine->GameViewport;
			if (GameViewport->GetWindow().IsValid())
			{
				GameViewport->GetWindow()->SetWindowMode(WindowMode);
				UE_LOG(LogTemp, Log, TEXT("Applied Fullscreen Mode: %s"), *Mode);
			}
		}
	}
	
	// Helper method to get current window mode
	UFUNCTION(BlueprintCallable, Category = "Display Settings")
	FString GetCurrentWindowMode() const
	{
		if (GEngine && GEngine->GameViewport && GEngine->GameViewport->GetWindow().IsValid())
		{
			EWindowMode::Type CurrentMode = GEngine->GameViewport->GetWindow()->GetWindowMode();
			switch (CurrentMode)
			{
				case EWindowMode::Windowed:
					return TEXT("Windowed");
				case EWindowMode::WindowedFullscreen:
					return TEXT("Borderless");
				case EWindowMode::Fullscreen:
					return TEXT("Fullscreen");
				default:
					return TEXT("Windowed");
			}
		}
		return TEXT("Windowed");
	}
};

// Brightness Setting
UCLASS(Blueprintable, DisplayName = "Brightness")
class UNREALEXTENDEDFRAMEWORK_API UEFBrightnessSetting : public UEFModularSettingsFloat
{
	GENERATED_BODY()
	
public:
	UEFBrightnessSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Display.Brightness"));
		DisplayName = NSLOCTEXT("Settings", "Brightness", "Brightness");
		ConfigCategory = TEXT("Display");
		DefaultValue = TEXT("1.0");
		
		Value = 1.0f;
		Min = 0.5f;
		Max = 2.0f;
	}
	
	virtual void Apply() override
	{
		if (GEngine)
		{
			FString Command = FString::Printf(TEXT("r.Tonemapper.Sharpen %f"), Value);
			GEngine->Exec(GEngine->GetWorld(), *Command);
			
			// Also apply gamma correction
			FString GammaCommand = FString::Printf(TEXT("r.Gamma %f"), Value);
			GEngine->Exec(GEngine->GetWorld(), *GammaCommand);
			
			UE_LOG(LogTemp, Log, TEXT("Applied Brightness: %.2f"), Value);
		}
	}
	
	UFUNCTION(BlueprintCallable, Category = "Display Settings")
	float GetCurrentBrightness() const
	{
		return Value;
	}
};

// Contrast Setting
UCLASS(Blueprintable, DisplayName = "Contrast")
class UNREALEXTENDEDFRAMEWORK_API UEFContrastSetting : public UEFModularSettingsFloat
{
	GENERATED_BODY()
	
public:
	UEFContrastSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Display.Contrast"));
		DisplayName = NSLOCTEXT("Settings", "Contrast", "Contrast");
		ConfigCategory = TEXT("Display");
		DefaultValue = TEXT("1.0");
		
		Value = 1.0f;
		Min = 0.5f;
		Max = 2.0f;
	}
	
	virtual void Apply() override
	{
		if (GEngine)
		{
			// Apply contrast through post-process settings
			FString Command = FString::Printf(TEXT("r.Color.Contrast %f"), Value);
			GEngine->Exec(GEngine->GetWorld(), *Command);
			
			UE_LOG(LogTemp, Log, TEXT("Applied Contrast: %.2f"), Value);
		}
	}
	
	UFUNCTION(BlueprintCallable, Category = "Display Settings")
	float GetCurrentContrast() const
	{
		return Value;
	}
};

// Saturation Setting
UCLASS(Blueprintable, DisplayName = "Saturation")
class UNREALEXTENDEDFRAMEWORK_API UEFSaturationSetting : public UEFModularSettingsFloat
{
	GENERATED_BODY()
	
public:
	UEFSaturationSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Display.Saturation"));
		DisplayName = NSLOCTEXT("Settings", "Saturation", "Saturation");
		ConfigCategory = TEXT("Display");
		DefaultValue = TEXT("1.0");
		
		Value = 1.0f;
		Min = 0.0f;
		Max = 2.0f;
	}
	
	virtual void Apply() override
	{
		if (GEngine)
		{
			// Apply saturation through post-process settings
			FString Command = FString::Printf(TEXT("r.Color.Saturation %f"), Value);
			GEngine->Exec(GEngine->GetWorld(), *Command);
			
			UE_LOG(LogTemp, Log, TEXT("Applied Saturation: %.2f"), Value);
		}
	}
	
	UFUNCTION(BlueprintCallable, Category = "Display Settings")
	float GetCurrentSaturation() const
	{
		return Value;
	}
};

// Field of View Setting
UCLASS(Blueprintable, DisplayName = "Field of View")
class UNREALEXTENDEDFRAMEWORK_API UEFFOVSetting : public UEFModularSettingsFloat
{
	GENERATED_BODY()
	
public:
	UEFFOVSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Display.FOV"));
		DisplayName = NSLOCTEXT("Settings", "FOV", "Field of View");
		ConfigCategory = TEXT("Display");
		DefaultValue = TEXT("90.0");
		
		Value = 90.0f;
		Min = 60.0f;
		Max = 120.0f;
	}
	
	virtual void Apply() override
	{
		if (GEngine)
		{
			// Apply FOV to the current player controller
			if (APlayerController* PC = GEngine->GetFirstLocalPlayerController(GEngine->GetCurrentPlayWorld()))
			{
				if (APawn* Pawn = PC->GetPawn())
				{
					// This would need to be adapted based on your camera system
					// For now, we'll use a console command approach
					FString Command = FString::Printf(TEXT("fov %f"), Value);
					GEngine->Exec(GEngine->GetWorld(), *Command);
					
					UE_LOG(LogTemp, Log, TEXT("Applied FOV: %.1f"), Value);
				}
			}
		}
	}
	
	UFUNCTION(BlueprintCallable, Category = "Display Settings")
	float GetCurrentFOV() const
	{
		return Value;
	}
};

// UI Scale Setting
UCLASS(Blueprintable, DisplayName = "UI Scale")
class UNREALEXTENDEDFRAMEWORK_API UEFUIScaleSetting : public UEFModularSettingsFloat
{
	GENERATED_BODY()
	
public:
	UEFUIScaleSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Display.UIScale"));
		DisplayName = NSLOCTEXT("Settings", "UIScale", "UI Scale");
		ConfigCategory = TEXT("Display");
		DefaultValue = TEXT("1.0");
		
		Value = 1.0f;
		Min = 0.5f;
		Max = 2.0f;
	}
	
	virtual void Apply() override
	{
		if (GEngine)
		{
			// Apply UI scaling through DPI settings
			FString Command = FString::Printf(TEXT("r.MobileContentScaleFactor %f"), Value);
			GEngine->Exec(GEngine->GetWorld(), *Command);
			
			UE_LOG(LogTemp, Log, TEXT("Applied UI Scale: %.2f"), Value);
		}
	}
	
	UFUNCTION(BlueprintCallable, Category = "Display Settings")
	float GetCurrentUIScale() const
	{
		return Value;
	}
};

// HDR Setting
UCLASS(Blueprintable, DisplayName = "HDR")
class UNREALEXTENDEDFRAMEWORK_API UEFHDRSetting : public UEFModularSettingsBool
{
	GENERATED_BODY()
	
public:
	UEFHDRSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Display.HDR"));
		DisplayName = NSLOCTEXT("Settings", "HDR", "HDR (High Dynamic Range)");
		ConfigCategory = TEXT("Display");
		DefaultValue = TEXT("false");
		bValue = false;
	}
	
	virtual void Apply() override
	{
		if (GEngine)
		{
			// Enable/disable HDR output
			FString Command = bValue ? TEXT("r.HDR.Display.OutputDevice 1") : TEXT("r.HDR.Display.OutputDevice 0");
			GEngine->Exec(GEngine->GetWorld(), *Command);
			
			UE_LOG(LogTemp, Log, TEXT("Applied HDR: %s"), bValue ? TEXT("Enabled") : TEXT("Disabled"));
		}
	}
	
	UFUNCTION(BlueprintCallable, Category = "Display Settings")
	bool IsHDREnabled() const
	{
		return bValue;
	}
};

// Color Blind Support Setting
UCLASS(Blueprintable, DisplayName = "Color Blind Support")
class UNREALEXTENDEDFRAMEWORK_API UEFColorBlindSetting : public UEFModularSettingsMultiSelect
{
	GENERATED_BODY()
	
public:
	UEFColorBlindSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Display.ColorBlind"));
		DisplayName = NSLOCTEXT("Settings", "ColorBlind", "Color Blind Support");
		ConfigCategory = TEXT("Display");
		DefaultValue = TEXT("None");
		
		Values = { 
			TEXT("None"), 
			TEXT("Protanopia"), 
			TEXT("Deuteranopia"), 
			TEXT("Tritanopia") 
		};
		
		DisplayNames = {
			NSLOCTEXT("Settings", "ColorBlindNone", "None"),
			NSLOCTEXT("Settings", "ColorBlindProtanopia", "Protanopia (Red-Green)"),
			NSLOCTEXT("Settings", "ColorBlindDeuteranopia", "Deuteranopia (Red-Green)"),
			NSLOCTEXT("Settings", "ColorBlindTritanopia", "Tritanopia (Blue-Yellow)")
		};
		
		int32 DefaultIndex = Values.Find(DefaultValue);
		SelectedIndex = DefaultIndex != INDEX_NONE ? DefaultIndex : 0;
	}
	
	virtual void Apply() override
	{
		if (Values.IsValidIndex(SelectedIndex))
		{
			FString ColorBlindType = Values[SelectedIndex];
			
			if (GEngine)
			{
				FString Command;
				if (ColorBlindType == TEXT("None"))
				{
					Command = TEXT("r.Color.Deficiency.Type 0");
				}
				else if (ColorBlindType == TEXT("Protanopia"))
				{
					Command = TEXT("r.Color.Deficiency.Type 1; r.Color.Deficiency.Severity 1.0");
				}
				else if (ColorBlindType == TEXT("Deuteranopia"))
				{
					Command = TEXT("r.Color.Deficiency.Type 2; r.Color.Deficiency.Severity 1.0");
				}
				else if (ColorBlindType == TEXT("Tritanopia"))
				{
					Command = TEXT("r.Color.Deficiency.Type 3; r.Color.Deficiency.Severity 1.0");
				}
				
				GEngine->Exec(GEngine->GetWorld(), *Command);
				UE_LOG(LogTemp, Log, TEXT("Applied Color Blind Support: %s"), *ColorBlindType);
			}
		}
	}
	
	UFUNCTION(BlueprintCallable, Category = "Display Settings")
	FString GetCurrentColorBlindSetting() const
	{
		if (Values.IsValidIndex(SelectedIndex))
		{
			return Values[SelectedIndex];
		}
		return TEXT("None");
	}
};

// Safe Zone Setting
UCLASS(Blueprintable, DisplayName = "Safe Zone")
class UNREALEXTENDEDFRAMEWORK_API UEFSafeZoneSetting : public UEFModularSettingsFloat
{
	GENERATED_BODY()
	
public:
	UEFSafeZoneSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Display.SafeZone"));
		DisplayName = NSLOCTEXT("Settings", "SafeZone", "Safe Zone");
		ConfigCategory = TEXT("Display");
		DefaultValue = TEXT("1.0");
		
		Value = 1.0f;
		Min = 0.8f;
		Max = 1.0f;
	}
	
	virtual void Apply() override
	{
		if (GEngine)
		{
			// Apply safe zone scaling for UI elements
			FString Command = FString::Printf(TEXT("r.SafeZone.Scale %f"), Value);
			GEngine->Exec(GEngine->GetWorld(), *Command);
			
			UE_LOG(LogTemp, Log, TEXT("Applied Safe Zone: %.2f"), Value);
		}
	}
	
	UFUNCTION(BlueprintCallable, Category = "Display Settings")
	float GetCurrentSafeZone() const
	{
		return Value;
	}
};

// Display Mode Setting (for multiple monitors)
UCLASS(Blueprintable, DisplayName = "Display Mode")
class UNREALEXTENDEDFRAMEWORK_API UEFDisplayModeSetting : public UEFModularSettingsMultiSelect
{
	GENERATED_BODY()
	
public:
	UEFDisplayModeSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Display.DisplayMode"));
		DisplayName = NSLOCTEXT("Settings", "DisplayMode", "Display Mode");
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
	
	virtual void Apply() override
	{
		if (Values.IsValidIndex(SelectedIndex))
		{
			FString DisplayMode = Values[SelectedIndex];
			
			// This would need platform-specific implementation
			// For now, we'll just log the intended behavior
			UE_LOG(LogTemp, Log, TEXT("Applied Display Mode: %s"), *DisplayMode);
		}
	}
	
	UFUNCTION(BlueprintCallable, Category = "Display Settings")
	FString GetCurrentDisplayMode() const
	{
		if (Values.IsValidIndex(SelectedIndex))
		{
			return Values[SelectedIndex];
		}
		return TEXT("Primary");
	}
};
