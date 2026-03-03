// EFUpscalerSettings.h
// Modular Settings for image upscalers (DLSS, FSR, XeSS), frame generation, and resolution scale.
// Uses compile-time guards: WITH_DLSS, WITH_FSR, WITH_XESS (set by UnrealExtendedFramework.Build.cs).

#pragma once

#include "CoreMinimal.h"
#include "UnrealExtendedFramework/ModularSettings/Settings/EFModularSettingsBase.h"
#include "UnrealExtendedFramework/ModularSettings/EFModularSettingsSubsystem.h"
#include "UnrealExtendedFramework/ModularSettings/EFModularSettingsLibrary.h"
#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"

// Compile-time upscaler guards — default to 0 if not defined by Build.cs
#ifndef WITH_DLSS
#define WITH_DLSS 0
#endif
#ifndef WITH_FSR4
#define WITH_FSR4 0
#endif
#ifndef WITH_FSR3
#define WITH_FSR3 0
#endif
#ifndef WITH_FSR
#define WITH_FSR WITH_FSR3 || WITH_FSR4
#endif
#ifndef WITH_FSR_GENERIC
#define WITH_FSR_GENERIC 0
#endif
#ifndef WITH_XESS
#define WITH_XESS 0
#endif

// Vendor includes (compile-guarded)
#if WITH_DLSS
#include "DLSSLibrary.h"
#endif

#if WITH_FSR
#include "FFXFSRSettings.h"
#endif
#if WITH_FSR4
#include "FFXFSR4Settings.h"
#elif WITH_FSR3
#include "FFXFSR3Settings.h"
#endif

#if WITH_XESS
#include "XeSSBlueprintLibrary.h"
#include "XeFGBlueprintLibrary.h"
#endif

#include "EFUpscalerSettings.generated.h"


// ─────────────────────────────────────────────────────────────────────────────
//  Upscaler Selection (None / DLSS / FSR / XeSS)
// ─────────────────────────────────────────────────────────────────────────────
UCLASS(Blueprintable, EditInlineNew, DisplayName = "Extended Upscaler Selection")
class UNREALEXTENDEDFRAMEWORK_API UEFUpscalerSelectSetting : public UEFModularSettingsMultiSelect
{
	GENERATED_BODY()

public:
	UEFUpscalerSelectSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.Upscaler"));
		DisplayName = NSLOCTEXT("Settings", "Upscaler", "Upscaler");
		ConfigCategory = TEXT("Graphics");
		DefaultValue = TEXT("None");

		// Always include None
		Values.Add(TEXT("None"));
		DisplayNames.Add(NSLOCTEXT("Settings", "UpscalerNone", "None"));

#if WITH_DLSS
		Values.Add(TEXT("DLSS"));
		DisplayNames.Add(NSLOCTEXT("Settings", "UpscalerDLSS", "NVIDIA DLSS"));
#endif

#if WITH_FSR
		Values.Add(TEXT("FSR"));
		DisplayNames.Add(NSLOCTEXT("Settings", "UpscalerFSR", "AMD FSR"));
#endif

#if WITH_XESS
		Values.Add(TEXT("XeSS"));
		DisplayNames.Add(NSLOCTEXT("Settings", "UpscalerXeSS", "Intel XeSS"));
#endif

		SelectedIndex = 0;
	}

	virtual void Apply_Implementation() override;

	/** Returns the currently selected upscaler as a string key (None/DLSS/FSR/XeSS). */
	UFUNCTION(BlueprintPure, Category = "Upscaler")
	FString GetActiveUpscaler() const
	{
		return Values.IsValidIndex(SelectedIndex) ? Values[SelectedIndex] : TEXT("None");
	}

	/** Check if a specific upscaler is available on this hardware at runtime. */
	UFUNCTION(BlueprintPure, Category = "Upscaler")
	static bool IsUpscalerAvailable(const FString& UpscalerName);
};


// ─────────────────────────────────────────────────────────────────────────────
//  DLSS Quality Mode
// ─────────────────────────────────────────────────────────────────────────────
UCLASS(Blueprintable, EditInlineNew, DisplayName = "Extended DLSS Mode")
class UNREALEXTENDEDFRAMEWORK_API UEFUpscalerDLSSModeSetting : public UEFModularSettingsMultiSelect
{
	GENERATED_BODY()

public:
	UEFUpscalerDLSSModeSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.Upscaler.DLSS.Mode"));
		DisplayName = NSLOCTEXT("Settings", "DLSSMode", "DLSS Mode");
		ConfigCategory = TEXT("Graphics");
		DefaultValue = TEXT("Auto");

		// Maps to UDLSSMode enum (1=Auto, 2=DLAA, 3=UltraQuality, 4=Quality, 5=Balanced, 6=Performance, 7=UltraPerformance)
		Values = {
			TEXT("Auto"),
			TEXT("DLAA"),
			TEXT("UltraQuality"),
			TEXT("Quality"),
			TEXT("Balanced"),
			TEXT("Performance"),
			TEXT("UltraPerformance")
		};

		DisplayNames = {
			NSLOCTEXT("Settings", "DLSSAuto", "Auto"),
			NSLOCTEXT("Settings", "DLSSDLAA", "DLAA"),
			NSLOCTEXT("Settings", "DLSSUltraQuality", "Ultra Quality"),
			NSLOCTEXT("Settings", "DLSSQuality", "Quality"),
			NSLOCTEXT("Settings", "DLSSBalanced", "Balanced"),
			NSLOCTEXT("Settings", "DLSSPerformance", "Performance"),
			NSLOCTEXT("Settings", "DLSSUltraPerf", "Ultra Performance")
		};

		SelectedIndex = 0;
	}

	virtual void Apply_Implementation() override;

	/** Convert our index (0-based) to the vendor enum value (1-based for UDLSSMode). */
	int32 GetDLSSModeValue() const { return SelectedIndex + 1; }
};


// ─────────────────────────────────────────────────────────────────────────────
//  DLSS Ray Reconstruction
// ─────────────────────────────────────────────────────────────────────────────
UCLASS(Blueprintable, EditInlineNew, DisplayName = "Extended DLSS Ray Reconstruction")
class UNREALEXTENDEDFRAMEWORK_API UEFUpscalerDLSSRRSetting : public UEFModularSettingsBool
{
	GENERATED_BODY()

public:
	UEFUpscalerDLSSRRSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.Upscaler.DLSS.RayReconstruction"));
		DisplayName = NSLOCTEXT("Settings", "DLSSRR", "DLSS Ray Reconstruction");
		ConfigCategory = TEXT("Graphics");
		DefaultValue = false;
		Value = false;
	}

	virtual void Apply_Implementation() override;

	UFUNCTION(BlueprintPure, Category = "DLSS")
	static bool IsDLSSRayReconstructionAvailable();
};


// ─────────────────────────────────────────────────────────────────────────────
//  FSR Quality Mode
// ─────────────────────────────────────────────────────────────────────────────
UCLASS(Blueprintable, EditInlineNew, DisplayName = "Extended FSR Mode")
class UNREALEXTENDEDFRAMEWORK_API UEFUpscalerFSRModeSetting : public UEFModularSettingsMultiSelect
{
	GENERATED_BODY()

public:
	UEFUpscalerFSRModeSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.Upscaler.FSR.Mode"));
		DisplayName = NSLOCTEXT("Settings", "FSRMode", "FSR Mode");
		ConfigCategory = TEXT("Graphics");
		DefaultValue = TEXT("Quality");

		// Maps to r.FidelityFX.FSR4.QualityMode (0=NativeAA, 1=Quality, 2=Balanced, 3=Performance, 4=UltraPerformance)
		Values = {
			TEXT("NativeAA"),
			TEXT("Quality"),
			TEXT("Balanced"),
			TEXT("Performance"),
			TEXT("UltraPerformance")
		};

		DisplayNames = {
			NSLOCTEXT("Settings", "FSRNativeAA", "Native AA"),
			NSLOCTEXT("Settings", "FSRQuality", "Quality"),
			NSLOCTEXT("Settings", "FSRBalanced", "Balanced"),
			NSLOCTEXT("Settings", "FSRPerformance", "Performance"),
			NSLOCTEXT("Settings", "FSRUltraPerf", "Ultra Performance")
		};

		int32 DefaultIndex = Values.Find(DefaultValue);
		SelectedIndex = DefaultIndex != INDEX_NONE ? DefaultIndex : 1;
	}

	virtual void Apply_Implementation() override;
};


// ─────────────────────────────────────────────────────────────────────────────
//  FSR Sharpness
// ─────────────────────────────────────────────────────────────────────────────
UCLASS(Blueprintable, EditInlineNew, DisplayName = "Extended FSR Sharpness")
class UNREALEXTENDEDFRAMEWORK_API UEFUpscalerFSRSharpnessSetting : public UEFModularSettingsFloat
{
	GENERATED_BODY()

public:
	UEFUpscalerFSRSharpnessSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.Upscaler.FSR.Sharpness"));
		DisplayName = NSLOCTEXT("Settings", "FSRSharpness", "FSR Sharpness");
		ConfigCategory = TEXT("Graphics");
		DefaultValue = 0.0f;
		Value = 0.0f;
		Min = 0.0f;
		Max = 1.0f;
	}

	virtual void Apply_Implementation() override;
};


// ─────────────────────────────────────────────────────────────────────────────
//  XeSS Quality Mode
// ─────────────────────────────────────────────────────────────────────────────
UCLASS(Blueprintable, EditInlineNew, DisplayName = "Extended XeSS Quality Mode")
class UNREALEXTENDEDFRAMEWORK_API UEFUpscalerXeSSQualityModeSetting : public UEFModularSettingsMultiSelect
{
	GENERATED_BODY()

public:
	UEFUpscalerXeSSQualityModeSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.Upscaler.XeSS.QualityMode"));
		DisplayName = NSLOCTEXT("Settings", "XeSSQualityMode", "XeSS Quality Mode");
		ConfigCategory = TEXT("Graphics");
		DefaultValue = TEXT("Quality");

		// Maps to EXeSSQualityMode (1=UltraPerformance, 2=Performance, 3=Balanced, 4=Quality, 5=UltraQuality, 6=UltraQualityPlus, 7=AntiAliasing)
		Values = {
			TEXT("UltraPerformance"),
			TEXT("Performance"),
			TEXT("Balanced"),
			TEXT("Quality"),
			TEXT("UltraQuality"),
			TEXT("UltraQualityPlus"),
			TEXT("AntiAliasing")
		};

		DisplayNames = {
			NSLOCTEXT("Settings", "XeSSUltraPerf", "Ultra Performance"),
			NSLOCTEXT("Settings", "XeSSPerformance", "Performance"),
			NSLOCTEXT("Settings", "XeSSBalanced", "Balanced"),
			NSLOCTEXT("Settings", "XeSSQuality", "Quality"),
			NSLOCTEXT("Settings", "XeSSUltraQuality", "Ultra Quality"),
			NSLOCTEXT("Settings", "XeSSUltraQualityPlus", "Ultra Quality+"),
			NSLOCTEXT("Settings", "XeSSAntiAliasing", "Anti-Aliasing")
		};

		int32 DefaultIndex = Values.Find(DefaultValue);
		SelectedIndex = DefaultIndex != INDEX_NONE ? DefaultIndex : 3;
	}

	virtual void Apply_Implementation() override;

	/** Convert our 0-based index to vendor enum value (1-based for EXeSSQualityMode). */
	int32 GetXeSSQualityModeValue() const { return SelectedIndex + 1; }
};


// ─────────────────────────────────────────────────────────────────────────────
//  Frame Generation (FSR FFI / XeSS XeFG)
// ─────────────────────────────────────────────────────────────────────────────
UCLASS(Blueprintable, EditInlineNew, DisplayName = "Extended Frame Generation")
class UNREALEXTENDEDFRAMEWORK_API UEFUpscalerFrameGenSetting : public UEFModularSettingsBool
{
	GENERATED_BODY()

public:
	UEFUpscalerFrameGenSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.Upscaler.FrameGeneration"));
		DisplayName = NSLOCTEXT("Settings", "FrameGeneration", "Frame Generation");
		ConfigCategory = TEXT("Graphics");
		DefaultValue = false;
		Value = false;
	}

	virtual void Apply_Implementation() override;

	/** Check if frame generation is supported with the currently active upscaler. */
	UFUNCTION(BlueprintPure, Category = "FrameGen")
	bool IsFrameGenAvailable() const;
};


// ─────────────────────────────────────────────────────────────────────────────
//  Resolution Scale (r.ScreenPercentage — active only when upscaler is None)
// ─────────────────────────────────────────────────────────────────────────────
UCLASS(Blueprintable, EditInlineNew, DisplayName = "Extended Resolution Scale")
class UNREALEXTENDEDFRAMEWORK_API UEFUpscalerResolutionScaleSetting : public UEFModularSettingsFloat
{
	GENERATED_BODY()

public:
	UEFUpscalerResolutionScaleSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.Upscaler.ResolutionScale"));
		DisplayName = NSLOCTEXT("Settings", "ResolutionScale", "Resolution Scale");
		ConfigCategory = TEXT("Graphics");
		DefaultValue = 100.0f;
		Value = 100.0f;
		Min = 25.0f;
		Max = 200.0f;
	}

	virtual void Apply_Implementation() override;
};
