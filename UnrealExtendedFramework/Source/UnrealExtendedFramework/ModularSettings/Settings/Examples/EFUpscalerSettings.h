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

#include "Internationalization/StringTableRegistry.h"
#include "EFUpscalerSettings.generated.h"


// ─────────────────────────────────────────────────────────────────────────────
//  Upscaler Selection (DLSS / FSR / XeSS)
// ─────────────────────────────────────────────────────────────────────────────
UCLASS(Blueprintable, EditInlineNew, DisplayName = "Extended Upscaler Selection")
class UNREALEXTENDEDFRAMEWORK_API UEFUpscalerSelectSetting : public UEFModularSettingsMultiSelect
{
	GENERATED_BODY()

public:
	UEFUpscalerSelectSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.Upscaler"));
		DisplayName = LOCTABLE("ExtendedFramework", "Settings.Upscaler");
		ConfigCategory = TEXT("Graphics");
		DefaultValue = TEXT("");

		Values.Add(TEXT("None"));
		DisplayNames.Add(LOCTABLE("ExtendedFramework", "Common.None"));

#if WITH_DLSS
		Values.Add(TEXT("DLSS"));
		DisplayNames.Add(LOCTABLE("ExtendedFramework", "Settings.UpscalerDLSS"));
		if (DefaultValue.IsEmpty())
		{
			DefaultValue = TEXT("DLSS");
		}
#endif

#if WITH_FSR
		Values.Add(TEXT("FSR"));
		DisplayNames.Add(LOCTABLE("ExtendedFramework", "Settings.UpscalerFSR"));
		if (DefaultValue.IsEmpty())
		{
			DefaultValue = TEXT("FSR");
		}
#endif

#if WITH_XESS
		Values.Add(TEXT("XeSS"));
		DisplayNames.Add(LOCTABLE("ExtendedFramework", "Settings.UpscalerXeSS"));
		if (DefaultValue.IsEmpty())
		{
			DefaultValue = TEXT("XeSS");
		}
#endif

		if (DefaultValue.IsEmpty())
		{
			DefaultValue = TEXT("None");
		}

		const int32 DefaultIndex = Values.Find(DefaultValue);
		SelectedIndex = DefaultIndex != INDEX_NONE ? DefaultIndex : 0;
	}

	virtual void Apply_Implementation() override;
	virtual void SetValueFromString(const FString& Value) override;

	/** An upscaler this machine cannot run is not offered (DLSS on AMD, XeSS without support). */
	virtual bool IsOptionSupported(const FString& OptionValue) const override;

	/** The first upscaler, in option order, that this machine supports. */
	virtual FString ComputeDeviceDefault() const override;

	/** Returns the currently selected upscaler key (DLSS/FSR/XeSS). Returns "None" on invalid index. */
	UFUNCTION(BlueprintPure, Category = "Upscaler")
	FString GetActiveUpscaler() const
	{
		return Values.IsValidIndex(SelectedIndex) ? Values[SelectedIndex] : TEXT("None");
	}

	/** Check if a specific upscaler is available on this hardware at runtime. */
	UFUNCTION(BlueprintPure, Category = "Upscaler")
	static bool IsUpscalerAvailable(const FString& UpscalerName);

private:
	/** Set while an Apply made before the vendor modules loaded is waiting for the PostEngineInit phase. */
	FDelegateHandle UpscalerModulesLoadedHandle;
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
		DisplayName = LOCTABLE("ExtendedFramework", "Settings.DLSSMode");
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
			LOCTABLE("ExtendedFramework", "Settings.DLSSAuto"),
			LOCTABLE("ExtendedFramework", "Settings.DLSSDLAA"),
			LOCTABLE("ExtendedFramework", "Common.UltraQuality"),
			LOCTABLE("ExtendedFramework", "Common.Quality"),
			LOCTABLE("ExtendedFramework", "Common.Balanced"),
			LOCTABLE("ExtendedFramework", "Common.Performance"),
			LOCTABLE("ExtendedFramework", "Common.UltraPerformance")
		};

		SelectedIndex = 0;
	}

	virtual void Apply_Implementation() override;

	/** Hidden on a machine without DLSS. */
	virtual bool IsSupportedOnThisMachine() const override;

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
		DisplayName = LOCTABLE("ExtendedFramework", "Settings.DLSSRR");
		ConfigCategory = TEXT("Graphics");
		DefaultValue = false;
		Value = false;
	}

	virtual void Apply_Implementation() override;

	/** Hidden on a machine without DLSS. This switch also turns DLSS itself on, so it stays while DLSS works. */
	virtual bool IsSupportedOnThisMachine() const override;

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
		DisplayName = LOCTABLE("ExtendedFramework", "Settings.FSRMode");
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
			LOCTABLE("ExtendedFramework", "Settings.FSRNativeAA"),
			LOCTABLE("ExtendedFramework", "Common.Quality"),
			LOCTABLE("ExtendedFramework", "Common.Balanced"),
			LOCTABLE("ExtendedFramework", "Common.Performance"),
			LOCTABLE("ExtendedFramework", "Common.UltraPerformance")
		};

		int32 DefaultIndex = Values.Find(DefaultValue);
		SelectedIndex = DefaultIndex != INDEX_NONE ? DefaultIndex : 1;
	}

	virtual void Apply_Implementation() override;

	/** Hidden when the build has no FSR. */
	virtual bool IsSupportedOnThisMachine() const override;
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
		DisplayName = LOCTABLE("ExtendedFramework", "Settings.FSRSharpness");
		ConfigCategory = TEXT("Graphics");
		DefaultValue = 0.0f;
		Value = 0.0f;
		Min = 0.0f;
		Max = 1.0f;
	}

	virtual void Apply_Implementation() override;

	/** Hidden when the build has no FSR. */
	virtual bool IsSupportedOnThisMachine() const override;
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
		DisplayName = LOCTABLE("ExtendedFramework", "Settings.XeSSQualityMode");
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
			LOCTABLE("ExtendedFramework", "Common.UltraPerformance"),
			LOCTABLE("ExtendedFramework", "Common.Performance"),
			LOCTABLE("ExtendedFramework", "Common.Balanced"),
			LOCTABLE("ExtendedFramework", "Common.Quality"),
			LOCTABLE("ExtendedFramework", "Common.UltraQuality"),
			LOCTABLE("ExtendedFramework", "Settings.XeSSUltraQualityPlus"),
			LOCTABLE("ExtendedFramework", "Settings.XeSSAntiAliasing")
		};

		int32 DefaultIndex = Values.Find(DefaultValue);
		SelectedIndex = DefaultIndex != INDEX_NONE ? DefaultIndex : 3;
	}

	virtual void Apply_Implementation() override;

	/** Hidden on a machine without XeSS. */
	virtual bool IsSupportedOnThisMachine() const override;

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
		DisplayName = LOCTABLE("ExtendedFramework", "Settings.FrameGeneration");
		ConfigCategory = TEXT("Graphics");
		DefaultValue = false;
		Value = false;
	}

	virtual void Apply_Implementation() override;

	/** Hidden when no upscaler on this machine can generate frames (FSR, or XeSS with XeFG). */
	virtual bool IsSupportedOnThisMachine() const override;

	/** Check if frame generation is supported with the currently active upscaler. */
	UFUNCTION(BlueprintPure, Category = "FrameGen")
	bool IsFrameGenAvailable() const;
};


// ─────────────────────────────────────────────────────────────────────────────
//  Resolution Scale (r.ScreenPercentage — active only when no valid upscaler is selected)
// ─────────────────────────────────────────────────────────────────────────────
UCLASS(Blueprintable, EditInlineNew, DisplayName = "Extended Resolution Scale")
class UNREALEXTENDEDFRAMEWORK_API UEFUpscalerResolutionScaleSetting : public UEFModularSettingsFloat
{
	GENERATED_BODY()

public:
	UEFUpscalerResolutionScaleSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.Upscaler.ResolutionScale"));
		DisplayName = LOCTABLE("ExtendedFramework", "Settings.ResolutionScale");
		ConfigCategory = TEXT("Graphics");
		DefaultValue = 100.0f;
		Value = 100.0f;
		Min = 25.0f;
		Max = 200.0f;
	}

	virtual void Apply_Implementation() override;
};
