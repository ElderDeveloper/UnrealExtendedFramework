// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealExtendedFramework/ModularSettings/Settings/EFModularSettingsBase.h"
#include "UnrealExtendedFramework/ModularSettings/EFModularSettingsLibrary.h"
#include "UnrealExtendedFramework/Libraries/Monitor/EFMonitorLibrary.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/GameUserSettings.h"
#include "UnrealExtendedFramework/ModularSettings/EFModularSettingsSubsystem.h"
#include "Internationalization/StringTableRegistry.h"
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
		DisplayName = LOCTABLE("ExtendedFramework", "Settings.DisplayMode");
		ConfigCategory = TEXT("Display");
		DefaultValue = TEXT("Windowed");

		Values = {
			TEXT("Windowed"),
			TEXT("Borderless"),
			TEXT("Fullscreen")
		};

		DisplayNames = {
			LOCTABLE("ExtendedFramework", "Settings.Windowed"),
			LOCTABLE("ExtendedFramework", "Settings.Borderless"),
			LOCTABLE("ExtendedFramework", "Settings.Fullscreen")
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
				SyncResolutionSettingToAppliedMode(WindowMode, UserSettings);

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

private:
	FString GetAppliedResolutionString(EWindowMode::Type WindowMode, UGameUserSettings* UserSettings) const
	{
		if (WindowMode == EWindowMode::WindowedFullscreen)
		{
			int32 MonitorWidth = 0;
			int32 MonitorHeight = 0;
			UEFMonitorLibrary::GetCurrentMonitorResolution(MonitorWidth, MonitorHeight);
			if (MonitorWidth > 0 && MonitorHeight > 0)
			{
				return FString::Printf(TEXT("%dx%d"), MonitorWidth, MonitorHeight);
			}
		}

		if (GEngine && GEngine->GameViewport)
		{
			FVector2D ViewportSize;
			GEngine->GameViewport->GetViewportSize(ViewportSize);
			if (ViewportSize.X > 0.0f && ViewportSize.Y > 0.0f)
			{
				return FString::Printf(TEXT("%.0fx%.0f"), ViewportSize.X, ViewportSize.Y);
			}
		}

		if (UserSettings)
		{
			const FIntPoint Resolution = UserSettings->GetScreenResolution();
			if (Resolution.X > 0 && Resolution.Y > 0)
			{
				return FString::Printf(TEXT("%dx%d"), Resolution.X, Resolution.Y);
			}
		}

		return TEXT("");
	}

	void SyncResolutionSettingToAppliedMode(EWindowMode::Type WindowMode, UGameUserSettings* UserSettings)
	{
		UEFModularSettingsMultiSelect* ResolutionSetting = Cast<UEFModularSettingsMultiSelect>(
			UEFModularSettingsLibrary::GetModularSetting(
				this,
				FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.Resolution")),
				EEFSettingsSource::Local));

		if (!ResolutionSetting)
		{
			return;
		}

		ResolutionSetting->RefreshValues();

		const FString AppliedResolution = GetAppliedResolutionString(WindowMode, UserSettings);
		if (AppliedResolution.IsEmpty())
		{
			return;
		}

		ResolutionSetting->SetValueFromString(AppliedResolution);
		ResolutionSetting->SaveCurrentValue();
		ResolutionSetting->ClearDirty();

		if (ModularSettingsSubsystem)
		{
			ModularSettingsSubsystem->OnSettingsChanged.Broadcast(ResolutionSetting);
		}
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
		DisplayName = LOCTABLE("ExtendedFramework", "Settings.DisplayMonitor");
		ConfigCategory = TEXT("Display");
		DefaultValue = TEXT("Primary");
		
		Values = { 
			TEXT("Primary"), 
			TEXT("Secondary"), 
			TEXT("Extended"),
			TEXT("Mirrored")
		};
		
		DisplayNames = {
			LOCTABLE("ExtendedFramework", "Settings.DisplayPrimary"),
			LOCTABLE("ExtendedFramework", "Settings.DisplaySecondary"),
			LOCTABLE("ExtendedFramework", "Settings.DisplayExtended"),
			LOCTABLE("ExtendedFramework", "Settings.DisplayMirrored")
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
		DisplayName = LOCTABLE("ExtendedFramework", "Settings.Brightness");
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
		DisplayName = LOCTABLE("ExtendedFramework", "Settings.Contrast");
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
		DisplayName = LOCTABLE("ExtendedFramework", "Settings.Saturation");
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
		DisplayName = LOCTABLE("ExtendedFramework", "Settings.FrameRateLimit");
		ConfigCategory = TEXT("Display");
		DefaultValue = TEXT("Unlimited");
		
		// 40 and 45 are handheld caps (a 40 Hz or 90 Hz screen); device rules decide where they are offered.
		for (const int32 Cap : GetStandardCaps())
		{
			Values.Add(FString::FromInt(Cap));
			DisplayNames.Add(GetCapDisplayName(Cap));
		}
		Values.Add(TEXT("Unlimited"));
		DisplayNames.Add(LOCTABLE("ExtendedFramework", "Settings.FPSUnlimited"));
		
		int32 DefaultIndex = Values.Find(DefaultValue);
		SelectedIndex = DefaultIndex != INDEX_NONE ? DefaultIndex : 0;
	}

	/**
	 * A container asset saves its own copy of the option list, so caps added to the class later would
	 * never reach it. Missing standard caps are merged in, in ascending order, before "Unlimited".
	 */
	virtual void OnRegistered() override
	{
		Super::OnRegistered();

		const FString Selected = GetValueAsString();
		bool bChanged = false;
		for (const int32 Cap : GetStandardCaps())
		{
			const FString CapValue = FString::FromInt(Cap);
			if (Values.Contains(CapValue))
			{
				continue;
			}

			// Before the first numeric option that is larger, else before the first non-numeric one.
			int32 InsertAt = Values.Num();
			for (int32 Index = 0; Index < Values.Num(); ++Index)
			{
				if (!Values[Index].IsNumeric() || FCString::Atoi(*Values[Index]) > Cap)
				{
					InsertAt = Index;
					break;
				}
			}

			Values.Insert(CapValue, InsertAt);
			if (DisplayNames.Num() >= InsertAt)
			{
				DisplayNames.Insert(GetCapDisplayName(Cap), InsertAt);
			}
			bChanged = true;
		}

		if (bChanged)
		{
			const int32 SelectedAgain = Values.Find(Selected);
			SelectedIndex = SelectedAgain != INDEX_NONE ? SelectedAgain : FMath::Max(0, Values.Find(DefaultValue));
		}
	}

	static const TArray<int32>& GetStandardCaps()
	{
		static const TArray<int32> Caps = { 30, 40, 45, 60, 90, 120, 144 };
		return Caps;
	}

	static FText GetCapDisplayName(int32 Cap)
	{
		switch (Cap)
		{
		case 30: return LOCTABLE("ExtendedFramework", "Settings.FPS30");
		case 40: return LOCTABLE("ExtendedFramework", "Settings.FPS40");
		case 45: return LOCTABLE("ExtendedFramework", "Settings.FPS45");
		case 60: return LOCTABLE("ExtendedFramework", "Settings.FPS60");
		case 90: return LOCTABLE("ExtendedFramework", "Settings.FPS90");
		case 120: return LOCTABLE("ExtendedFramework", "Settings.FPS120");
		case 144: return LOCTABLE("ExtendedFramework", "Settings.FPS144");
		default: return FText::AsNumber(Cap);
		}
	}
	
	virtual void Apply_Implementation() override
	{
		if (Values.IsValidIndex(SelectedIndex))
		{
			FString FPSValue = Values[SelectedIndex];
			float MaxFPS = 0.0f; // 0 = unlimited in UE
			
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
		DisplayName = LOCTABLE("ExtendedFramework", "Settings.AntiAliasingType");
		ConfigCategory = TEXT("Display");
		DefaultValue = TEXT("TSR");
		
		// 0: None, 1: FXAA, 2: TAA, 3: MSAA, 4: TSR, 5: SMAA
		Values = { 
			TEXT("None"), 
			TEXT("FXAA"), 
			TEXT("TAA"),
			TEXT("MSAA"),
			TEXT("TSR"),
			TEXT("SMAA")
		};
		
		DisplayNames = {
			LOCTABLE("ExtendedFramework", "Common.None"),
			LOCTABLE("ExtendedFramework", "Settings.AAFXAA"),
			LOCTABLE("ExtendedFramework", "Settings.AATAA"),
			LOCTABLE("ExtendedFramework", "Settings.AAMSAA"),
			LOCTABLE("ExtendedFramework", "Settings.AATSR"),
			LOCTABLE("ExtendedFramework", "Settings.AASMAA")
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
			else if (AAType == TEXT("SMAA")) AAValue = 5;
			
			if (GetWorld())
			{
				FString Command = FString::Printf(TEXT("r.AntiAliasingMethod %d"), AAValue);
				GEngine->Exec(GetWorld(), *Command);
				UE_LOG(LogTemp, Log, TEXT("Applied Anti-Aliasing Type: %s"), *AAType);
			}
		}
	}
};
