#pragma once

#include "CoreMinimal.h"
#include "UnrealExtendedFramework/ModularSettings/Settings/EFModularSettingsBase.h"
#include "UnrealExtendedFramework/ModularSettings/EFModularSettingsSubsystem.h"
#include "UnrealExtendedFramework/ModularSettings/EFModularSettingsLibrary.h"
#include "UnrealExtendedFramework/Libraries/Monitor/EFMonitorLibrary.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/GameUserSettings.h"
#include "Widgets/SWindow.h"
#include "HAL/IConsoleManager.h"
#include "HAL/ThreadingBase.h"
#include "Internationalization/StringTableRegistry.h"
#include "EFGraphicsSettings.generated.h"

namespace EFGraphicsSettingsInternal
{
	inline int32 GetConsoleIntValue(const TCHAR* Name, int32 DefaultValue)
	{
		if (const IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			return CVar->GetInt();
		}

		return DefaultValue;
	}

	inline float GetConsoleFloatValue(const TCHAR* Name, float DefaultValue)
	{
		if (const IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			return CVar->GetFloat();
		}

		return DefaultValue;
	}

	inline int32 MapMaxAnisotropyToIndex(int32 MaxAnisotropy)
	{
		if (MaxAnisotropy <= 0)
		{
			return 0;
		}
		if (MaxAnisotropy <= 1)
		{
			return 1;
		}
		if (MaxAnisotropy <= 2)
		{
			return 2;
		}
		if (MaxAnisotropy <= 4)
		{
			return 3;
		}
		if (MaxAnisotropy <= 8)
		{
			return 4;
		}

		return 5;
	}

	inline int32 MapSSRQualityToIndex(int32 SSRQuality)
	{
		return FMath::Clamp(SSRQuality, 0, 4);
	}

	/** Sets one scalability group by its console variable name (sg.ShadowQuality). False for an unknown group. */
	inline bool SetScalabilityGroupLevel(UGameUserSettings* UserSettings, FName Group, int32 Level)
	{
		if (!UserSettings)
		{
			return false;
		}

		static const FName ViewDistance(TEXT("sg.ViewDistanceQuality"));
		static const FName AntiAliasing(TEXT("sg.AntiAliasingQuality"));
		static const FName Shadow(TEXT("sg.ShadowQuality"));
		static const FName GlobalIllumination(TEXT("sg.GlobalIlluminationQuality"));
		static const FName Reflection(TEXT("sg.ReflectionQuality"));
		static const FName PostProcess(TEXT("sg.PostProcessQuality"));
		static const FName Texture(TEXT("sg.TextureQuality"));
		static const FName Effects(TEXT("sg.EffectsQuality"));
		static const FName Foliage(TEXT("sg.FoliageQuality"));
		static const FName Shading(TEXT("sg.ShadingQuality"));
		static const FName Landscape(TEXT("sg.LandscapeQuality"));

		if (Group == ViewDistance) { UserSettings->SetViewDistanceQuality(Level); }
		else if (Group == AntiAliasing) { UserSettings->SetAntiAliasingQuality(Level); }
		else if (Group == Shadow) { UserSettings->SetShadowQuality(Level); }
		else if (Group == GlobalIllumination) { UserSettings->SetGlobalIlluminationQuality(Level); }
		else if (Group == Reflection) { UserSettings->SetReflectionQuality(Level); }
		else if (Group == PostProcess) { UserSettings->SetPostProcessingQuality(Level); }
		else if (Group == Texture) { UserSettings->SetTextureQuality(Level); }
		else if (Group == Effects) { UserSettings->SetVisualEffectQuality(Level); }
		else if (Group == Foliage) { UserSettings->SetFoliageQuality(Level); }
		else if (Group == Shading) { UserSettings->SetShadingQuality(Level); }
		else if (Group == Landscape) { UserSettings->SetLandscapeQuality(Level); }
		else { return false; }
		return true;
	}

	inline int32 MapBloomQualityToIndex(int32 BloomQuality)
	{
		if (BloomQuality <= 0)
		{
			return 0;
		}
		if (BloomQuality <= 2)
		{
			return 1;
		}
		if (BloomQuality == 3)
		{
			return 2;
		}
		if (BloomQuality == 4)
		{
			return 3;
		}

		return 4;
	}
}

// Screen Resolution Setting
UCLASS(Blueprintable,EditInlineNew, DisplayName = "Extended Screen Resolution")
class UNREALEXTENDEDFRAMEWORK_API UEFResolutionSetting : public UEFModularSettingsMultiSelect
{
	GENERATED_BODY()

public:
	UEFResolutionSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.Resolution"));
		DisplayName = LOCTABLE("ExtendedFramework", "Settings.Resolution");
		ConfigCategory = TEXT("Graphics");
		DefaultValue = TEXT("1920x1080");
		bRefreshBeforeApply = true;

		Values = { DefaultValue };
		DisplayNames = { FText::FromString(DefaultValue) };
		SelectedIndex = 0;
	}

	virtual void OnRegistered() override
	{
		PopulateResolutionOptions();
	}

	virtual void RefreshValues_Implementation() override
	{
		PopulateResolutionOptions();
	}

	/** The monitor's native resolution, when the list offers it: the sensible start on any device. */
	virtual FString ComputeDeviceDefault() const override
	{
		int32 MonitorWidth = 0;
		int32 MonitorHeight = 0;
		UEFMonitorLibrary::GetCurrentMonitorResolution(MonitorWidth, MonitorHeight);
		if (MonitorWidth <= 0 || MonitorHeight <= 0)
		{
			return FString();
		}

		const FString Native = FString::Printf(TEXT("%dx%d"), MonitorWidth, MonitorHeight);
		return Values.Contains(Native) ? Native : FString();
	}

	virtual void Apply_Implementation() override
	{
		if (!Values.IsValidIndex(SelectedIndex) || !GEngine)
		{
			return;
		}

		const FString Resolution = Values[SelectedIndex];
		int32 Width = 0, Height = 0;

		if (!ParseResolution(Resolution, Width, Height))
		{
			return;
		}

		if (UGameUserSettings* UserSettings = GEngine->GetGameUserSettings())
		{
			UserSettings->SetScreenResolution(FIntPoint(Width, Height));
			UserSettings->ApplySettings(false);
			UserSettings->SaveSettings();

			UE_LOG(LogTemp, Log, TEXT("Applied & saved resolution via GameUserSettings: %dx%d"), Width, Height);
			return;
		}

		// Fallback (non-persistent)
		if (GEngine->GameViewport && GEngine->GameViewport->GetWindow().IsValid())
		{
			GEngine->GameViewport->GetWindow()->Resize(FVector2D(Width, Height));
			UE_LOG(LogTemp, Warning, TEXT("Applied resolution via window resize (non-persistent): %dx%d"), Width, Height);
		}
	}

	// Helper method to get current resolution
	UFUNCTION(BlueprintCallable, Category = "Graphics Settings")
	FString GetCurrentResolution() const
	{
		if (GEngine)
		{
			if (GEngine->GameViewport)
			{
				FVector2D ViewportSize;
				GEngine->GameViewport->GetViewportSize(ViewportSize);
				if (ViewportSize.X > 0.0f && ViewportSize.Y > 0.0f)
				{
					return FString::Printf(TEXT("%.0fx%.0f"), ViewportSize.X, ViewportSize.Y);
				}
			}

			if (UGameUserSettings* UserSettings = GEngine->GetGameUserSettings())
			{
				const FIntPoint Res = UserSettings->GetScreenResolution();
				return FString::Printf(TEXT("%dx%d"), Res.X, Res.Y);
			}
		}
		
		return TEXT("1920x1080");
	}

private:
	void PopulateResolutionOptions()
	{
		if (!IsInGameThread())
		{
			return;
		}

		const FString PreviousResolution = Values.IsValidIndex(SelectedIndex)
			? Values[SelectedIndex]
			: GetCurrentResolution();

		Values.Empty();
		DisplayNames.Empty();

		TArray<FIntPoint> SupportedResolutions = UEFMonitorLibrary::GetCurrentMonitorSupportedResolutions();
		SupportedResolutions.Sort([](const FIntPoint& Left, const FIntPoint& Right)
		{
			const int64 LeftPixels = static_cast<int64>(Left.X) * static_cast<int64>(Left.Y);
			const int64 RightPixels = static_cast<int64>(Right.X) * static_cast<int64>(Right.Y);
			if (LeftPixels == RightPixels)
			{
				return Left.X < Right.X;
			}

			return LeftPixels < RightPixels;
		});

		auto AddResolutionOption = [this](const FIntPoint& Resolution)
		{
			if (Resolution.X <= 0 || Resolution.Y <= 0)
			{
				return;
			}

			const FString ResolutionString = FString::Printf(TEXT("%dx%d"), Resolution.X, Resolution.Y);
			if (!Values.Contains(ResolutionString))
			{
				Values.Add(ResolutionString);
				DisplayNames.Add(FText::FromString(ResolutionString));
			}
		};

		for (const FIntPoint& Resolution : SupportedResolutions)
		{
			AddResolutionOption(Resolution);
		}

		int32 MonitorWidth = 0;
		int32 MonitorHeight = 0;
		UEFMonitorLibrary::GetCurrentMonitorResolution(MonitorWidth, MonitorHeight);
		AddResolutionOption(FIntPoint(MonitorWidth, MonitorHeight));

		int32 CurrentWidth = 0;
		int32 CurrentHeight = 0;
		if (ParseResolution(GetCurrentResolution(), CurrentWidth, CurrentHeight))
		{
			AddResolutionOption(FIntPoint(CurrentWidth, CurrentHeight));
		}

		if (GEngine)
		{
			if (UGameUserSettings* UserSettings = GEngine->GetGameUserSettings())
			{
				const FIntPoint SavedResolution = UserSettings->GetScreenResolution();
				AddResolutionOption(SavedResolution);
			}
		}

		if (Values.Num() == 0)
		{
			const TArray<FIntPoint> CommonResolutions = {
				FIntPoint(1280, 720),
				FIntPoint(1366, 768),
				FIntPoint(1600, 900),
				FIntPoint(1680, 1050),
				FIntPoint(1600, 900),
				FIntPoint(1920, 1080),
				FIntPoint(1920, 1200),
				FIntPoint(2560, 1080),
				FIntPoint(2560, 1440),
				FIntPoint(3440, 1440),
				FIntPoint(3840, 1080),
				FIntPoint(3840, 1600),
				FIntPoint(3840, 2160),
				FIntPoint(5120, 1440),
				FIntPoint(5120, 2160),
				FIntPoint(7680, 2160)
			};

			for (const FIntPoint& Resolution : CommonResolutions)
			{
				if ((MonitorWidth <= 0 || Resolution.X <= MonitorWidth) &&
					(MonitorHeight <= 0 || Resolution.Y <= MonitorHeight))
				{
					AddResolutionOption(Resolution);
				}
			}

			if (Values.Num() == 0 && MonitorWidth > 0 && MonitorHeight > 0)
			{
				AddResolutionOption(FIntPoint(MonitorWidth, MonitorHeight));
			}
		}

		FString PreferredDefaultValue = DefaultValue;
		if (!Values.Contains(PreferredDefaultValue))
		{
			const FString CurrentResolution = GetCurrentResolution();
			if (Values.Contains(CurrentResolution))
			{
				PreferredDefaultValue = CurrentResolution;
			}
			else if (Values.Num() > 0)
			{
				PreferredDefaultValue = Values.Last();
			}
		}
		DefaultValue = PreferredDefaultValue;

		int32 PreferredIndex = Values.Find(PreviousResolution);
		if (PreferredIndex == INDEX_NONE)
		{
			PreferredIndex = Values.Find(DefaultValue);
		}

		SelectedIndex = PreferredIndex != INDEX_NONE ? PreferredIndex : 0;
	}

	bool ParseResolution(const FString& Resolution, int32& Width, int32& Height)
	{
		FString WidthStr, HeightStr;
		if (Resolution.Split(TEXT("x"), &WidthStr, &HeightStr))
		{
			Width = FCString::Atoi(*WidthStr);
			Height = FCString::Atoi(*HeightStr);
			return Width > 0 && Height > 0;
		}
		return false;
	}
};

// VSync Setting
UCLASS(Blueprintable,EditInlineNew,  DisplayName = "Extended VSync")
class UNREALEXTENDEDFRAMEWORK_API UEFVSyncSetting : public UEFModularSettingsBool
{
	GENERATED_BODY()

public:
	UEFVSyncSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.VSync"));
		DisplayName = LOCTABLE("ExtendedFramework", "Settings.VSync");
		ConfigCategory = TEXT("Graphics");
		DefaultValue = true;
		Value = true;
	}

	virtual void Apply_Implementation() override
	{
		if (!GEngine)
		{
			return;
		}

		if (UGameUserSettings* UserSettings = GEngine->GetGameUserSettings())
		{
			UserSettings->SetVSyncEnabled(Value);
			UserSettings->ApplySettings(false);
			UserSettings->SaveSettings();

			UE_LOG(LogTemp, Log, TEXT("Applied & saved VSync via GameUserSettings: %s"), Value ? TEXT("Enabled") : TEXT("Disabled"));
			return;
		}

		// Fallback (non-persistent)
		const FString Command = Value ? TEXT("r.VSync 1") : TEXT("r.VSync 0");
		if (UWorld* World = GetWorld())
		{
			GEngine->Exec(World, *Command);
		}
	}
};

// Anti-Aliasing Quality Setting
UCLASS(Blueprintable,EditInlineNew,  DisplayName = "Extended Anti-Aliasing Quality")
class UNREALEXTENDEDFRAMEWORK_API UEFAntiAliasingQualitySetting : public UEFModularSettingsMultiSelect
{
	GENERATED_BODY()

public:
	UEFAntiAliasingQualitySetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.AntiAliasingQuality"));
		ScalabilityGroup = TEXT("sg.AntiAliasingQuality");
		DisplayName = LOCTABLE("ExtendedFramework", "Settings.AntiAliasingQuality");
		ConfigCategory = TEXT("Graphics");
		DefaultValue = TEXT("High");

		Values = { TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Ultra") };
		DisplayNames = {
			LOCTABLE("ExtendedFramework", "Common.Low"),
			LOCTABLE("ExtendedFramework", "Common.Medium"),
			LOCTABLE("ExtendedFramework", "Common.High"),
			LOCTABLE("ExtendedFramework", "Common.Ultra")
		};

		int32 DefaultIndex = Values.Find(DefaultValue);
		SelectedIndex = DefaultIndex != INDEX_NONE ? DefaultIndex : 2;
	}

	virtual void Apply_Implementation() override
	{
		if (Values.IsValidIndex(SelectedIndex))
		{
			const int32 QualityLevel = FMath::Clamp(SelectedIndex, 0, 3);

			if (GEngine)
			{
				if (UGameUserSettings* UserSettings = GEngine->GetGameUserSettings())
				{
					UserSettings->SetAntiAliasingQuality(QualityLevel);
					UserSettings->ApplySettings(false);
					UserSettings->SaveSettings();

					UE_LOG(LogTemp, Log, TEXT("Applied & saved Anti-Aliasing Quality via GameUserSettings: %s (Level %d)"),*Values[SelectedIndex], QualityLevel);
					return;
				}

				// Fallback (non-persistent)
				if (UWorld* World = GetWorld())
				{
					const FString Command = FString::Printf(TEXT("sg.AntiAliasingQuality %d"), QualityLevel);
					GEngine->Exec(World, *Command);
				}
			}
		}
	}
};

// Texture Quality Setting
UCLASS(Blueprintable,EditInlineNew,  DisplayName = "Extended Texture Quality")
class UNREALEXTENDEDFRAMEWORK_API UEFTextureQualitySetting : public UEFModularSettingsMultiSelect
{
	GENERATED_BODY()

public:
	UEFTextureQualitySetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.TextureQuality"));
		ScalabilityGroup = TEXT("sg.TextureQuality");
		DisplayName = LOCTABLE("ExtendedFramework", "Settings.TextureQuality");
		ConfigCategory = TEXT("Graphics");
		DefaultValue = TEXT("High");

		Values = { TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Ultra") };
		DisplayNames = {
			LOCTABLE("ExtendedFramework", "Common.Low"),
			LOCTABLE("ExtendedFramework", "Common.Medium"),
			LOCTABLE("ExtendedFramework", "Common.High"),
			LOCTABLE("ExtendedFramework", "Common.Ultra")
		};

		int32 DefaultIndex = Values.Find(DefaultValue);
		SelectedIndex = DefaultIndex != INDEX_NONE ? DefaultIndex : 2;
	}

	virtual void Apply_Implementation() override
	{
		if (Values.IsValidIndex(SelectedIndex))
		{
			const int32 QualityLevel = FMath::Clamp(SelectedIndex, 0, 3);

			if (GEngine)
			{
				if (UGameUserSettings* UserSettings = GEngine->GetGameUserSettings())
				{
					UserSettings->SetTextureQuality(QualityLevel);
					UserSettings->ApplySettings(false);
					UserSettings->SaveSettings();

					UE_LOG(LogTemp, Log, TEXT("Applied & saved Texture Quality via GameUserSettings: %s (Level %d)"),*Values[SelectedIndex], QualityLevel);
					return;
				}

				// Fallback (non-persistent)
				if (UWorld* World = GetWorld())
				{
					const FString Command = FString::Printf(TEXT("sg.TextureQuality %d"), QualityLevel);
					GEngine->Exec(World, *Command);
				}
			}
		}
	}
};

// Texture Filtering Quality Setting
UCLASS(Blueprintable,EditInlineNew,  DisplayName = "Extended Texture Filtering Quality")
class UNREALEXTENDEDFRAMEWORK_API UEFTextureFilteringQualitySetting : public UEFModularSettingsMultiSelect
{
	GENERATED_BODY()
	
public:
	UEFTextureFilteringQualitySetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.TextureFilteringQuality"));
		DisplayName = LOCTABLE("ExtendedFramework", "Settings.TextureFilteringQuality");
		ConfigCategory = TEXT("Graphics");
		DefaultValue = TEXT("8x");
		
		Values = { TEXT("Trilinear"), TEXT("1x"), TEXT("2x"), TEXT("4x"), TEXT("8x"), TEXT("16x") };
		DisplayNames = {
			LOCTABLE("ExtendedFramework", "Settings.TextureFilteringTrilinear"),
			LOCTABLE("ExtendedFramework", "Settings.TextureFiltering1x"),
			LOCTABLE("ExtendedFramework", "Settings.TextureFiltering2x"),
			LOCTABLE("ExtendedFramework", "Settings.TextureFiltering4x"),
			LOCTABLE("ExtendedFramework", "Settings.TextureFiltering8x"),
			LOCTABLE("ExtendedFramework", "Settings.TextureFiltering16x")
		};
		
		int32 DefaultIndex = Values.Find(DefaultValue);
		SelectedIndex = DefaultIndex != INDEX_NONE ? DefaultIndex : 4;
	}
	
	virtual void Apply_Implementation() override
	{
		if (Values.IsValidIndex(SelectedIndex))
		{
			// Map our indices to anisotropy levels:
			// Trilinear(0)->0, 1x(1)->1, 2x(2)->2, 4x(3)->4, 8x(4)->8, 16x(5)->16
			int32 Aniso = 0;
			switch (SelectedIndex)
			{
				case 0: Aniso = 0; break; // Trilinear
				case 1: Aniso = 1; break;
				case 2: Aniso = 2; break;
				case 3: Aniso = 4; break;
				case 4: Aniso = 8; break;
				case 5: Aniso = 16; break;
				default: Aniso = 8; break;
			}

			if (GEngine)
			{
				// No direct UGameUserSettings API for texture filtering. Use CVar.
				if (UWorld* World = GetWorld())
				{
					const FString Command = FString::Printf(TEXT("r.MaxAnisotropy %d"), Aniso);
					GEngine->Exec(World, *Command);
					UE_LOG(LogTemp, Log, TEXT("Applied Texture Filtering Quality (Anisotropy %d)"), Aniso);
				}
			}
		}
	}
};

// Foliage Quality Setting
UCLASS(Blueprintable,EditInlineNew,  DisplayName = "Extended Foliage Quality")
class UNREALEXTENDEDFRAMEWORK_API UEFFoliageQualitySetting : public UEFModularSettingsMultiSelect
{
	GENERATED_BODY()
	
public:
	UEFFoliageQualitySetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.FoliageQuality"));
		ScalabilityGroup = TEXT("sg.FoliageQuality");
		DisplayName = LOCTABLE("ExtendedFramework", "Settings.FoliageQuality");
		ConfigCategory = TEXT("Graphics");
		DefaultValue = TEXT("Medium");
		
		Values = { TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Ultra") };
		DisplayNames = {
			LOCTABLE("ExtendedFramework", "Common.Low"),
			LOCTABLE("ExtendedFramework", "Common.Medium"),
			LOCTABLE("ExtendedFramework", "Common.High"),
			LOCTABLE("ExtendedFramework", "Common.Ultra")
		};
		
		int32 DefaultIndex = Values.Find(DefaultValue);
		SelectedIndex = DefaultIndex != INDEX_NONE ? DefaultIndex : 2;
	}
	
	virtual void Apply_Implementation() override
	{
		if (Values.IsValidIndex(SelectedIndex))
		{
			const int32 QualityLevel = FMath::Clamp(SelectedIndex, 0, 3);
			if (GEngine)
			{
				if (UGameUserSettings* UserSettings = GEngine->GetGameUserSettings())
				{
					UserSettings->SetFoliageQuality(QualityLevel);
					UserSettings->ApplySettings(false);
					UserSettings->SaveSettings();
					UE_LOG(LogTemp, Log, TEXT("Applied & saved Foliage Quality via GameUserSettings: %s (Level %d)"), *Values[SelectedIndex], QualityLevel);
					return;
				}
				// Fallback
				if (UWorld* World = GetWorld())
				{
					const FString Command = FString::Printf(TEXT("sg.FoliageQuality %d"), QualityLevel);
					GEngine->Exec(World, *Command);
				}
			}
		}
	}
};

// Shadow Quality Setting
UCLASS(Blueprintable,EditInlineNew,  DisplayName = "Extended Shadow Quality")
class UNREALEXTENDEDFRAMEWORK_API UEFShadowQualitySetting : public UEFModularSettingsMultiSelect
{
	GENERATED_BODY()
	
public:
	UEFShadowQualitySetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.ShadowQuality"));
		ScalabilityGroup = TEXT("sg.ShadowQuality");
		DisplayName = LOCTABLE("ExtendedFramework", "Settings.ShadowQuality");
		ConfigCategory = TEXT("Graphics");
		DefaultValue = TEXT("Medium");
		
		Values = { TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Ultra") };
		DisplayNames = {
			LOCTABLE("ExtendedFramework", "Common.Low"),
			LOCTABLE("ExtendedFramework", "Common.Medium"),
			LOCTABLE("ExtendedFramework", "Common.High"),
			LOCTABLE("ExtendedFramework", "Common.Ultra")
		};
		
		int32 DefaultIndex = Values.Find(DefaultValue);
		SelectedIndex = DefaultIndex != INDEX_NONE ? DefaultIndex : 2;
	}
	
	virtual void Apply_Implementation() override
	{
		if (Values.IsValidIndex(SelectedIndex))
		{
			const int32 QualityLevel = FMath::Clamp(SelectedIndex, 0, 3);
			if (GEngine)
			{
				if (UGameUserSettings* UserSettings = GEngine->GetGameUserSettings())
				{
					UserSettings->SetShadowQuality(QualityLevel);
					UserSettings->ApplySettings(false);
					UserSettings->SaveSettings();
					UE_LOG(LogTemp, Log, TEXT("Applied & saved Shadow Quality via GameUserSettings: %s (Level %d)"), *Values[SelectedIndex], QualityLevel);
					return;
				}
				// Fallback
				if (UWorld* World = GetWorld())
				{
					const FString Command = FString::Printf(TEXT("sg.ShadowQuality %d"), QualityLevel);
					GEngine->Exec(World, *Command);
				}
			}
		}
	}
};

// Shading Quality Setting
UCLASS(Blueprintable,EditInlineNew,  DisplayName = "Extended Shading Quality")
class UNREALEXTENDEDFRAMEWORK_API UEFShadingQualitySetting : public UEFModularSettingsMultiSelect
{
	GENERATED_BODY()
	
public:
	UEFShadingQualitySetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.ShadingQuality"));
		ScalabilityGroup = TEXT("sg.ShadingQuality");
		DisplayName = LOCTABLE("ExtendedFramework", "Settings.ShadingQuality");
		ConfigCategory = TEXT("Graphics");
		DefaultValue = TEXT("Medium");
		
		Values = { TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Ultra") };
		DisplayNames = {
			LOCTABLE("ExtendedFramework", "Common.Low"),
			LOCTABLE("ExtendedFramework", "Common.Medium"),
			LOCTABLE("ExtendedFramework", "Common.High"),
			LOCTABLE("ExtendedFramework", "Common.Ultra")
		};
		
		int32 DefaultIndex = Values.Find(DefaultValue);
		SelectedIndex = DefaultIndex != INDEX_NONE ? DefaultIndex : 2;
	}
	
	virtual void Apply_Implementation() override
	{
		if (Values.IsValidIndex(SelectedIndex))
		{
			const int32 QualityLevel = FMath::Clamp(SelectedIndex, 0, 3);
			if (GEngine)
			{
				if (UGameUserSettings* UserSettings = GEngine->GetGameUserSettings())
				{
					UserSettings->SetShadingQuality(QualityLevel);
					UserSettings->ApplySettings(false);
					UserSettings->SaveSettings();
					UE_LOG(LogTemp, Log, TEXT("Applied & saved Shading Quality via GameUserSettings: %s (Level %d)"), *Values[SelectedIndex], QualityLevel);
					return;
				}
				// Fallback
				if (UWorld* World = GetWorld())
				{
					const FString Command = FString::Printf(TEXT("sg.ShadingQuality %d"), QualityLevel);
					GEngine->Exec(World, *Command);
				}
			}
		}
	}
};

// VFX Quality Setting
UCLASS(Blueprintable,EditInlineNew,  DisplayName = "Extended VFX Quality")
class UNREALEXTENDEDFRAMEWORK_API UEFVFXQualitySetting : public UEFModularSettingsMultiSelect
{
	GENERATED_BODY()
	
public:
	UEFVFXQualitySetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.VFXQuality"));
		ScalabilityGroup = TEXT("sg.EffectsQuality");
		DisplayName = LOCTABLE("ExtendedFramework", "Settings.VFXQuality");
		ConfigCategory = TEXT("Graphics");
		DefaultValue = TEXT("Medium");
		
		Values = { TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Ultra") };
		DisplayNames = {
			LOCTABLE("ExtendedFramework", "Common.Low"),
			LOCTABLE("ExtendedFramework", "Common.Medium"),
			LOCTABLE("ExtendedFramework", "Common.High"),
			LOCTABLE("ExtendedFramework", "Common.Ultra")
		};
		
		int32 DefaultIndex = Values.Find(DefaultValue);
		SelectedIndex = DefaultIndex != INDEX_NONE ? DefaultIndex : 2;
	}
	
	virtual void Apply_Implementation() override
	{
		if (Values.IsValidIndex(SelectedIndex))
		{
			const int32 QualityLevel = FMath::Clamp(SelectedIndex, 0, 3);
			if (GEngine)
			{
				if (UGameUserSettings* UserSettings = GEngine->GetGameUserSettings())
				{
					UserSettings->SetVisualEffectQuality(QualityLevel);
					UserSettings->ApplySettings(false);
					UserSettings->SaveSettings();
					UE_LOG(LogTemp, Log, TEXT("Applied & saved VFX (Effects) Quality via GameUserSettings: %s (Level %d)"), *Values[SelectedIndex], QualityLevel);
					return;
				}
				// Fallback
				if (UWorld* World = GetWorld())
				{
					const FString Command = FString::Printf(TEXT("sg.EffectsQuality %d"), QualityLevel);
					GEngine->Exec(World, *Command);
				}
			}
		}
	}
};

// Post-Processing Quality Setting
UCLASS(Blueprintable,EditInlineNew,  DisplayName = "Extended Post-Processing Quality")
class UNREALEXTENDEDFRAMEWORK_API UEFPostProcessQualitySetting : public UEFModularSettingsMultiSelect
{
	GENERATED_BODY()
	
public:
	UEFPostProcessQualitySetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.PostProcessQuality"));
		ScalabilityGroup = TEXT("sg.PostProcessQuality");
		DisplayName = LOCTABLE("ExtendedFramework", "Settings.PostProcessQuality");
		ConfigCategory = TEXT("Graphics");
		DefaultValue = TEXT("Medium");
		
		Values = { TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Ultra") };
		DisplayNames = {
			LOCTABLE("ExtendedFramework", "Common.Low"),
			LOCTABLE("ExtendedFramework", "Common.Medium"),
			LOCTABLE("ExtendedFramework", "Common.High"),
			LOCTABLE("ExtendedFramework", "Common.Ultra")
		};
		
		int32 DefaultIndex = Values.Find(DefaultValue);
		SelectedIndex = DefaultIndex != INDEX_NONE ? DefaultIndex : 1;
	}
	
	virtual void Apply_Implementation() override
	{
		if (Values.IsValidIndex(SelectedIndex))
		{
			const int32 QualityLevel = FMath::Clamp(SelectedIndex, 0, 3);
			if (GEngine)
			{
				if (UGameUserSettings* UserSettings = GEngine->GetGameUserSettings())
				{
					UserSettings->SetPostProcessingQuality(QualityLevel);
					UserSettings->ApplySettings(false);
					UserSettings->SaveSettings();
					UE_LOG(LogTemp, Log, TEXT("Applied & saved Post-Processing Quality via GameUserSettings: %s (Level %d)"), *Values[SelectedIndex], QualityLevel);
					return;
				}
				// Fallback
				if (UWorld* World = GetWorld())
				{
					const FString Command = FString::Printf(TEXT("sg.PostProcessQuality %d"), QualityLevel);
					GEngine->Exec(World, *Command);
				}
			}
		}
	}
};

// View Distance Setting
UCLASS(Blueprintable,EditInlineNew,  DisplayName = "Extended View Distance")
class UNREALEXTENDEDFRAMEWORK_API UEFViewDistanceSetting : public UEFModularSettingsMultiSelect
{
	GENERATED_BODY()
	
public:
	UEFViewDistanceSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.ViewDistance"));
		ScalabilityGroup = TEXT("sg.ViewDistanceQuality");
		DisplayName = LOCTABLE("ExtendedFramework", "Settings.ViewDistance");
		ConfigCategory = TEXT("Graphics");
		DefaultValue = TEXT("Medium");
		
		Values = { TEXT("Near"), TEXT("Medium"), TEXT("Far"), TEXT("Ultra") };
		DisplayNames = {
			LOCTABLE("ExtendedFramework", "Settings.ViewDistanceNear"),
			LOCTABLE("ExtendedFramework", "Common.Medium"),
			LOCTABLE("ExtendedFramework", "Settings.ViewDistanceFar"),
			LOCTABLE("ExtendedFramework", "Common.Ultra")
		};
		
		int32 DefaultIndex = Values.Find(DefaultValue);
		SelectedIndex = DefaultIndex != INDEX_NONE ? DefaultIndex : 1;
	}
	
	virtual void Apply_Implementation() override
	{
		if (Values.IsValidIndex(SelectedIndex))
		{
			const int32 QualityLevel = FMath::Clamp(SelectedIndex, 0, 3);
			if (GEngine)
			{
				if (UGameUserSettings* UserSettings = GEngine->GetGameUserSettings())
				{
					UserSettings->SetViewDistanceQuality(QualityLevel);
					UserSettings->ApplySettings(false);
					UserSettings->SaveSettings();
					UE_LOG(LogTemp, Log, TEXT("Applied & saved View Distance Quality via GameUserSettings: %s (Level %d)"), *Values[SelectedIndex], QualityLevel);
					return;
				}
				// Fallback
				if (UWorld* World = GetWorld())
				{
					const FString Command = FString::Printf(TEXT("sg.ViewDistanceQuality %d"), QualityLevel);
					GEngine->Exec(World, *Command);
				}
			}
		}
	}
};

// Screen Space Reflections Setting
UCLASS(Blueprintable,EditInlineNew,  DisplayName = "Extended Screen Space Reflections")
class UNREALEXTENDEDFRAMEWORK_API UEFScreenSpaceReflectionSetting : public UEFModularSettingsMultiSelect
{
	GENERATED_BODY()
	
public:
	UEFScreenSpaceReflectionSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.ScreenSpaceReflection"));
		DisplayName = LOCTABLE("ExtendedFramework", "Settings.ScreenSpaceReflection");
		ConfigCategory = TEXT("Graphics");
		DefaultValue = TEXT("Medium");
		
		Values = { TEXT("Off"), TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Ultra") };
		DisplayNames = {
			LOCTABLE("ExtendedFramework", "Common.Off"),
			LOCTABLE("ExtendedFramework", "Common.Low"),
			LOCTABLE("ExtendedFramework", "Common.Medium"),
			LOCTABLE("ExtendedFramework", "Common.High"),
			LOCTABLE("ExtendedFramework", "Common.Ultra")
		};
		
		int32 DefaultIndex = Values.Find(DefaultValue);
		SelectedIndex = DefaultIndex != INDEX_NONE ? DefaultIndex : 2;
	}
	
	virtual void Apply_Implementation() override
	{
		if (Values.IsValidIndex(SelectedIndex))
		{
			const int32 QualityLevel = FMath::Clamp(SelectedIndex, 0, 4); // 0..4 for SSR CVar
			if (GEngine)
			{
				// There is no dedicated UGameUserSettings setter for SSR in UE5.6; keep CVar path.
				if (UWorld* World = GetWorld())
				{
					const FString Command = FString::Printf(TEXT("r.SSR.Quality %d"), QualityLevel);
					GEngine->Exec(World, *Command);
					UE_LOG(LogTemp, Log, TEXT("Applied Screen Space Reflections Quality: %s (Level %d)"), *Values[SelectedIndex], QualityLevel);
				}
			}
		}
	}
};

// Bloom Setting
UCLASS(Blueprintable,EditInlineNew,  DisplayName = "Extended Bloom")
class UNREALEXTENDEDFRAMEWORK_API UEFBloomSetting : public UEFModularSettingsMultiSelect
{
	GENERATED_BODY()
	
public:
	UEFBloomSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.Bloom"));
		DisplayName = LOCTABLE("ExtendedFramework", "Settings.Bloom");
		ConfigCategory = TEXT("Graphics");
		DefaultValue = TEXT("Medium");
		
		Values = { TEXT("Off"), TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Ultra") };
		DisplayNames = {
			LOCTABLE("ExtendedFramework", "Common.Off"),
			LOCTABLE("ExtendedFramework", "Common.Low"),
			LOCTABLE("ExtendedFramework", "Common.Medium"),
			LOCTABLE("ExtendedFramework", "Common.High"),
			LOCTABLE("ExtendedFramework", "Common.Ultra")
		};
		
		int32 DefaultIndex = Values.Find(DefaultValue);
		SelectedIndex = DefaultIndex != INDEX_NONE ? DefaultIndex : 2;
	}
	
	virtual void Apply_Implementation() override
	{
		if (Values.IsValidIndex(SelectedIndex))
		{
			FString Quality = Values[SelectedIndex];
			int32 QualityLevel = SelectedIndex;
			
			if (GEngine)
			{
				// r.BloomQuality: 0=Off, 1-5=Quality
				FString Command = FString::Printf(TEXT("r.BloomQuality %d"), QualityLevel == 0 ? 0 : QualityLevel + 1);
				GEngine->Exec(GEngine->GetWorld(), *Command);
				UE_LOG(LogTemp, Log, TEXT("Applied Bloom: %s"), *Quality);
			}
		}
	}
};

// Motion Blur Amount Setting
UCLASS(Blueprintable,EditInlineNew,  DisplayName = "Extended Motion Blur Amount")
class UNREALEXTENDEDFRAMEWORK_API UEFMotionBlurAmountSetting : public UEFModularSettingsFloat
{
	GENERATED_BODY()
	
public:
	UEFMotionBlurAmountSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.MotionBlurAmount"));
		DisplayName = LOCTABLE("ExtendedFramework", "Settings.MotionBlurAmount");
		ConfigCategory = TEXT("Graphics");
		DefaultValue = 0.5f;
		
		Value = 0.5f;
		Min = 0.0f;
		Max = 1.0f;
	}
	
	virtual void Apply_Implementation() override
	{
		if (GEngine)
		{
			// r.MotionBlur.Amount ranges from 0.0 to 1.0
			FString Command = FString::Printf(TEXT("r.MotionBlur.Amount %f"), Value);
			GEngine->Exec(GEngine->GetWorld(), *Command);
			UE_LOG(LogTemp, Log, TEXT("Applied Motion Blur Amount: %.2f"), Value);
		}
	}
};

// Enhanced Graphics Quality Setting with Scalability Integration
UCLASS(Blueprintable,EditInlineNew, DisplayName = "Extended Overall Graphics Quality")
class UNREALEXTENDEDFRAMEWORK_API UEFOverallQualitySetting : public UEFModularSettingsMultiSelect
{
	GENERATED_BODY()
	
public:
	UEFOverallQualitySetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.OverallQuality"));
		DisplayName = LOCTABLE("ExtendedFramework", "Settings.OverallGraphicsQuality");
		ConfigCategory = TEXT("Graphics");
		DefaultValue = TEXT("Medium");
		
		Values = { TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Ultra"), TEXT("Custom") };
		DisplayNames = {
			LOCTABLE("ExtendedFramework", "Common.Low"),
			LOCTABLE("ExtendedFramework", "Common.Medium"),
			LOCTABLE("ExtendedFramework", "Common.High"),
			LOCTABLE("ExtendedFramework", "Common.Ultra"),
			LOCTABLE("ExtendedFramework", "Settings.OverallQualityCustom")
		};
		
		int32 DefaultIndex = Values.Find(DefaultValue);
		SelectedIndex = DefaultIndex != INDEX_NONE ? DefaultIndex : 1;
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	bool bUpdateSettingsImmediately = false;

	virtual void SetValueFromString(const FString& Value) override
	{
		const int32 CustomIndex = GetCustomQualityIndex();
		if (CustomIndex != INDEX_NONE && Value.Equals(Values[CustomIndex], ESearchCase::IgnoreCase))
		{
			SetSelectedIndexDirect(CustomIndex);
			return;
		}

		Super::SetValueFromString(Value);
	}

	void SyncFromCurrentEngineState()
	{
		RefreshSubSettingsFromEngine();

		for (int32 PresetIndex = 0; PresetIndex < 4; ++PresetIndex)
		{
			if (MatchesPreset(PresetIndex))
			{
				CaptureResolvedPresetSnapshotFromEngine(PresetIndex);
				SetSelectedIndexDirect(PresetIndex);
				return;
			}
		}

		SetSelectedIndexDirect(GetCustomQualityIndex());
	}
	
	virtual void Apply_Implementation() override
	{
		// Apply overall scalability only for presets Low..Ultra; skip for Custom
		if (!GEngine || SelectedIndex < 0)
		{
			return;
		}

		if (SelectedIndex <= 3)
		{
			if (UGameUserSettings* UserSettings = GEngine->GetGameUserSettings())
			{
				UserSettings->SetOverallScalabilityLevel(SelectedIndex);

				// A device that hides an option of a group (a Steam Deck without Low textures) means "this
				// preset with that group on its nearest offered option". SetOverallScalabilityLevel alone
				// would put the hidden level back, so every group whose adjusted level differs is set again
				// before anything is applied.
				const int32 AdjustedGroups = ApplyAdjustedGroupLevels(UserSettings, SelectedIndex);

				UserSettings->ApplySettings(false);
				UserSettings->SaveSettings();
				CaptureResolvedPresetSnapshotFromEngine(SelectedIndex);
				UE_LOG(LogTemp, Log, TEXT("Applied & saved Overall Scalability Level via GameUserSettings: %s (Level %d, %d group(s) adjusted to this device)"), *Values[SelectedIndex], SelectedIndex, AdjustedGroups);
			}

			// Sync modular sub-setting objects to match the engine state.
			// Without this, the UI keeps showing stale values and saving
			// those stale values later overwrites the engine state.
			// Skipped while the subsystem applies every setting in order: the
			// sub-settings then apply their own (saved or default) values next.
			if (!bSuppressSubSettingSync)
			{
				RefreshSubSettingsFromEngine();
			}
		}
	}

	/** Runs first: its preset writes the individual quality settings, which then apply their own values. */
	virtual int32 GetApplyPriority() const override { return -100; }

	/** "Custom" is locked against the player, so a Custom default is assigned directly. */
	virtual void ResetToDefault() override
	{
		const int32 CustomIndex = GetCustomQualityIndex();
		if (CustomIndex != INDEX_NONE && DefaultValue.Equals(Values[CustomIndex], ESearchCase::IgnoreCase))
		{
			SetSelectedIndexDirect(CustomIndex);
			return;
		}
		Super::ResetToDefault();
	}

	/** Set by the subsystem around its ordered startup apply; see Apply_Implementation. */
	void SetSubSettingSyncSuppressed(bool bSuppressed) { bSuppressSubSettingSync = bSuppressed; }

	/** The preset index (0 Low .. 3 Ultra) of an option id, or INDEX_NONE for Custom or an unknown id. */
	int32 FindPresetIndex(const FString& PresetValue) const
	{
		for (int32 PresetIndex = 0; PresetIndex < 4 && PresetIndex < Values.Num(); ++PresetIndex)
		{
			if (Values[PresetIndex].Equals(PresetValue, ESearchCase::IgnoreCase))
			{
				return PresetIndex;
			}
		}
		return INDEX_NONE;
	}

	/**
	 * The preset whose (device-adjusted) values equal every registered sub-setting's default, or Custom.
	 * This is Overall Quality's own device default unless a device rule names one.
	 */
	FString DeriveDefaultFromSubSettings() const
	{
		for (int32 PresetIndex = 0; PresetIndex < 4 && PresetIndex < Values.Num(); ++PresetIndex)
		{
			bool bMatches = true;
			for (const FGameplayTag& Tag : GetGraphicsSettingTags())
			{
				const UEFModularSettingsMultiSelect* Setting = Cast<UEFModularSettingsMultiSelect>(
					UEFModularSettingsLibrary::GetModularSetting(this, Tag, EEFSettingsSource::Local));
				if (!Setting)
				{
					continue;
				}

				const int32 Expected = GetExpectedSubSettingIndex(Tag, PresetIndex);
				if (!Setting->Values.IsValidIndex(Expected) || !Setting->Values[Expected].Equals(Setting->DefaultValue, ESearchCase::IgnoreCase))
				{
					bMatches = false;
					break;
				}
			}
			if (bMatches)
			{
				return Values[PresetIndex];
			}
		}

		const int32 CustomIndex = GetCustomQualityIndex();
		return CustomIndex != INDEX_NONE ? Values[CustomIndex] : DefaultValue;
	}

	// Centralised list of all sub-settings managed by the overall preset.
	static const TArray<FGameplayTag>& GetGraphicsSettingTags()
	{
		static const TArray<FGameplayTag> Tags = {
			FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.AntiAliasingQuality")),
			FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.TextureQuality")),
			FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.TextureFilteringQuality")),
			FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.FoliageQuality")),
			FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.ShadowQuality")),
			FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.ShadingQuality")),
			FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.VFXQuality")),
			FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.PostProcessQuality")),
			FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.ViewDistance")),
			FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.ScreenSpaceReflection")),
			FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.Bloom"))
		};
		return Tags;
	}

	// Maps a preset level (0=Low..3=Ultra) to the expected sub-setting index,
	// accounting for settings with non-standard option layouts, and for this
	// device: an option the device hides becomes its nearest offered one, and a
	// setting the device fixes (Locked/Hidden) is expected to stay where it is.
	int32 GetExpectedSubSettingIndex(const FGameplayTag& Tag, int32 PresetIndex) const
	{
		const int32 RawIndex = GetRawExpectedSubSettingIndex(Tag, PresetIndex);

		if (const UEFModularSettingsMultiSelect* Setting = Cast<UEFModularSettingsMultiSelect>(
			UEFModularSettingsLibrary::GetModularSetting(this, Tag, EEFSettingsSource::Local)))
		{
			if (!Setting->IsEditableOnThisDevice())
			{
				const int32 FixedIndex = Setting->Values.Find(Setting->DefaultValue);
				return FixedIndex != INDEX_NONE ? FixedIndex : Setting->SelectedIndex;
			}

			const int32 Nearest = Setting->FindNearestOfferedIndex(RawIndex);
			if (Nearest != INDEX_NONE)
			{
				return Nearest;
			}
		}

		return RawIndex;
	}
	
	virtual void SetSelectedIndex_Implementation(int32 Index) override
	{
		Super::SetSelectedIndex_Implementation(Index);

		if (Index >= Values.Num())
		{
			return;
		}

		if (Index == Values.Num() - 1) // Custom option
		{
			return;
		}
		
		// Update individual settings based on quality level
		bIsUpdating = true;
		for (const FGameplayTag& Tag : GetGraphicsSettingTags())
		{
			const int32 TargetIndex = GetExpectedSubSettingIndex(Tag, Index);
			UEFModularSettingsLibrary::SetModularSelectedIndex(this, Tag, TargetIndex, EEFSettingsSource::Local, nullptr, bUpdateSettingsImmediately);
		}
		SyncResolutionScaleForPresetPreview(Index);
		bIsUpdating = false;
	}
	
	virtual void OnRegistered() override
	{
		if (ModularSettingsSubsystem)
		{
			ModularSettingsSubsystem->OnSettingsChanged.AddDynamic(this, &UEFOverallQualitySetting::OnSettingChanged);
		}

		// Lock "Custom" so the stepper UI can't navigate to it.
		// The system sets it directly via field assignment when needed.
		SetOptionLocked(TEXT("Custom"), true);
	}

	UFUNCTION()
	void OnSettingChanged(UEFModularSettingsBase* ChangedSetting)
	{
		if (bIsUpdating || !ChangedSetting) return;

		if (GetGraphicsSettingTags().Contains(ChangedSetting->SettingTag))
		{
			// Check if we match any preset
			int32 MatchedPreset = -1;
			
			// Check Low(0), Medium(1), High(2), Ultra(3)
			for (int32 i = 0; i < 4; ++i)
			{
				if (MatchesPreset(i))
				{
					MatchedPreset = i;
					break;
				}
			}

			bIsUpdating = true;
			if (MatchedPreset != -1)
			{
				if (SelectedIndex != MatchedPreset)
				{
					SetSelectedIndex(MatchedPreset);
					
					if (ModularSettingsSubsystem)
					{
						ModularSettingsSubsystem->OnSettingsChanged.Broadcast(this);
					}
				}
			}
			else
			{
				// Custom — assign directly to bypass the lock check
				if (SelectedIndex != 4)
				{
					SelectedIndex = 4;
					MarkDirty();
					if (ModularSettingsSubsystem)
					{
						ModularSettingsSubsystem->OnSettingsChanged.Broadcast(this);
					}
				}
			}
			bIsUpdating = false;
		}
	}
	
private:
	int32 GetCustomQualityIndex() const
	{
		return Values.Num() > 0 ? Values.Num() - 1 : INDEX_NONE;
	}

	void SetSelectedIndexDirect(int32 Index)
	{
		if (!Values.IsValidIndex(Index))
		{
			return;
		}

		if (SelectedIndex != Index)
		{
			SelectedIndex = Index;
			MarkDirty();
		}
	}

	struct FResolvedPresetSnapshot
	{
		TMap<FGameplayTag, int32> ResolvedIndices;
		bool bHasResolutionScale = false;
		float ResolutionScale = 100.0f;
	};

	bool bIsUpdating = false;
	bool bSuppressSubSettingSync = false;
	TMap<int32, FResolvedPresetSnapshot> ResolvedPresetSnapshots;

	/**
	 * Sets every scalability group whose device-adjusted level for the preset differs from the preset
	 * itself, through the per-group setters. Returns how many groups were adjusted.
	 */
	int32 ApplyAdjustedGroupLevels(UGameUserSettings* UserSettings, int32 PresetIndex) const
	{
		int32 Adjusted = 0;
		for (const FGameplayTag& Tag : GetGraphicsSettingTags())
		{
			const UEFModularSettingsBase* Setting = UEFModularSettingsLibrary::GetModularSetting(this, Tag, EEFSettingsSource::Local);
			const FName Group = Setting ? Setting->GetEffectiveScalabilityGroup() : NAME_None;
			if (Group.IsNone())
			{
				continue;
			}

			// Quality groups use the option index as the engine level.
			const int32 Level = FMath::Clamp(GetExpectedSubSettingIndex(Tag, PresetIndex), 0, 3);
			if (Level == PresetIndex)
			{
				continue;
			}

			if (EFGraphicsSettingsInternal::SetScalabilityGroupLevel(UserSettings, Group, Level))
			{
				++Adjusted;
			}
		}
		return Adjusted;
	}

	static FGameplayTag GetTextureFilteringTag()
	{
		return FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.TextureFilteringQuality"));
	}

	static FGameplayTag GetScreenSpaceReflectionTag()
	{
		return FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.ScreenSpaceReflection"));
	}

	static FGameplayTag GetBloomTag()
	{
		return FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.Bloom"));
	}

	static FGameplayTag GetResolutionScaleTag()
	{
		return FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.Upscaler.ResolutionScale"));
	}

	static FGameplayTag GetUpscalerTag()
	{
		return FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.Upscaler"));
	}

	static FGameplayTag GetDLSSRayReconstructionTag()
	{
		return FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.Upscaler.DLSS.RayReconstruction"));
	}

	bool ShouldSyncResolutionScaleFromEngine() const
	{
		const UEFModularSettingsMultiSelect* UpscalerSetting = Cast<UEFModularSettingsMultiSelect>(
			UEFModularSettingsLibrary::GetModularSetting(this, GetUpscalerTag(), EEFSettingsSource::Local));

		if (!UpscalerSetting)
		{
			return true;
		}

		const FString ActiveUpscaler = UpscalerSetting->GetValueAsString();
		if (ActiveUpscaler == TEXT("FSR") || ActiveUpscaler == TEXT("XeSS"))
		{
			return false;
		}

		if (ActiveUpscaler == TEXT("DLSS"))
		{
			const UEFModularSettingsBool* DLSSRayReconstructionSetting = Cast<UEFModularSettingsBool>(
				UEFModularSettingsLibrary::GetModularSetting(this, GetDLSSRayReconstructionTag(), EEFSettingsSource::Local));
			return !DLSSRayReconstructionSetting || !DLSSRayReconstructionSetting->Value;
		}

		return true;
	}

	int32 ResolveCurrentEngineIndexForTag(const FGameplayTag& Tag) const
	{
		if (Tag == GetTextureFilteringTag())
		{
			return EFGraphicsSettingsInternal::MapMaxAnisotropyToIndex(
				EFGraphicsSettingsInternal::GetConsoleIntValue(TEXT("r.MaxAnisotropy"), 0));
		}

		if (Tag == GetScreenSpaceReflectionTag())
		{
			return EFGraphicsSettingsInternal::MapSSRQualityToIndex(
				EFGraphicsSettingsInternal::GetConsoleIntValue(TEXT("r.SSR.Quality"), 0));
		}

		if (Tag == GetBloomTag())
		{
			return EFGraphicsSettingsInternal::MapBloomQualityToIndex(
				EFGraphicsSettingsInternal::GetConsoleIntValue(TEXT("r.BloomQuality"), 0));
		}

		return INDEX_NONE;
	}

	void CaptureResolvedPresetSnapshotFromEngine(int32 PresetIndex)
	{
		if (PresetIndex < 0 || PresetIndex > 3)
		{
			return;
		}

		FResolvedPresetSnapshot& Snapshot = ResolvedPresetSnapshots.FindOrAdd(PresetIndex);
		Snapshot.ResolvedIndices.Reset();
		Snapshot.ResolvedIndices.Add(GetTextureFilteringTag(), ResolveCurrentEngineIndexForTag(GetTextureFilteringTag()));
		Snapshot.ResolvedIndices.Add(GetScreenSpaceReflectionTag(), ResolveCurrentEngineIndexForTag(GetScreenSpaceReflectionTag()));
		Snapshot.ResolvedIndices.Add(GetBloomTag(), ResolveCurrentEngineIndexForTag(GetBloomTag()));

		Snapshot.bHasResolutionScale = false;
		if (ShouldSyncResolutionScaleFromEngine())
		{
			if (const UEFModularSettingsFloat* ResolutionScaleSetting = Cast<UEFModularSettingsFloat>(
				UEFModularSettingsLibrary::GetModularSetting(this, GetResolutionScaleTag(), EEFSettingsSource::Local)))
			{
				Snapshot.ResolutionScale = FMath::Clamp(
					EFGraphicsSettingsInternal::GetConsoleFloatValue(TEXT("r.ScreenPercentage"), ResolutionScaleSetting->Value),
					ResolutionScaleSetting->Min,
					ResolutionScaleSetting->Max);
				Snapshot.bHasResolutionScale = true;
			}
		}
	}

	const FResolvedPresetSnapshot* GetResolvedPresetSnapshot(int32 PresetIndex) const
	{
		return ResolvedPresetSnapshots.Find(PresetIndex);
	}

	// The preset's index for a sub-setting before this device's mask is applied.
	int32 GetRawExpectedSubSettingIndex(const FGameplayTag& Tag, int32 PresetIndex) const
	{
		if (const FResolvedPresetSnapshot* Snapshot = GetResolvedPresetSnapshot(PresetIndex))
		{
			if (const int32* ResolvedIndex = Snapshot->ResolvedIndices.Find(Tag))
			{
				return *ResolvedIndex;
			}
		}

		// SSR and Bloom have an extra "Off" at index 0, so Low=1, Med=2, High=3, Ultra=4
		if (Tag == GetScreenSpaceReflectionTag() || Tag == GetBloomTag())
		{
			return PresetIndex + 1;
		}

		// Texture Filtering: Trilinear(0), 1x(1), 2x(2), 4x(3), 8x(4), 16x(5)
		if (Tag == GetTextureFilteringTag())
		{
			switch (PresetIndex)
			{
				case 0: return 1;  // Low  -> 1x
				case 1: return 3;  // Med  -> 4x
				case 2: return 4;  // High -> 8x
				case 3: return 5;  // Ultra-> 16x
				default: return 4;
			}
		}

		// Standard 4-option settings (Low=0, Med=1, High=2, Ultra=3)
		return PresetIndex;
	}

	void SyncResolutionScaleFromEngine()
	{
		if (!ShouldSyncResolutionScaleFromEngine())
		{
			return;
		}

		if (UEFModularSettingsFloat* ResolutionScaleSetting = Cast<UEFModularSettingsFloat>(
			UEFModularSettingsLibrary::GetModularSetting(this, GetResolutionScaleTag(), EEFSettingsSource::Local)))
		{
			const float ScreenPercentage = FMath::Clamp(
				EFGraphicsSettingsInternal::GetConsoleFloatValue(TEXT("r.ScreenPercentage"), ResolutionScaleSetting->Value),
				ResolutionScaleSetting->Min,
				ResolutionScaleSetting->Max);
			UEFModularSettingsLibrary::SetModularFloat(this,
				GetResolutionScaleTag(),
				ScreenPercentage,
				EEFSettingsSource::Local,
				nullptr,
				false);
		}
	}

	void SyncResolutionScaleForPresetPreview(int32 PresetIndex)
	{
		if (!ShouldSyncResolutionScaleFromEngine())
		{
			return;
		}

		if (const FResolvedPresetSnapshot* Snapshot = GetResolvedPresetSnapshot(PresetIndex))
		{
			if (Snapshot->bHasResolutionScale)
			{
				UEFModularSettingsLibrary::SetModularFloat(this,
					GetResolutionScaleTag(),
					Snapshot->ResolutionScale,
					EEFSettingsSource::Local,
					nullptr,
					bUpdateSettingsImmediately);
			}
		}
	}

	// After the engine's scalability level is set, read back the engine state
	// and update every modular sub-setting object so the UI stays in sync.
	void RefreshSubSettingsFromEngine()
	{
		if (!GEngine) return;
		UGameUserSettings* UserSettings = GEngine->GetGameUserSettings();
		if (!UserSettings) return;

		// Map engine scalability getters to our setting tags.
		// The engine returns 0-3 for each category after SetOverallScalabilityLevel.
		struct FEngineSettingMapping
		{
			FGameplayTag Tag;
			int32 EngineLevel;
		};

		TArray<FEngineSettingMapping> Mappings = {
			{ FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.AntiAliasingQuality")), UserSettings->GetAntiAliasingQuality() },
			{ FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.TextureQuality")), UserSettings->GetTextureQuality() },
			{ FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.FoliageQuality")), UserSettings->GetFoliageQuality() },
			{ FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.ShadowQuality")), UserSettings->GetShadowQuality() },
			{ FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.ShadingQuality")), UserSettings->GetShadingQuality() },
			{ FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.VFXQuality")), UserSettings->GetVisualEffectQuality() },
			{ FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.PostProcessQuality")), UserSettings->GetPostProcessingQuality() },
			{ FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.ViewDistance")), UserSettings->GetViewDistanceQuality() },
		};

		bIsUpdating = true;

		for (const FEngineSettingMapping& Mapping : Mappings)
		{
			// Engine level maps 1:1 to our index for standard 4-option settings
			UEFModularSettingsLibrary::SetModularSelectedIndex(this, Mapping.Tag, Mapping.EngineLevel, EEFSettingsSource::Local, nullptr, false);
		}

		for (const FGameplayTag& Tag : { GetScreenSpaceReflectionTag(), GetBloomTag(), GetTextureFilteringTag() })
		{
			const int32 ResolvedIndex = ResolveCurrentEngineIndexForTag(Tag);
			if (ResolvedIndex != INDEX_NONE)
			{
				UEFModularSettingsLibrary::SetModularSelectedIndex(this, Tag, ResolvedIndex, EEFSettingsSource::Local, nullptr, false);
			}
		}

		SyncResolutionScaleFromEngine();

		bIsUpdating = false;
	}

	bool DoesResolutionScaleMatchPreset(int32 PresetIndex) const
	{
		if (!ShouldSyncResolutionScaleFromEngine())
		{
			return true;
		}

		const UEFModularSettingsFloat* ResolutionScaleSetting = Cast<UEFModularSettingsFloat>(
			UEFModularSettingsLibrary::GetModularSetting(this, GetResolutionScaleTag(), EEFSettingsSource::Local));
		if (!ResolutionScaleSetting)
		{
			return true;
		}

		if (const FResolvedPresetSnapshot* Snapshot = GetResolvedPresetSnapshot(PresetIndex))
		{
			if (Snapshot->bHasResolutionScale)
			{
				return FMath::IsNearlyEqual(ResolutionScaleSetting->Value, Snapshot->ResolutionScale, 0.5f);
			}
		}

		return true;
	}

	// Check if all current sub-setting indices match the expected values for a preset.
	bool MatchesPreset(int32 PresetIndex)
	{
		for (const FGameplayTag& Tag : GetGraphicsSettingTags())
		{
			UEFModularSettingsMultiSelect* Setting = Cast<UEFModularSettingsMultiSelect>(
				UEFModularSettingsLibrary::GetModularSetting(this, Tag, EEFSettingsSource::Local));

			if (!Setting)
			{
				continue; // Setting not registered yet; don't block preset detection
			}

			const int32 ExpectedIndex = GetExpectedSubSettingIndex(Tag, PresetIndex);
			if (Setting->SelectedIndex != ExpectedIndex)
			{
				return false;
			}
		}

		return DoesResolutionScaleMatchPreset(PresetIndex);
	}
};
