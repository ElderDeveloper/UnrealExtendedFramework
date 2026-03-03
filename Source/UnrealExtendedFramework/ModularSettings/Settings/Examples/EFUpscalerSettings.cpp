// EFUpscalerSettings.cpp
// Apply logic for upscaler settings (DLSS, FSR, XeSS, Frame Generation, Resolution Scale).
// All vendor API calls are behind compile-time guards defined in UnrealExtendedFramework.Build.cs.

#include "EFUpscalerSettings.h"


// ─────────────────────────────────────────────────────────────────────────────
//  Helper: Get or set r.ScreenPercentage CVar
// ─────────────────────────────────────────────────────────────────────────────
namespace EFUpscalerInternal
{
	static void SetScreenPercentage(float NewValue)
	{
		if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage")))
		{
			CVar->Set(NewValue);
		}
	}

	static float GetScreenPercentage()
	{
		if (const IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage")))
		{
			return CVar->GetFloat();
		}
		return 100.0f;
	}

	/** Get the active upscaler string from the subsystem (searches local settings). */
	static FString GetActiveUpscalerFromSubsystem(const UObject* WorldContext)
	{
		if (!WorldContext) return TEXT("None");

		UEFModularSettingsBase* Setting = UEFModularSettingsLibrary::GetModularSetting(
			WorldContext,
			FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.Upscaler")),
			EEFSettingsSource::Local
		);

		if (UEFModularSettingsMultiSelect* MultiSelect = Cast<UEFModularSettingsMultiSelect>(Setting))
		{
			if (MultiSelect->Values.IsValidIndex(MultiSelect->SelectedIndex))
			{
				return MultiSelect->Values[MultiSelect->SelectedIndex];
			}
		}
		return TEXT("None");
	}
}


// ─────────────────────────────────────────────────────────────────────────────
//  Upscaler Selection — Apply
// ─────────────────────────────────────────────────────────────────────────────
bool UEFUpscalerSelectSetting::IsUpscalerAvailable(const FString& UpscalerName)
{
#if WITH_DLSS
	if (UpscalerName == TEXT("DLSS"))
	{
		return UDLSSLibrary::IsDLSSSupported();
	}
#endif

#if WITH_FSR
	if (UpscalerName == TEXT("FSR"))
	{
		return true; // FSR is software-based, always available when the plugin is compiled in
	}
#endif

#if WITH_XESS
	if (UpscalerName == TEXT("XeSS"))
	{
		return UXeSSBlueprintLibrary::IsXeSSSupported();
	}
#endif

	if (UpscalerName == TEXT("None"))
	{
		return true;
	}

	return false;
}

void UEFUpscalerSelectSetting::Apply_Implementation()
{
	if (!Values.IsValidIndex(SelectedIndex))
	{
		return;
	}

	const FString SelectedUpscaler = Values[SelectedIndex];
	UE_LOG(LogTemp, Log, TEXT("Applying Upscaler: %s"), *SelectedUpscaler);

	// Step 1: Reset screen percentage to 100% (upscalers will set their own)
	EFUpscalerInternal::SetScreenPercentage(100.0f);

	// Step 2: Enable/disable each upscaler
#if WITH_DLSS
	if (UDLSSLibrary::IsDLSSSupported())
	{
		UDLSSLibrary::EnableDLSS(SelectedUpscaler == TEXT("DLSS"));
	}
#endif

#if WITH_FSR
	{
		// Enable/disable FSR via CVar
		IConsoleVariable* CVarEnableFSR = nullptr;
#if WITH_FSR_GENERIC
		CVarEnableFSR = IConsoleManager::Get().FindConsoleVariable(TEXT("r.FidelityFX.FSR.Enabled"));
#elif WITH_FSR4
		CVarEnableFSR = IConsoleManager::Get().FindConsoleVariable(TEXT("r.FidelityFX.FSR4.Enabled"));
#elif WITH_FSR3
		CVarEnableFSR = IConsoleManager::Get().FindConsoleVariable(TEXT("r.FidelityFX.FSR3.Enabled"));
#endif
		if (CVarEnableFSR)
		{
			CVarEnableFSR->Set(SelectedUpscaler == TEXT("FSR") ? 1 : 0);
		}
	}
#endif

#if WITH_XESS
	if (UXeSSBlueprintLibrary::IsXeSSSupported())
	{
		if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.XeSS.Enabled")))
		{
			CVar->Set(SelectedUpscaler == TEXT("XeSS") ? 1 : 0);
		}
	}
#endif

	// Step 3: Deferred apply of the active upscaler's sub-settings (next frame).
	// This ensures the upscaler is fully initialized before we configure it.
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this, SelectedUpscaler](float)
	{
		// Trigger sub-setting apply by looking them up from the subsystem
		if (SelectedUpscaler == TEXT("DLSS"))
		{
			if (auto* Setting = UEFModularSettingsLibrary::GetModularSetting(this,
				FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.Upscaler.DLSS.Mode")),
				EEFSettingsSource::Local))
			{
				Setting->Apply();
			}
		}
		else if (SelectedUpscaler == TEXT("FSR"))
		{
			if (auto* Setting = UEFModularSettingsLibrary::GetModularSetting(this,
				FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.Upscaler.FSR.Mode")),
				EEFSettingsSource::Local))
			{
				Setting->Apply();
			}
			if (auto* Setting = UEFModularSettingsLibrary::GetModularSetting(this,
				FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.Upscaler.FSR.Sharpness")),
				EEFSettingsSource::Local))
			{
				Setting->Apply();
			}
		}
		else if (SelectedUpscaler == TEXT("XeSS"))
		{
			if (auto* Setting = UEFModularSettingsLibrary::GetModularSetting(this,
				FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.Upscaler.XeSS.QualityMode")),
				EEFSettingsSource::Local))
			{
				Setting->Apply();
			}
		}
		else // "None"
		{
			// Apply manual resolution scale when no upscaler is active
			if (auto* Setting = UEFModularSettingsLibrary::GetModularSetting(this,
				FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.Upscaler.ResolutionScale")),
				EEFSettingsSource::Local))
			{
				Setting->Apply();
			}
		}

		// Always re-apply frame gen (it depends on active upscaler)
		if (auto* Setting = UEFModularSettingsLibrary::GetModularSetting(this,
			FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.Upscaler.FrameGeneration")),
			EEFSettingsSource::Local))
		{
			Setting->Apply();
		}

		return false; // Don't tick again
	}), 0.0f);
}


// ─────────────────────────────────────────────────────────────────────────────
//  DLSS Mode — Apply
// ─────────────────────────────────────────────────────────────────────────────
void UEFUpscalerDLSSModeSetting::Apply_Implementation()
{
#if WITH_DLSS
	const FString ActiveUpscaler = EFUpscalerInternal::GetActiveUpscalerFromSubsystem(this);
	if (ActiveUpscaler != TEXT("DLSS") || !UDLSSLibrary::IsDLSSSupported())
	{
		return;
	}

	const int32 DLSSModeValue = GetDLSSModeValue(); // 1-based
	const UDLSSMode DLSSPluginMode = static_cast<UDLSSMode>(DLSSModeValue);

	// Query optimal screen percentage from NVIDIA
	const FVector2D ScreenRes = FVector2D(
		GSystemResolution.ResX > 0 ? GSystemResolution.ResX : 1920,
		GSystemResolution.ResY > 0 ? GSystemResolution.ResY : 1080
	);

	bool bIsSupported = false;
	float OptimalScreenPercentage = 100.0f;
	bool bIsFixedScreenPercentage = false;
	float MinScreenPercentage = 0.0f, MaxScreenPercentage = 100.0f, OptimalSharpness = 0.0f;

	UDLSSLibrary::GetDLSSModeInformation(
		DLSSPluginMode, ScreenRes,
		bIsSupported, OptimalScreenPercentage, bIsFixedScreenPercentage,
		MinScreenPercentage, MaxScreenPercentage, OptimalSharpness
	);

	// DLAA uses 100% screen percentage (as per NVIDIA docs)
	if (DLSSPluginMode == UDLSSMode::DLAA)
	{
		OptimalScreenPercentage = 100.0f;
	}

	if (bIsSupported)
	{
		EFUpscalerInternal::SetScreenPercentage(OptimalScreenPercentage);
		UE_LOG(LogTemp, Log, TEXT("Applied DLSS Mode: %s (r.ScreenPercentage = %.1f)"),
			*Values[SelectedIndex], OptimalScreenPercentage);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("DLSS Mode %s is not supported on this hardware"), *Values[SelectedIndex]);
	}
#endif
}


// ─────────────────────────────────────────────────────────────────────────────
//  DLSS Ray Reconstruction — Apply
// ─────────────────────────────────────────────────────────────────────────────
void UEFUpscalerDLSSRRSetting::Apply_Implementation()
{
#if WITH_DLSS
	const FString ActiveUpscaler = EFUpscalerInternal::GetActiveUpscalerFromSubsystem(this);
	if (ActiveUpscaler != TEXT("DLSS"))
	{
		return;
	}

	if (Value && UDLSSLibrary::IsDLSSRRSupported())
	{
		UDLSSLibrary::EnableDLSSRR(true);
		UE_LOG(LogTemp, Log, TEXT("Enabled DLSS Ray Reconstruction"));
	}
	else
	{
		UDLSSLibrary::EnableDLSSRR(false);
		UE_LOG(LogTemp, Log, TEXT("Disabled DLSS Ray Reconstruction"));
	}
#endif
}

bool UEFUpscalerDLSSRRSetting::IsDLSSRayReconstructionAvailable()
{
#if WITH_DLSS
	return UDLSSLibrary::IsDLSSRRSupported();
#else
	return false;
#endif
}


// ─────────────────────────────────────────────────────────────────────────────
//  FSR Mode — Apply
// ─────────────────────────────────────────────────────────────────────────────
void UEFUpscalerFSRModeSetting::Apply_Implementation()
{
#if WITH_FSR
	const FString ActiveUpscaler = EFUpscalerInternal::GetActiveUpscalerFromSubsystem(this);
	if (ActiveUpscaler != TEXT("FSR"))
	{
		return;
	}

	const int32 QualityMode = SelectedIndex; // 0=NativeAA, 1=Quality, etc.

	// Set FSR quality mode CVar
	IConsoleVariable* CVarQualityMode = nullptr;
#if WITH_FSR_GENERIC
	CVarQualityMode = IConsoleManager::Get().FindConsoleVariable(TEXT("r.FidelityFX.FSR.QualityMode"));
#elif WITH_FSR4
	CVarQualityMode = IConsoleManager::Get().FindConsoleVariable(TEXT("r.FidelityFX.FSR4.QualityMode"));
#elif WITH_FSR3
	CVarQualityMode = IConsoleManager::Get().FindConsoleVariable(TEXT("r.FidelityFX.FSR3.QualityMode"));
#endif

	if (CVarQualityMode)
	{
		CVarQualityMode->Set(QualityMode);
		UE_LOG(LogTemp, Log, TEXT("Applied FSR Quality Mode: %s (CVar = %d, r.ScreenPercentage = %.1f)"),
			*Values[SelectedIndex], QualityMode, EFUpscalerInternal::GetScreenPercentage());
	}
#endif
}


// ─────────────────────────────────────────────────────────────────────────────
//  FSR Sharpness — Apply
// ─────────────────────────────────────────────────────────────────────────────
void UEFUpscalerFSRSharpnessSetting::Apply_Implementation()
{
#if WITH_FSR
	const FString ActiveUpscaler = EFUpscalerInternal::GetActiveUpscalerFromSubsystem(this);
	if (ActiveUpscaler != TEXT("FSR"))
	{
		return;
	}

	// FSR sharpness CVar — Blueshift's range was 0-20 but our UI is 0.0-1.0.
	// The CVar itself accepts 0.0+ (no upper limit), we map our 0-1 range directly.
	IConsoleVariable* CVarSharpness = nullptr;
#if WITH_FSR_GENERIC
	CVarSharpness = IConsoleManager::Get().FindConsoleVariable(TEXT("r.FidelityFX.FSR.Sharpness"));
#elif WITH_FSR4
	CVarSharpness = IConsoleManager::Get().FindConsoleVariable(TEXT("r.FidelityFX.FSR4.Sharpness"));
#elif WITH_FSR3
	CVarSharpness = IConsoleManager::Get().FindConsoleVariable(TEXT("r.FidelityFX.FSR3.Sharpness"));
#endif

	if (CVarSharpness)
	{
		CVarSharpness->Set(Value);
		UE_LOG(LogTemp, Log, TEXT("Applied FSR Sharpness: %.2f"), Value);
	}
#endif
}


// ─────────────────────────────────────────────────────────────────────────────
//  XeSS Quality Mode — Apply
// ─────────────────────────────────────────────────────────────────────────────
void UEFUpscalerXeSSQualityModeSetting::Apply_Implementation()
{
#if WITH_XESS
	const FString ActiveUpscaler = EFUpscalerInternal::GetActiveUpscalerFromSubsystem(this);
	if (ActiveUpscaler != TEXT("XeSS") || !UXeSSBlueprintLibrary::IsXeSSSupported())
	{
		return;
	}

	const int32 QualityModeValue = GetXeSSQualityModeValue(); // 1-based
	const EXeSSQualityMode PluginMode = static_cast<EXeSSQualityMode>(QualityModeValue);

	UXeSSBlueprintLibrary::SetXeSSQualityMode(PluginMode);
	UE_LOG(LogTemp, Log, TEXT("Applied XeSS Quality Mode: %s (r.ScreenPercentage = %.1f)"),
		*Values[SelectedIndex], EFUpscalerInternal::GetScreenPercentage());
#endif
}


// ─────────────────────────────────────────────────────────────────────────────
//  Frame Generation — Apply
// ─────────────────────────────────────────────────────────────────────────────
void UEFUpscalerFrameGenSetting::Apply_Implementation()
{
	const FString ActiveUpscaler = EFUpscalerInternal::GetActiveUpscalerFromSubsystem(this);

	// FSR Frame Interpolation
#if WITH_FSR
	if (ActiveUpscaler == TEXT("FSR"))
	{
#if WITH_FSR4 || WITH_FSR3
		if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.FidelityFX.FI.Enabled")))
		{
			CVar->Set(Value ? 1 : 0);
			UE_LOG(LogTemp, Log, TEXT("FSR Frame Interpolation: %s"), Value ? TEXT("Enabled") : TEXT("Disabled"));
		}
#endif
	}
#endif

	// XeSS Frame Generation
#if WITH_XESS
	if (ActiveUpscaler == TEXT("XeSS"))
	{
		if (UXeFGBlueprintLibrary::IsXeFGSupported())
		{
			UXeFGBlueprintLibrary::SetXeFGMode(Value ? EXeFGMode::On : EXeFGMode::Off);
			UE_LOG(LogTemp, Log, TEXT("XeSS Frame Generation: %s"), Value ? TEXT("Enabled") : TEXT("Disabled"));
		}
	}
#endif

	// DLSS: Frame Generation is automatic and tied to the DLSS version — no manual toggle available
	if (ActiveUpscaler == TEXT("DLSS"))
	{
		UE_LOG(LogTemp, Verbose, TEXT("DLSS Frame Generation is automatic — no manual toggle"));
	}
}

bool UEFUpscalerFrameGenSetting::IsFrameGenAvailable() const
{
	const FString ActiveUpscaler = EFUpscalerInternal::GetActiveUpscalerFromSubsystem(this);

#if WITH_FSR
	if (ActiveUpscaler == TEXT("FSR"))
	{
		return true; // FFI is software, always available with FSR
	}
#endif

#if WITH_XESS
	if (ActiveUpscaler == TEXT("XeSS"))
	{
		return UXeFGBlueprintLibrary::IsXeFGSupported();
	}
#endif

	// DLSS frame gen is automatic, not player-toggleable
	return false;
}


// ─────────────────────────────────────────────────────────────────────────────
//  Resolution Scale — Apply
// ─────────────────────────────────────────────────────────────────────────────
void UEFUpscalerResolutionScaleSetting::Apply_Implementation()
{
	const FString ActiveUpscaler = EFUpscalerInternal::GetActiveUpscalerFromSubsystem(this);

	// Only apply manual resolution scale when no upscaler is active.
	// Upscalers manage r.ScreenPercentage themselves.
	if (ActiveUpscaler != TEXT("None"))
	{
		UE_LOG(LogTemp, Verbose, TEXT("Resolution Scale ignored — upscaler '%s' manages r.ScreenPercentage"), *ActiveUpscaler);
		return;
	}

	EFUpscalerInternal::SetScreenPercentage(Value);
	UE_LOG(LogTemp, Log, TEXT("Applied Resolution Scale: %.0f%%"), Value);
}
