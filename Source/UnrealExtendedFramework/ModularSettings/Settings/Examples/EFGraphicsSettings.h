#pragma once

#include "CoreMinimal.h"
#include "UnrealExtendedFramework/ModularSettings/Settings/EFModularSettingBase.h"
#include "Engine/Engine.h"
#include "EFGraphicsSettings.generated.h"

// Screen Resolution Setting
UCLASS(Blueprintable, DisplayName = "Screen Resolution")
class UNREALEXTENDEDFRAMEWORK_API UEFResolutionSetting : public UEFModularSettingMultiSelect
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
	
	virtual void Apply() override
	{
		if (Values.IsValidIndex(SelectedIndex))
		{
			FString Resolution = Values[SelectedIndex];
			int32 Width, Height;
			
			if (ParseResolution(Resolution, Width, Height))
			{
				if (GEngine && GEngine->GameViewport)
				{
					GEngine->GameViewport->GetWindow()->Resize(FVector2D(Width, Height));
					UE_LOG(LogTemp, Log, TEXT("Applied resolution: %dx%d"), Width, Height);
				}
			}
		}
	}
	
	// Helper method to get current resolution
	UFUNCTION(BlueprintCallable, Category = "Graphics Settings")
	FString GetCurrentResolution() const
	{
		if (GEngine && GEngine->GameViewport)
		{
			FVector2D ViewportSize;
			GEngine->GameViewport->GetViewportSize(ViewportSize);
			return FString::Printf(TEXT("%.0fx%.0f"), ViewportSize.X, ViewportSize.Y);
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

// Graphics Quality Setting
UCLASS(Blueprintable, DisplayName = "Graphics Quality")
class UNREALEXTENDEDFRAMEWORK_API UEFGraphicsQualitySetting : public UEFModularSettingMultiSelect
{
	GENERATED_BODY()
	
public:
	UEFGraphicsQualitySetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.Quality"));
		DisplayName = NSLOCTEXT("Settings", "GraphicsQuality", "Graphics Quality");
		ConfigCategory = TEXT("Graphics");
		DefaultValue = TEXT("Medium");
		
		Values = { TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Ultra") };
		DisplayNames = {
			NSLOCTEXT("Settings", "QualityLow", "Low"),
			NSLOCTEXT("Settings", "QualityMedium", "Medium"),
			NSLOCTEXT("Settings", "QualityHigh", "High"),
			NSLOCTEXT("Settings", "QualityUltra", "Ultra")
		};
		
		int32 DefaultIndex = Values.Find(DefaultValue);
		SelectedIndex = DefaultIndex != INDEX_NONE ? DefaultIndex : 1;
	}
	
	virtual void Apply() override
	{
		if (Values.IsValidIndex(SelectedIndex))
		{
			FString Quality = Values[SelectedIndex];
			int32 QualityLevel = SelectedIndex;
			
			// Apply scalability settings based on quality level
			if (GEngine)
			{
				// Apply to various scalability groups
				ApplyScalabilitySettings(QualityLevel);
				UE_LOG(LogTemp, Log, TEXT("Applied graphics quality: %s (Level %d)"), *Quality, QualityLevel);
			}
		}
	}
	
	// Helper method to get current quality level
	UFUNCTION(BlueprintCallable, Category = "Graphics Settings")
	int32 GetCurrentQualityLevel() const
	{
		return SelectedIndex;
	}
	
private:
	void ApplyScalabilitySettings(int32 QualityLevel)
	{
		// This would integrate with Unreal's scalability system
		// For now, we'll just log the intended changes
		
		switch (QualityLevel)
		{
		case 0: // Low
			UE_LOG(LogTemp, Log, TEXT("Would apply Low quality settings"));
			break;
		case 1: // Medium
			UE_LOG(LogTemp, Log, TEXT("Would apply Medium quality settings"));
			break;
		case 2: // High
			UE_LOG(LogTemp, Log, TEXT("Would apply High quality settings"));
			break;
		case 3: // Ultra
			UE_LOG(LogTemp, Log, TEXT("Would apply Ultra quality settings"));
			break;
		default:
			UE_LOG(LogTemp, Log, TEXT("Would apply Medium quality settings (default)"));
			break;
		}
	}
};

// VSync Setting
UCLASS(Blueprintable, DisplayName = "VSync")
class UNREALEXTENDEDFRAMEWORK_API UEFVSyncSetting : public UEFModularSettingBool
{
	GENERATED_BODY()
	
public:
	UEFVSyncSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.VSync"));
		DisplayName = NSLOCTEXT("Settings", "VSync", "Vertical Sync");
		ConfigCategory = TEXT("Graphics");
		DefaultValue = TEXT("true");
		bValue = true;
	}
	
	virtual void Apply() override
	{
		if (GEngine)
		{
			// Apply VSync setting to the engine
			// This would typically involve console commands or engine settings
			FString Command = bValue ? TEXT("r.VSync 1") : TEXT("r.VSync 0");
			GEngine->Exec(GEngine->GetWorld(), *Command);
			
			UE_LOG(LogTemp, Log, TEXT("Applied VSync: %s"), bValue ? TEXT("Enabled") : TEXT("Disabled"));
		}
	}
	
	// Helper method to check current VSync state
	UFUNCTION(BlueprintCallable, Category = "Graphics Settings")
	bool IsVSyncEnabled() const
	{
		return bValue;
	}
};

// Frame Rate Limit Setting
UCLASS(Blueprintable, DisplayName = "Frame Rate Limit")
class UNREALEXTENDEDFRAMEWORK_API UEFFrameRateLimitSetting : public UEFModularSettingMultiSelect
{
	GENERATED_BODY()
	
public:
	UEFFrameRateLimitSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.FrameRateLimit"));
		DisplayName = NSLOCTEXT("Settings", "FrameRateLimit", "Frame Rate Limit");
		ConfigCategory = TEXT("Graphics");
		DefaultValue = TEXT("60");
		
		Values = { TEXT("30"), TEXT("60"), TEXT("120"), TEXT("144"), TEXT("Unlimited") };
		DisplayNames = {
			NSLOCTEXT("Settings", "FPS30", "30 FPS"),
			NSLOCTEXT("Settings", "FPS60", "60 FPS"),
			NSLOCTEXT("Settings", "FPS120", "120 FPS"),
			NSLOCTEXT("Settings", "FPS144", "144 FPS"),
			NSLOCTEXT("Settings", "FPSUnlimited", "Unlimited")
		};
		
		int32 DefaultIndex = Values.Find(DefaultValue);
		SelectedIndex = DefaultIndex != INDEX_NONE ? DefaultIndex : 1;
	}
	
	virtual void Apply() override
	{
		if (Values.IsValidIndex(SelectedIndex))
		{
			FString FrameRateLimit = Values[SelectedIndex];
			
			if (GEngine)
			{
				FString Command;
				if (FrameRateLimit == TEXT("Unlimited"))
				{
					Command = TEXT("t.MaxFPS 0");
				}
				else
				{
					int32 FPSLimit = FCString::Atoi(*FrameRateLimit);
					Command = FString::Printf(TEXT("t.MaxFPS %d"), FPSLimit);
				}
				
				GEngine->Exec(GEngine->GetWorld(), *Command);
				UE_LOG(LogTemp, Log, TEXT("Applied Frame Rate Limit: %s"), *FrameRateLimit);
			}
		}
	}
	
	// Helper method to get current frame rate limit
	UFUNCTION(BlueprintCallable, Category = "Graphics Settings")
	FString GetCurrentFrameRateLimit() const
	{
		if (Values.IsValidIndex(SelectedIndex))
		{
			return Values[SelectedIndex];
		}
		return TEXT("60");
	}
};

// Anti-Aliasing Setting
UCLASS(Blueprintable, DisplayName = "Anti-Aliasing")
class UNREALEXTENDEDFRAMEWORK_API UEFAntiAliasingSetting : public UEFModularSettingMultiSelect
{
	GENERATED_BODY()
	
public:
	UEFAntiAliasingSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.AntiAliasing"));
		DisplayName = NSLOCTEXT("Settings", "AntiAliasing", "Anti-Aliasing");
		ConfigCategory = TEXT("Graphics");
		DefaultValue = TEXT("TAA");
		
		Values = { TEXT("Off"), TEXT("FXAA"), TEXT("TAA"), TEXT("MSAA_2x"), TEXT("MSAA_4x") };
		DisplayNames = {
			NSLOCTEXT("Settings", "AAOff", "Off"),
			NSLOCTEXT("Settings", "AAFXAA", "FXAA"),
			NSLOCTEXT("Settings", "AATAA", "TAA"),
			NSLOCTEXT("Settings", "AAMSAA2x", "MSAA 2x"),
			NSLOCTEXT("Settings", "AAMSAA4x", "MSAA 4x")
		};
		
		int32 DefaultIndex = Values.Find(DefaultValue);
		SelectedIndex = DefaultIndex != INDEX_NONE ? DefaultIndex : 2;
	}
	
	virtual void Apply() override
	{
		if (Values.IsValidIndex(SelectedIndex))
		{
			FString AAMode = Values[SelectedIndex];
			
			if (GEngine)
			{
				FString Command;
				if (AAMode == TEXT("Off"))
				{
					Command = TEXT("r.AntiAliasingMethod 0");
				}
				else if (AAMode == TEXT("FXAA"))
				{
					Command = TEXT("r.AntiAliasingMethod 1");
				}
				else if (AAMode == TEXT("TAA"))
				{
					Command = TEXT("r.AntiAliasingMethod 2");
				}
				else if (AAMode == TEXT("MSAA_2x"))
				{
					Command = TEXT("r.AntiAliasingMethod 3; r.MSAACount 2");
				}
				else if (AAMode == TEXT("MSAA_4x"))
				{
					Command = TEXT("r.AntiAliasingMethod 3; r.MSAACount 4");
				}
				
				GEngine->Exec(GEngine->GetWorld(), *Command);
				UE_LOG(LogTemp, Log, TEXT("Applied Anti-Aliasing: %s"), *AAMode);
			}
		}
	}
	
	UFUNCTION(BlueprintCallable, Category = "Graphics Settings")
	FString GetCurrentAntiAliasing() const
	{
		if (Values.IsValidIndex(SelectedIndex))
		{
			return Values[SelectedIndex];
		}
		return TEXT("TAA");
	}
};

// Texture Quality Setting
UCLASS(Blueprintable, DisplayName = "Texture Quality")
class UNREALEXTENDEDFRAMEWORK_API UEFTextureQualitySetting : public UEFModularSettingMultiSelect
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
	
	virtual void Apply() override
	{
		if (Values.IsValidIndex(SelectedIndex))
		{
			FString Quality = Values[SelectedIndex];
			int32 QualityLevel = SelectedIndex;
			
			if (GEngine)
			{
				FString Command = FString::Printf(TEXT("r.TextureQuality %d"), QualityLevel);
				GEngine->Exec(GEngine->GetWorld(), *Command);
				UE_LOG(LogTemp, Log, TEXT("Applied Texture Quality: %s (Level %d)"), *Quality, QualityLevel);
			}
		}
	}
};

// Shadow Quality Setting
UCLASS(Blueprintable, DisplayName = "Shadow Quality")
class UNREALEXTENDEDFRAMEWORK_API UEFShadowQualitySetting : public UEFModularSettingMultiSelect
{
	GENERATED_BODY()
	
public:
	UEFShadowQualitySetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.ShadowQuality"));
		DisplayName = NSLOCTEXT("Settings", "ShadowQuality", "Shadow Quality");
		ConfigCategory = TEXT("Graphics");
		DefaultValue = TEXT("Medium");
		
		Values = { TEXT("Off"), TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Ultra") };
		DisplayNames = {
			NSLOCTEXT("Settings", "ShadowOff", "Off"),
			NSLOCTEXT("Settings", "ShadowLow", "Low"),
			NSLOCTEXT("Settings", "ShadowMedium", "Medium"),
			NSLOCTEXT("Settings", "ShadowHigh", "High"),
			NSLOCTEXT("Settings", "ShadowUltra", "Ultra")
		};
		
		int32 DefaultIndex = Values.Find(DefaultValue);
		SelectedIndex = DefaultIndex != INDEX_NONE ? DefaultIndex : 2;
	}
	
	virtual void Apply() override
	{
		if (Values.IsValidIndex(SelectedIndex))
		{
			FString Quality = Values[SelectedIndex];
			int32 QualityLevel = SelectedIndex;
			
			if (GEngine)
			{
				FString Command = FString::Printf(TEXT("r.ShadowQuality %d"), QualityLevel);
				GEngine->Exec(GEngine->GetWorld(), *Command);
				UE_LOG(LogTemp, Log, TEXT("Applied Shadow Quality: %s (Level %d)"), *Quality, QualityLevel);
			}
		}
	}
};

// Post-Processing Quality Setting
UCLASS(Blueprintable, DisplayName = "Post-Processing Quality")
class UNREALEXTENDEDFRAMEWORK_API UEFPostProcessQualitySetting : public UEFModularSettingMultiSelect
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
	
	virtual void Apply() override
	{
		if (Values.IsValidIndex(SelectedIndex))
		{
			FString Quality = Values[SelectedIndex];
			int32 QualityLevel = SelectedIndex;
			
			if (GEngine)
			{
				FString Command = FString::Printf(TEXT("r.PostProcessQuality %d"), QualityLevel);
				GEngine->Exec(GEngine->GetWorld(), *Command);
				UE_LOG(LogTemp, Log, TEXT("Applied Post-Processing Quality: %s (Level %d)"), *Quality, QualityLevel);
			}
		}
	}
};

// View Distance Setting
UCLASS(Blueprintable, DisplayName = "View Distance")
class UNREALEXTENDEDFRAMEWORK_API UEFViewDistanceSetting : public UEFModularSettingMultiSelect
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
	
	virtual void Apply() override
	{
		if (Values.IsValidIndex(SelectedIndex))
		{
			FString Quality = Values[SelectedIndex];
			int32 QualityLevel = SelectedIndex;
			
			if (GEngine)
			{
				FString Command = FString::Printf(TEXT("r.ViewDistanceScale %f"), 0.5f + (QualityLevel * 0.5f));
				GEngine->Exec(GEngine->GetWorld(), *Command);
				UE_LOG(LogTemp, Log, TEXT("Applied View Distance: %s (Level %d)"), *Quality, QualityLevel);
			}
		}
	}
};

// Motion Blur Setting
UCLASS(Blueprintable, DisplayName = "Motion Blur")
class UNREALEXTENDEDFRAMEWORK_API UEFMotionBlurSetting : public UEFModularSettingBool
{
	GENERATED_BODY()
	
public:
	UEFMotionBlurSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.MotionBlur"));
		DisplayName = NSLOCTEXT("Settings", "MotionBlur", "Motion Blur");
		ConfigCategory = TEXT("Graphics");
		DefaultValue = TEXT("false");
		bValue = false;
	}
	
	virtual void Apply() override
	{
		if (GEngine)
		{
			FString Command = bValue ? TEXT("r.MotionBlurQuality 3") : TEXT("r.MotionBlurQuality 0");
			GEngine->Exec(GEngine->GetWorld(), *Command);
			UE_LOG(LogTemp, Log, TEXT("Applied Motion Blur: %s"), bValue ? TEXT("Enabled") : TEXT("Disabled"));
		}
	}
};

// Screen Space Reflections Setting
UCLASS(Blueprintable, DisplayName = "Screen Space Reflections")
class UNREALEXTENDEDFRAMEWORK_API UEFSSRSetting : public UEFModularSettingBool
{
	GENERATED_BODY()
	
public:
	UEFSSRSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.SSR"));
		DisplayName = NSLOCTEXT("Settings", "SSR", "Screen Space Reflections");
		ConfigCategory = TEXT("Graphics");
		DefaultValue = TEXT("true");
		bValue = true;
	}
	
	virtual void Apply() override
	{
		if (GEngine)
		{
			FString Command = bValue ? TEXT("r.SSR.Quality 3") : TEXT("r.SSR.Quality 0");
			GEngine->Exec(GEngine->GetWorld(), *Command);
			UE_LOG(LogTemp, Log, TEXT("Applied Screen Space Reflections: %s"), bValue ? TEXT("Enabled") : TEXT("Disabled"));
		}
	}
};

// Bloom Setting
UCLASS(Blueprintable, DisplayName = "Bloom")
class UNREALEXTENDEDFRAMEWORK_API UEFBloomSetting : public UEFModularSettingBool
{
	GENERATED_BODY()
	
public:
	UEFBloomSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.Bloom"));
		DisplayName = NSLOCTEXT("Settings", "Bloom", "Bloom");
		ConfigCategory = TEXT("Graphics");
		DefaultValue = TEXT("true");
		bValue = true;
	}
	
	virtual void Apply() override
	{
		if (GEngine)
		{
			FString Command = bValue ? TEXT("r.BloomQuality 5") : TEXT("r.BloomQuality 0");
			GEngine->Exec(GEngine->GetWorld(), *Command);
			UE_LOG(LogTemp, Log, TEXT("Applied Bloom: %s"), bValue ? TEXT("Enabled") : TEXT("Disabled"));
		}
	}
};

// Lens Flares Setting
UCLASS(Blueprintable, DisplayName = "Lens Flares")
class UNREALEXTENDEDFRAMEWORK_API UEFLensFlaresSetting : public UEFModularSettingBool
{
	GENERATED_BODY()
	
public:
	UEFLensFlaresSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.LensFlares"));
		DisplayName = NSLOCTEXT("Settings", "LensFlares", "Lens Flares");
		ConfigCategory = TEXT("Graphics");
		DefaultValue = TEXT("true");
		bValue = true;
	}
	
	virtual void Apply() override
	{
		if (GEngine)
		{
			FString Command = bValue ? TEXT("r.LensFlareQuality 3") : TEXT("r.LensFlareQuality 0");
			GEngine->Exec(GEngine->GetWorld(), *Command);
			UE_LOG(LogTemp, Log, TEXT("Applied Lens Flares: %s"), bValue ? TEXT("Enabled") : TEXT("Disabled"));
		}
	}
};

// Enhanced Graphics Quality Setting with Scalability Integration
UCLASS(Blueprintable, DisplayName = "Overall Graphics Quality")
class UNREALEXTENDEDFRAMEWORK_API UEFOverallGraphicsQualitySetting : public UEFModularSettingMultiSelect
{
	GENERATED_BODY()
	
public:
	UEFOverallGraphicsQualitySetting()
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
	
	virtual void Apply() override
	{
		if (Values.IsValidIndex(SelectedIndex))
		{
			FString Quality = Values[SelectedIndex];
			int32 QualityLevel = SelectedIndex;
			
			if (GEngine && Quality != TEXT("Custom"))
			{
				// Apply comprehensive quality settings
				ApplyComprehensiveQualitySettings(QualityLevel);
				UE_LOG(LogTemp, Log, TEXT("Applied Overall Graphics Quality: %s (Level %d)"), *Quality, QualityLevel);
			}
		}
	}
	
private:
	void ApplyComprehensiveQualitySettings(int32 QualityLevel)
	{
		if (!GEngine) return;
		
		TArray<FString> Commands;
		
		switch (QualityLevel)
		{
		case 0: // Low
			Commands.Add(TEXT("r.ViewDistanceScale 0.5"));
			Commands.Add(TEXT("r.ShadowQuality 0"));
			Commands.Add(TEXT("r.TextureQuality 0"));
			Commands.Add(TEXT("r.PostProcessQuality 0"));
			Commands.Add(TEXT("r.AntiAliasingMethod 1")); // FXAA
			Commands.Add(TEXT("r.MotionBlurQuality 0"));
			Commands.Add(TEXT("r.SSR.Quality 0"));
			Commands.Add(TEXT("r.BloomQuality 1"));
			break;
			
		case 1: // Medium
			Commands.Add(TEXT("r.ViewDistanceScale 0.75"));
			Commands.Add(TEXT("r.ShadowQuality 2"));
			Commands.Add(TEXT("r.TextureQuality 1"));
			Commands.Add(TEXT("r.PostProcessQuality 1"));
			Commands.Add(TEXT("r.AntiAliasingMethod 2")); // TAA
			Commands.Add(TEXT("r.MotionBlurQuality 1"));
			Commands.Add(TEXT("r.SSR.Quality 2"));
			Commands.Add(TEXT("r.BloomQuality 3"));
			break;
			
		case 2: // High
			Commands.Add(TEXT("r.ViewDistanceScale 1.0"));
			Commands.Add(TEXT("r.ShadowQuality 3"));
			Commands.Add(TEXT("r.TextureQuality 2"));
			Commands.Add(TEXT("r.PostProcessQuality 2"));
			Commands.Add(TEXT("r.AntiAliasingMethod 2")); // TAA
			Commands.Add(TEXT("r.MotionBlurQuality 2"));
			Commands.Add(TEXT("r.SSR.Quality 3"));
			Commands.Add(TEXT("r.BloomQuality 4"));
			break;
			
		case 3: // Ultra
			Commands.Add(TEXT("r.ViewDistanceScale 1.25"));
			Commands.Add(TEXT("r.ShadowQuality 4"));
			Commands.Add(TEXT("r.TextureQuality 3"));
			Commands.Add(TEXT("r.PostProcessQuality 3"));
			Commands.Add(TEXT("r.AntiAliasingMethod 2")); // TAA
			Commands.Add(TEXT("r.MotionBlurQuality 3"));
			Commands.Add(TEXT("r.SSR.Quality 4"));
			Commands.Add(TEXT("r.BloomQuality 5"));
			break;
		}
		
		// Execute all commands
		for (const FString& Command : Commands)
		{
			GEngine->Exec(GEngine->GetWorld(), *Command);
		}
	}
};
