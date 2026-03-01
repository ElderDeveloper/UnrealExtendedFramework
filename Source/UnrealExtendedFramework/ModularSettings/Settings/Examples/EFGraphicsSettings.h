#pragma once

#include "CoreMinimal.h"
#include "UnrealExtendedFramework/ModularSettings/Settings/EFModularSettingsBase.h"
#include "UnrealExtendedFramework/ModularSettings/EFModularSettingsSubsystem.h"
#include "UnrealExtendedFramework/ModularSettings/EFModularSettingsLibrary.h"
#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "EFGraphicsSettings.generated.h"

// Screen Resolution Setting
UCLASS(Blueprintable,EditInlineNew, DisplayName = "Extended Screen Resolution")
class UNREALEXTENDEDFRAMEWORK_API UEFResolutionSetting : public UEFModularSettingsMultiSelect
{
	GENERATED_BODY()

public:
	UEFResolutionSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.Resolution"));
		DisplayName = NSLOCTEXT("Settings", "Resolution", "Screen Resolution");
		ConfigCategory = TEXT("Graphics");
		DefaultValue = TEXT("1920x1080");

		PopulateResolutionOptions();
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
			if (UGameUserSettings* UserSettings = GEngine->GetGameUserSettings())
			{
				const FIntPoint Res = UserSettings->GetScreenResolution();
				return FString::Printf(TEXT("%dx%d"), Res.X, Res.Y);
			}

			if (GEngine->GameViewport)
			{
				FVector2D ViewportSize;
				GEngine->GameViewport->GetViewportSize(ViewportSize);
				return FString::Printf(TEXT("%.0fx%.0f"), ViewportSize.X, ViewportSize.Y);
			}
		}
		
		return TEXT("1920x1080");
	}

private:
	void PopulateResolutionOptions()
	{
		Values.Empty();
		DisplayNames.Empty();

		// Common resolutions
		TArray<FString> CommonResolutions = {
			TEXT("1280x720"),
			TEXT("1366x768"),
			TEXT("1600x900"),
			TEXT("1920x1080"),
			TEXT("2560x1440"),
			TEXT("3840x2160")
		};

		for (const FString& Resolution : CommonResolutions)
		{
			Values.Add(Resolution);
			DisplayNames.Add(FText::FromString(Resolution));
		}

		// Set default index
		int32 DefaultIndex = Values.Find(DefaultValue);
		SelectedIndex = DefaultIndex != INDEX_NONE ? DefaultIndex : 3; // Default to 1920x1080
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
		DisplayName = NSLOCTEXT("Settings", "VSync", "Vertical Sync");
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
		DisplayName = NSLOCTEXT("Settings", "AntiAliasingQuality", "Anti-Aliasing Quality");
		ConfigCategory = TEXT("Graphics");
		DefaultValue = TEXT("High");

		Values = { TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Ultra") };
		DisplayNames = {
			NSLOCTEXT("Settings", "AALow", "Low"),
			NSLOCTEXT("Settings", "AAMedium", "Medium"),
			NSLOCTEXT("Settings", "AAHigh", "High"),
			NSLOCTEXT("Settings", "AAUltra", "Ultra")
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
		DisplayName = NSLOCTEXT("Settings", "TextureQuality", "Texture Quality");
		ConfigCategory = TEXT("Graphics");
		DefaultValue = TEXT("High");

		Values = { TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Ultra") };
		DisplayNames = {
			NSLOCTEXT("Settings", "TextureLow", "Low"),
			NSLOCTEXT("Settings", "TextureMedium", "Medium"),
			NSLOCTEXT("Settings", "TextureHigh", "High"),
			NSLOCTEXT("Settings", "TextureUltra", "Ultra")
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
		DisplayName = NSLOCTEXT("Settings", "TextureFilteringQuality", "Texture Filtering Quality");
		ConfigCategory = TEXT("Graphics");
		DefaultValue = TEXT("8x");
		
		Values = { TEXT("Trilinear"), TEXT("1x"), TEXT("2x"), TEXT("4x"), TEXT("8x"), TEXT("16x") };
		DisplayNames = {
			NSLOCTEXT("Settings", "TextureFilteringTrilinear", "Trilinear"),
			NSLOCTEXT("Settings", "TextureFiltering1x", "1x Anisotropic"),
			NSLOCTEXT("Settings", "TextureFiltering2x", "2x Anisotropic"),
			NSLOCTEXT("Settings", "TextureFiltering4x", "4x Anisotropic"),
			NSLOCTEXT("Settings", "TextureFiltering8x", "8x Anisotropic"),
			NSLOCTEXT("Settings", "TextureFiltering16x", "16x Anisotropic")
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
		DisplayName = NSLOCTEXT("Settings", "FoliageQuality", "Foliage Quality");
		ConfigCategory = TEXT("Graphics");
		DefaultValue = TEXT("Medium");
		
		Values = { TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Ultra") };
		DisplayNames = {
			NSLOCTEXT("Settings", "FoliageLow", "Low"),
			NSLOCTEXT("Settings", "FoliageMedium", "Medium"),
			NSLOCTEXT("Settings", "FoliageHigh", "High"),
			NSLOCTEXT("Settings", "FoliageUltra", "Ultra")
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
		DisplayName = NSLOCTEXT("Settings", "ShadowQuality", "Shadow Quality");
		ConfigCategory = TEXT("Graphics");
		DefaultValue = TEXT("Medium");
		
		Values = { TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Ultra") };
		DisplayNames = {
			NSLOCTEXT("Settings", "ShadowLow", "Low"),
			NSLOCTEXT("Settings", "ShadowMedium", "Medium"),
			NSLOCTEXT("Settings", "ShadowHigh", "High"),
			NSLOCTEXT("Settings", "ShadowUltra", "Ultra")
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
		DisplayName = NSLOCTEXT("Settings", "ShadingQuality", "Shading Quality");
		ConfigCategory = TEXT("Graphics");
		DefaultValue = TEXT("Medium");
		
		Values = { TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Ultra") };
		DisplayNames = {
			NSLOCTEXT("Settings", "ShadingLow", "Low"),
			NSLOCTEXT("Settings", "ShadingMedium", "Medium"),
			NSLOCTEXT("Settings", "ShadingHigh", "High"),
			NSLOCTEXT("Settings", "ShadingUltra", "Ultra")
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
		DisplayName = NSLOCTEXT("Settings", "VFXQuality", "VFX Quality");
		ConfigCategory = TEXT("Graphics");
		DefaultValue = TEXT("Medium");
		
		Values = { TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Ultra") };
		DisplayNames = {
			NSLOCTEXT("Settings", "VFXLow", "Low"),
			NSLOCTEXT("Settings", "VFXMedium", "Medium"),
			NSLOCTEXT("Settings", "VFXHigh", "High"),
			NSLOCTEXT("Settings", "VFXUltra", "Ultra")
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
		DisplayName = NSLOCTEXT("Settings", "PostProcessQuality", "Post-Processing Quality");
		ConfigCategory = TEXT("Graphics");
		DefaultValue = TEXT("Medium");
		
		Values = { TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Ultra") };
		DisplayNames = {
			NSLOCTEXT("Settings", "PostProcessLow", "Low"),
			NSLOCTEXT("Settings", "PostProcessMedium", "Medium"),
			NSLOCTEXT("Settings", "PostProcessHigh", "High"),
			NSLOCTEXT("Settings", "PostProcessUltra", "Ultra")
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
		DisplayName = NSLOCTEXT("Settings", "ViewDistance", "View Distance");
		ConfigCategory = TEXT("Graphics");
		DefaultValue = TEXT("Medium");
		
		Values = { TEXT("Near"), TEXT("Medium"), TEXT("Far"), TEXT("Ultra") };
		DisplayNames = {
			NSLOCTEXT("Settings", "ViewDistanceNear", "Near"),
			NSLOCTEXT("Settings", "ViewDistanceMedium", "Medium"),
			NSLOCTEXT("Settings", "ViewDistanceFar", "Far"),
			NSLOCTEXT("Settings", "ViewDistanceUltra", "Ultra")
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
		DisplayName = NSLOCTEXT("Settings", "ScreenSpaceReflection", "Screen Space Reflections");
		ConfigCategory = TEXT("Graphics");
		DefaultValue = TEXT("Medium");
		
		Values = { TEXT("Off"), TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Ultra") };
		DisplayNames = {
			NSLOCTEXT("Settings", "SSROff", "Off"),
			NSLOCTEXT("Settings", "SSRLow", "Low"),
			NSLOCTEXT("Settings", "SSRMedium", "Medium"),
			NSLOCTEXT("Settings", "SSRHigh", "High"),
			NSLOCTEXT("Settings", "SSRUltra", "Ultra")
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
		DisplayName = NSLOCTEXT("Settings", "Bloom", "Bloom");
		ConfigCategory = TEXT("Graphics");
		DefaultValue = TEXT("Medium");
		
		Values = { TEXT("Off"), TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Ultra") };
		DisplayNames = {
			NSLOCTEXT("Settings", "BloomOff", "Off"),
			NSLOCTEXT("Settings", "BloomLow", "Low"),
			NSLOCTEXT("Settings", "BloomMedium", "Medium"),
			NSLOCTEXT("Settings", "BloomHigh", "High"),
			NSLOCTEXT("Settings", "BloomUltra", "Ultra")
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
		DisplayName = NSLOCTEXT("Settings", "MotionBlurAmount", "Motion Blur Amount");
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
		DisplayName = NSLOCTEXT("Settings", "OverallGraphicsQuality", "Overall Graphics Quality");
		ConfigCategory = TEXT("Graphics");
		DefaultValue = TEXT("Medium");
		
		Values = { TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Ultra"), TEXT("Custom") };
		DisplayNames = {
			NSLOCTEXT("Settings", "OverallQualityLow", "Low"),
			NSLOCTEXT("Settings", "OverallQualityMedium", "Medium"),
			NSLOCTEXT("Settings", "OverallQualityHigh", "High"),
			NSLOCTEXT("Settings", "OverallQualityUltra", "Ultra"),
			NSLOCTEXT("Settings", "OverallQualityCustom", "Custom")
		};
		
		int32 DefaultIndex = Values.Find(DefaultValue);
		SelectedIndex = DefaultIndex != INDEX_NONE ? DefaultIndex : 1;
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	bool bUpdateSettingsImmediately = false;
	
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
				UserSettings->ApplySettings(false);
				UserSettings->SaveSettings();
				UE_LOG(LogTemp, Log, TEXT("Applied & saved Overall Scalability Level via GameUserSettings: %s (Level %d)"), *Values[SelectedIndex], SelectedIndex);
			}

			// Sync modular sub-setting objects to match the engine state.
			// Without this, the UI keeps showing stale values and saving
			// those stale values later overwrites the engine state.
			RefreshSubSettingsFromEngine();
		}
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
	bool bIsUpdating = false;

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
	// accounting for settings with non-standard option layouts.
	static int32 GetExpectedSubSettingIndex(const FGameplayTag& Tag, int32 PresetIndex)
	{
		// SSR and Bloom have an extra "Off" at index 0, so Low=1, Med=2, High=3, Ultra=4
		if (Tag == FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.ScreenSpaceReflection")) ||
			Tag == FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.Bloom")))
		{
			return PresetIndex + 1;
		}

		// Texture Filtering: Trilinear(0), 1x(1), 2x(2), 4x(3), 8x(4), 16x(5)
		if (Tag == FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.TextureFilteringQuality")))
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

		// Handle non-standard settings based on the preset index (SelectedIndex)
		const int32 PresetIdx = SelectedIndex;

		// SSR and Bloom: add 1 for the "Off" option at index 0
		UEFModularSettingsLibrary::SetModularSelectedIndex(this,
			FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.ScreenSpaceReflection")),
			GetExpectedSubSettingIndex(FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.ScreenSpaceReflection")), PresetIdx),
			EEFSettingsSource::Local, nullptr, false);

		UEFModularSettingsLibrary::SetModularSelectedIndex(this,
			FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.Bloom")),
			GetExpectedSubSettingIndex(FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.Bloom")), PresetIdx),
			EEFSettingsSource::Local, nullptr, false);

		// Texture Filtering
		UEFModularSettingsLibrary::SetModularSelectedIndex(this,
			FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.TextureFilteringQuality")),
			GetExpectedSubSettingIndex(FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.TextureFilteringQuality")), PresetIdx),
			EEFSettingsSource::Local, nullptr, false);

		bIsUpdating = false;
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
		return true;
	}
};
