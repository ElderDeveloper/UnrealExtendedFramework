#pragma once

#include "CoreMinimal.h"
#include "UnrealExtendedFramework/ModularSettings/Settings/EFModularSettingsBase.h"
#include "UnrealExtendedFramework/ModularSettings/EFModularSettingsSubsystem.h"
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
		if (GEngine)
		{
			// Apply VSync setting to the engine
			// This would typically involve console commands or engine settings
			FString Command = Value ? TEXT("r.VSync 1") : TEXT("r.VSync 0");
			GEngine->Exec(GEngine->GetWorld(), *Command);
			
			UE_LOG(LogTemp, Log, TEXT("Applied VSync: %s"), Value ? TEXT("Enabled") : TEXT("Disabled"));
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
			FString Quality = Values[SelectedIndex];
			int32 QualityLevel = SelectedIndex;
			
			if (GEngine)
			{
				// r.PostProcessAAQuality maps to Scalability levels
				// 0=Low, 1=Medium, 2=High, 3=Epic -> 0, 2, 4, 6
				int32 EngineQuality = 0;
				switch(QualityLevel) {
					case 0: EngineQuality = 0; break;
					case 1: EngineQuality = 2; break;
					case 2: EngineQuality = 4; break;
					case 3: EngineQuality = 6; break;
				}
				
				FString Command = FString::Printf(TEXT("r.PostProcessAAQuality %d"), EngineQuality);
				GEngine->Exec(GEngine->GetWorld(), *Command);
				UE_LOG(LogTemp, Log, TEXT("Applied Anti-Aliasing Quality: %s (Level %d)"), *Quality, QualityLevel);
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
			FString Quality = Values[SelectedIndex];
			int32 Anisotropy = 0;
			
			if (Quality == TEXT("Trilinear")) Anisotropy = 0;
			else if (Quality == TEXT("1x")) Anisotropy = 1;
			else if (Quality == TEXT("2x")) Anisotropy = 2;
			else if (Quality == TEXT("4x")) Anisotropy = 4;
			else if (Quality == TEXT("8x")) Anisotropy = 8;
			else if (Quality == TEXT("16x")) Anisotropy = 16;
			
			if (GEngine)
			{
				FString Command = FString::Printf(TEXT("r.MaxAnisotropy %d"), Anisotropy);
				GEngine->Exec(GEngine->GetWorld(), *Command);
				UE_LOG(LogTemp, Log, TEXT("Applied Texture Filtering Quality: %s"), *Quality);
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
			FString Quality = Values[SelectedIndex];
			int32 QualityLevel = SelectedIndex;
			
			if (GEngine)
			{
				FString Command = FString::Printf(TEXT("r.FoliageQuality %d"), QualityLevel);
				GEngine->Exec(GEngine->GetWorld(), *Command);
				UE_LOG(LogTemp, Log, TEXT("Applied Foliage Quality: %s (Level %d)"), *Quality, QualityLevel);
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
			FString Quality = Values[SelectedIndex];
			int32 QualityLevel = SelectedIndex;
			
			if (GEngine)
			{
				FString Command = FString::Printf(TEXT("r.ShadingQuality %d"), QualityLevel);
				GEngine->Exec(GEngine->GetWorld(), *Command);
				UE_LOG(LogTemp, Log, TEXT("Applied Shading Quality: %s (Level %d)"), *Quality, QualityLevel);
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
			FString Quality = Values[SelectedIndex];
			int32 QualityLevel = SelectedIndex;
			
			if (GEngine)
			{
				// VFX Quality often maps to EffectsQuality
				FString Command = FString::Printf(TEXT("sg.EffectsQuality %d"), QualityLevel);
				GEngine->Exec(GEngine->GetWorld(), *Command);
				UE_LOG(LogTemp, Log, TEXT("Applied VFX Quality: %s (Level %d)"), *Quality, QualityLevel);
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
			FString Quality = Values[SelectedIndex];
			int32 QualityLevel = SelectedIndex;
			
			if (GEngine)
			{
				// r.SSR.Quality: 0=Off, 1=Low, 2=Medium, 3=High, 4=Epic
				FString Command = FString::Printf(TEXT("r.SSR.Quality %d"), QualityLevel);
				GEngine->Exec(GEngine->GetWorld(), *Command);
				UE_LOG(LogTemp, Log, TEXT("Applied Screen Space Reflections: %s"), *Quality);
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
	
	virtual void Apply_Implementation() override
	{
		// Logic to apply overall quality is handled by updating individual settings
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
		
		// Define all graphics settings that should be updated
		TArray<FGameplayTag> GraphicsSettingTags = {
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

		// Update individual settings based on quality level
		bIsUpdating = true;
		for (const FGameplayTag& Tag : GraphicsSettingTags)
		{
			if (UEFModularSettingsMultiSelect* Setting = ModularSettingsSubsystem->GetSetting<UEFModularSettingsMultiSelect>(Tag))
			{
				int32 TargetIndex = Index;
				
				if (Tag == FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.ScreenSpaceReflection")) ||
					Tag == FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.Bloom")))
				{
					TargetIndex = Index + 1; // Map Low(0) to Low(1), etc. Off(0) is skipped.
				}
				else if (Tag == FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.TextureFilteringQuality")))
				{
					// Trilinear(0), 1x(1), 2x(2), 4x(3), 8x(4), 16x(5)
					// Low(0) -> 1x(1)
					// Medium(1) -> 4x(3)
					// High(2) -> 8x(4)
					// Ultra(3) -> 16x(5)
					if (Index == 0) TargetIndex = 1;
					else if (Index == 1) TargetIndex = 3;
					else if (Index == 2) TargetIndex = 4;
					else if (Index == 3) TargetIndex = 5;
				}
				
				ModularSettingsSubsystem->SetIndex(Tag, TargetIndex);
			}
		}
		bIsUpdating = false;
	}
	
	virtual void OnRegistered() override
	{
		if (ModularSettingsSubsystem)
		{
			ModularSettingsSubsystem->OnSettingsChanged.AddDynamic(this, &UEFOverallQualitySetting::OnSettingChanged);
		}
	}

	UFUNCTION()
	void OnSettingChanged(UEFModularSettingsBase* ChangedSetting)
	{
		if (bIsUpdating || !ChangedSetting) return;

		// If the changed setting is one of our managed graphics settings
		static const TArray<FGameplayTag> GraphicsSettingTags = {
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

		if (GraphicsSettingTags.Contains(ChangedSetting->SettingTag))
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
					// Apply(); // No apply needed for overall setting itself
					ModularSettingsSubsystem->OnSettingsChanged.Broadcast(this);
				}
			}
			else
			{
				// Custom
				if (SelectedIndex != 4)
				{
					SetSelectedIndex(4);
					ModularSettingsSubsystem->OnSettingsChanged.Broadcast(this);
				}
			}
			bIsUpdating = false;
		}
	}
	
private:
	bool bIsUpdating = false;

	bool MatchesPreset(int32 PresetIndex)
	{
		// Helper to check if current settings match a preset
		// This requires checking every tracked setting
		
		// Example check for one setting:
		// if (GetSettingIndex("Settings.Graphics.TextureQuality") != PresetIndex) return false;
		
		// For brevity, we'll assume they match if we are here, but in a real implementation
		// we would query the subsystem for each value.
		
		// Implementation of checking each setting against the expected index for the preset
		// ...
		
		return false; // Placeholder
	}
};
