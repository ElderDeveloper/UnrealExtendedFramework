// Fill out your copyright notice in the Description page of Project Settings.

#include "EFModularSettingsSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "Misc/DefaultValueHelper.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "SaveGame/EFSettingsSaveGame.h"
#include "Settings/EFModularSettingsBase.h"
#include "Settings/Examples/EFGraphicsSettings.h"
#include "Async/Async.h"
#include "GameFramework/GameUserSettings.h"
#include "UnrealExtendedFramework/Libraries/Monitor/EFMonitorLibrary.h"

static const FString SettingsSaveSlotName = TEXT("ModularSettingsSave");
static const int32 SettingsUserIndex = 0;

namespace EFModularSettingsConsole
{
	static FString JoinArgs(const TArray<FString>& Args, const int32 StartIndex = 0)
	{
		FString JoinedValue;
		for (int32 Index = StartIndex; Index < Args.Num(); ++Index)
		{
			if (!JoinedValue.IsEmpty())
			{
				JoinedValue += TEXT(" ");
			}

			JoinedValue += Args[Index];
		}

		return JoinedValue;
	}

	static bool TryParseBool(const FString& ValueString, bool& bOutValue)
	{
		if (ValueString.Equals(TEXT("true"), ESearchCase::IgnoreCase)
			|| ValueString.Equals(TEXT("1"), ESearchCase::IgnoreCase)
			|| ValueString.Equals(TEXT("on"), ESearchCase::IgnoreCase)
			|| ValueString.Equals(TEXT("yes"), ESearchCase::IgnoreCase)
			|| ValueString.Equals(TEXT("enable"), ESearchCase::IgnoreCase)
			|| ValueString.Equals(TEXT("enabled"), ESearchCase::IgnoreCase))
		{
			bOutValue = true;
			return true;
		}

		if (ValueString.Equals(TEXT("false"), ESearchCase::IgnoreCase)
			|| ValueString.Equals(TEXT("0"), ESearchCase::IgnoreCase)
			|| ValueString.Equals(TEXT("off"), ESearchCase::IgnoreCase)
			|| ValueString.Equals(TEXT("no"), ESearchCase::IgnoreCase)
			|| ValueString.Equals(TEXT("disable"), ESearchCase::IgnoreCase)
			|| ValueString.Equals(TEXT("disabled"), ESearchCase::IgnoreCase))
		{
			bOutValue = false;
			return true;
		}

		return false;
	}

	static FString DescribeOptions(const UEFModularSettingsMultiSelect* Setting)
	{
		return Setting && Setting->Values.Num() > 0
			? FString::Join(Setting->Values, TEXT(", "))
			: TEXT("<none>");
	}

	static FGameplayTag MakeTag(const TCHAR* TagName)
	{
		return FGameplayTag::RequestGameplayTag(FName(TagName), false);
	}

	static bool TryParseAspectRatio(const FString& ValueString, int32& OutWidthPart, int32& OutHeightPart)
	{
		FString LeftPart;
		FString RightPart;
		if (!ValueString.Split(TEXT(":"), &LeftPart, &RightPart) && !ValueString.Split(TEXT("/"), &LeftPart, &RightPart))
		{
			return false;
		}

		OutWidthPart = FCString::Atoi(*LeftPart);
		OutHeightPart = FCString::Atoi(*RightPart);
		return OutWidthPart > 0 && OutHeightPart > 0;
	}

	static bool TryParseWindowMode(const FString& ValueString, EWindowMode::Type& OutWindowMode, FString& OutModeName)
	{
		if (ValueString.Equals(TEXT("windowed"), ESearchCase::IgnoreCase))
		{
			OutWindowMode = EWindowMode::Windowed;
			OutModeName = TEXT("Windowed");
			return true;
		}

		if (ValueString.Equals(TEXT("borderless"), ESearchCase::IgnoreCase)
			|| ValueString.Equals(TEXT("windowedfullscreen"), ESearchCase::IgnoreCase))
		{
			OutWindowMode = EWindowMode::WindowedFullscreen;
			OutModeName = TEXT("Borderless");
			return true;
		}

		if (ValueString.Equals(TEXT("fullscreen"), ESearchCase::IgnoreCase))
		{
			OutWindowMode = EWindowMode::Fullscreen;
			OutModeName = TEXT("Fullscreen");
			return true;
		}

		return false;
	}

	static int32 AlignDimensionForAspectTest(const int32 Value)
	{
		return Value > 1 ? Value - (Value % 2) : Value;
	}

	static FIntPoint ComputeLargestFittingResolution(const int32 MaxWidth, const int32 MaxHeight, const float TargetAspectRatio)
	{
		if (MaxWidth <= 0 || MaxHeight <= 0 || TargetAspectRatio <= 0.0f)
		{
			return FIntPoint::ZeroValue;
		}

		const int32 WidthLimitedHeight = AlignDimensionForAspectTest(FMath::FloorToInt(static_cast<float>(MaxWidth) / TargetAspectRatio));
		const int32 HeightLimitedWidth = AlignDimensionForAspectTest(FMath::FloorToInt(static_cast<float>(MaxHeight) * TargetAspectRatio));

		FIntPoint BestResolution = FIntPoint::ZeroValue;
		int64 BestPixelCount = -1;

		auto TryCandidate = [&](const int32 CandidateWidth, const int32 CandidateHeight)
		{
			if (CandidateWidth <= 0 || CandidateHeight <= 0)
			{
				return;
			}

			if (CandidateWidth > MaxWidth || CandidateHeight > MaxHeight)
			{
				return;
			}

			const int64 PixelCount = static_cast<int64>(CandidateWidth) * static_cast<int64>(CandidateHeight);
			if (PixelCount > BestPixelCount)
			{
				BestResolution = FIntPoint(CandidateWidth, CandidateHeight);
				BestPixelCount = PixelCount;
			}
		};

		TryCandidate(MaxWidth, WidthLimitedHeight);
		TryCandidate(HeightLimitedWidth, MaxHeight);
		return BestResolution;
	}
}

namespace
{
	static bool ShouldSkipHardwareBenchmarkInPIE(const UObject* WorldContextObject)
	{
#if WITH_EDITOR
		if (GIsEditor)
		{
			const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
			return World && World->WorldType == EWorldType::PIE;
		}
#endif

		return false;
	}
}

void UEFModularSettingsSubsystem::Initialize(FSubsystemCollectionBase& SubsystemCollectionBase)
{
	Super::Initialize(SubsystemCollectionBase);

	if (const UEFModularProjectSettings* ProjectSettings = GetDefault<UEFModularProjectSettings>())
	{
		for (const TSoftObjectPtr<UEFModularSettingsContainer>& ContainerPtr : ProjectSettings->LocalSettingsContainers)
		{
			if (UEFModularSettingsContainer* Container = ContainerPtr.LoadSynchronous())
			{
				for (UEFModularSettingsBase* Setting : Container->Settings)
				{
					if (Setting)
					{
						RegisterSetting(Setting);
					}
				}
			}
		}
	}

	LoadFromDisk();

	// Force apply all settings to ensure engine state matches loaded settings
	for (const auto& SettingPair : Settings)
	{
		if (UEFModularSettingsBase* Setting = SettingPair.Value)
		{
			// Ensure the "PreviousValue" matches what we just loaded
			Setting->SaveCurrentValue();
			Setting->Apply();
			Setting->ClearDirty();
		}
	}

	RegisterConsoleCommands();
}


void UEFModularSettingsSubsystem::Deinitialize()
{
	UnregisterConsoleCommands();
	Super::Deinitialize();
}


void UEFModularSettingsSubsystem::RegisterConsoleCommands()
{
	RegisterConsoleCommand(
		TEXT("Settings.Set"),
		TEXT("Set a modular setting value. Usage: Settings.Set <Tag> <Value>"),
		FConsoleCommandWithArgsDelegate::CreateUObject(this, &UEFModularSettingsSubsystem::HandleSetCommand));

	RegisterConsoleCommand(
		TEXT("Settings.Upscaler.Set"),
		TEXT("Set the active upscaler. Usage: Settings.Upscaler.Set <None|DLSS|FSR|XeSS>"),
		FConsoleCommandWithArgsDelegate::CreateUObject(this, &UEFModularSettingsSubsystem::HandleUpscalerSetCommand));

	RegisterConsoleCommand(
		TEXT("Settings.Upscaler.DLSS.Mode"),
		TEXT("Set the DLSS quality mode. Usage: Settings.Upscaler.DLSS.Mode <Auto|DLAA|UltraQuality|Quality|Balanced|Performance|UltraPerformance>"),
		FConsoleCommandWithArgsDelegate::CreateUObject(this, &UEFModularSettingsSubsystem::HandleUpscalerDLSSModeCommand));

	RegisterConsoleCommand(
		TEXT("Settings.Upscaler.DLSS.RayReconstruction"),
		TEXT("Enable or disable DLSS ray reconstruction. Usage: Settings.Upscaler.DLSS.RayReconstruction <true|false>"),
		FConsoleCommandWithArgsDelegate::CreateUObject(this, &UEFModularSettingsSubsystem::HandleUpscalerDLSSRayReconstructionCommand));

	RegisterConsoleCommand(
		TEXT("Settings.Upscaler.FSR.Mode"),
		TEXT("Set the FSR quality mode. Usage: Settings.Upscaler.FSR.Mode <NativeAA|Quality|Balanced|Performance|UltraPerformance>"),
		FConsoleCommandWithArgsDelegate::CreateUObject(this, &UEFModularSettingsSubsystem::HandleUpscalerFSRModeCommand));

	RegisterConsoleCommand(
		TEXT("Settings.Upscaler.FSR.Sharpness"),
		TEXT("Set the FSR sharpness. Usage: Settings.Upscaler.FSR.Sharpness <0.0-1.0>"),
		FConsoleCommandWithArgsDelegate::CreateUObject(this, &UEFModularSettingsSubsystem::HandleUpscalerFSRSharpnessCommand));

	RegisterConsoleCommand(
		TEXT("Settings.Upscaler.XeSS.QualityMode"),
		TEXT("Set the XeSS quality mode. Usage: Settings.Upscaler.XeSS.QualityMode <UltraPerformance|Performance|Balanced|Quality|UltraQuality|UltraQualityPlus|AntiAliasing>"),
		FConsoleCommandWithArgsDelegate::CreateUObject(this, &UEFModularSettingsSubsystem::HandleUpscalerXeSSQualityModeCommand));

	RegisterConsoleCommand(
		TEXT("Settings.Upscaler.FrameGeneration"),
		TEXT("Enable or disable frame generation. Usage: Settings.Upscaler.FrameGeneration <true|false>"),
		FConsoleCommandWithArgsDelegate::CreateUObject(this, &UEFModularSettingsSubsystem::HandleUpscalerFrameGenerationCommand));

	RegisterConsoleCommand(
		TEXT("Settings.Upscaler.ResolutionScale"),
		TEXT("Set the manual resolution scale. Usage: Settings.Upscaler.ResolutionScale <25-200>"),
		FConsoleCommandWithArgsDelegate::CreateUObject(this, &UEFModularSettingsSubsystem::HandleUpscalerResolutionScaleCommand));

	RegisterConsoleCommand(
		TEXT("Settings.Upscaler.Status"),
		TEXT("Log the current upscaler-related modular settings."),
		FConsoleCommandWithArgsDelegate::CreateUObject(this, &UEFModularSettingsSubsystem::HandleUpscalerStatusCommand));

	RegisterConsoleCommand(
		TEXT("Settings.Display.TestAspect"),
		TEXT("Apply the largest resolution that fits the current monitor for a target aspect ratio. Usage: Settings.Display.TestAspect <16:9|19:9|21:9|32:9|Native> [Windowed|Fullscreen|Borderless]"),
		FConsoleCommandWithArgsDelegate::CreateUObject(this, &UEFModularSettingsSubsystem::HandleDisplayTestAspectCommand));
}


void UEFModularSettingsSubsystem::UnregisterConsoleCommands()
{
	IConsoleManager& ConsoleManager = IConsoleManager::Get();
	for (const FString& CommandName : RegisteredConsoleCommands)
	{
		if (ConsoleManager.FindConsoleObject(*CommandName) != nullptr)
		{
			ConsoleManager.UnregisterConsoleObject(*CommandName, false);
		}
	}

	RegisteredConsoleCommands.Reset();
}


void UEFModularSettingsSubsystem::RegisterConsoleCommand(const TCHAR* Name, const TCHAR* Help, const FConsoleCommandWithArgsDelegate& Delegate)
{
	IConsoleManager& ConsoleManager = IConsoleManager::Get();
	if (ConsoleManager.FindConsoleObject(Name) != nullptr)
	{
		ConsoleManager.UnregisterConsoleObject(Name, false);
	}

	ConsoleManager.RegisterConsoleCommand(Name, Help, Delegate, ECVF_Default);
	RegisteredConsoleCommands.AddUnique(Name);
}


bool UEFModularSettingsSubsystem::ApplyConsoleSettingValue(FGameplayTag Tag, const FString& ValueString, const TCHAR* CommandLabel)
{
	UEFModularSettingsBase* Setting = GetSettingByTag(Tag);
	if (!Setting)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s could not find setting '%s'"), CommandLabel, *Tag.ToString());
		return false;
	}

	const FString TrimmedValue = ValueString.TrimStartAndEnd();
	if (TrimmedValue.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("%s requires a non-empty value for '%s'"), CommandLabel, *Tag.ToString());
		return false;
	}

	if (UEFModularSettingsMultiSelect* MultiSelectSetting = Cast<UEFModularSettingsMultiSelect>(Setting))
	{
		const int32 ValueIndex = MultiSelectSetting->Values.IndexOfByPredicate([&TrimmedValue](const FString& Candidate)
		{
			return Candidate.Equals(TrimmedValue, ESearchCase::IgnoreCase);
		});

		if (!MultiSelectSetting->Values.IsValidIndex(ValueIndex))
		{
			UE_LOG(LogTemp, Warning, TEXT("%s invalid value '%s' for '%s'. Valid options: %s"),
				CommandLabel,
				*TrimmedValue,
				*Tag.ToString(),
				*EFModularSettingsConsole::DescribeOptions(MultiSelectSetting));
			return false;
		}

		if (MultiSelectSetting->IsIndexLocked(ValueIndex))
		{
			UE_LOG(LogTemp, Warning, TEXT("%s option '%s' is currently locked for '%s'"), CommandLabel, *MultiSelectSetting->Values[ValueIndex], *Tag.ToString());
			return false;
		}

		MultiSelectSetting->SetSelectedIndex(ValueIndex);
	}
	else if (UEFModularSettingsBool* BoolSetting = Cast<UEFModularSettingsBool>(Setting))
	{
		bool bParsedValue = false;
		if (!EFModularSettingsConsole::TryParseBool(TrimmedValue, bParsedValue))
		{
			UE_LOG(LogTemp, Warning, TEXT("%s invalid bool value '%s' for '%s'. Expected one of: true, false, 1, 0, on, off"),
				CommandLabel,
				*TrimmedValue,
				*Tag.ToString());
			return false;
		}

		BoolSetting->SetValue(bParsedValue);
	}
	else if (UEFModularSettingsFloat* FloatSetting = Cast<UEFModularSettingsFloat>(Setting))
	{
		float ParsedValue = 0.0f;
		if (!FDefaultValueHelper::ParseFloat(TrimmedValue, ParsedValue))
		{
			UE_LOG(LogTemp, Warning, TEXT("%s invalid float value '%s' for '%s'"), CommandLabel, *TrimmedValue, *Tag.ToString());
			return false;
		}

		if (ParsedValue < FloatSetting->Min || ParsedValue > FloatSetting->Max)
		{
			UE_LOG(LogTemp, Warning, TEXT("%s value %.3f is out of range for '%s'. Expected %.3f to %.3f"),
				CommandLabel,
				ParsedValue,
				*Tag.ToString(),
				FloatSetting->Min,
				FloatSetting->Max);
			return false;
		}

		FloatSetting->SetValue(ParsedValue);
	}
	else
	{
		if (!Setting->Validate(TrimmedValue))
		{
			UE_LOG(LogTemp, Warning, TEXT("%s rejected value '%s' for '%s'"), CommandLabel, *TrimmedValue, *Tag.ToString());
			return false;
		}

		Setting->SetValueFromString(TrimmedValue);
	}

	Setting->Apply();
	Setting->SaveCurrentValue();
	Setting->ClearDirty();
	OnSettingsChanged.Broadcast(Setting);
	SaveToDisk();

	UE_LOG(LogTemp, Log, TEXT("%s set %s to %s"), CommandLabel, *Tag.ToString(), *Setting->GetValueAsString());
	return true;
}


void UEFModularSettingsSubsystem::LogSettingValue(FGameplayTag Tag, const TCHAR* Label) const
{
	const UEFModularSettingsBase* Setting = GetSettingByTag(Tag);
	if (!Setting)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: setting '%s' is not registered"), Label, *Tag.ToString());
		return;
	}

	if (const UEFModularSettingsMultiSelect* MultiSelectSetting = Cast<UEFModularSettingsMultiSelect>(Setting))
	{
		UE_LOG(LogTemp, Log, TEXT("%s: %s [Options: %s]"), Label, *MultiSelectSetting->GetValueAsString(), *EFModularSettingsConsole::DescribeOptions(MultiSelectSetting));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("%s: %s"), Label, *Setting->GetValueAsString());
}


void UEFModularSettingsSubsystem::LogUpscalerStatus() const
{
	LogSettingValue(EFModularSettingsConsole::MakeTag(TEXT("Settings.Graphics.Upscaler")), TEXT("Upscaler"));
	LogSettingValue(EFModularSettingsConsole::MakeTag(TEXT("Settings.Graphics.Upscaler.DLSS.Mode")), TEXT("DLSS Mode"));
	LogSettingValue(EFModularSettingsConsole::MakeTag(TEXT("Settings.Graphics.Upscaler.DLSS.RayReconstruction")), TEXT("DLSS Ray Reconstruction"));
	LogSettingValue(EFModularSettingsConsole::MakeTag(TEXT("Settings.Graphics.Upscaler.FSR.Mode")), TEXT("FSR Mode"));
	LogSettingValue(EFModularSettingsConsole::MakeTag(TEXT("Settings.Graphics.Upscaler.FSR.Sharpness")), TEXT("FSR Sharpness"));
	LogSettingValue(EFModularSettingsConsole::MakeTag(TEXT("Settings.Graphics.Upscaler.XeSS.QualityMode")), TEXT("XeSS Quality Mode"));
	LogSettingValue(EFModularSettingsConsole::MakeTag(TEXT("Settings.Graphics.Upscaler.FrameGeneration")), TEXT("Frame Generation"));
	LogSettingValue(EFModularSettingsConsole::MakeTag(TEXT("Settings.Graphics.Upscaler.ResolutionScale")), TEXT("Resolution Scale"));
}



void UEFModularSettingsSubsystem::ApplyAllChanges()
{
	bool bAnyChanged = false;

	// Populate snapshot BEFORE applying changes so we can revert to the state just before this apply
	PreviousSettingsSnapshot.Empty();
	for (const auto& SettingPair : Settings)
	{
		UEFModularSettingsBase* Setting = SettingPair.Value;
		if (Setting)
		{
			PreviousSettingsSnapshot.Add(Setting->SettingTag, Setting->GetSavedValueAsString());
		}
	}

	for (const auto& SettingPair : Settings)
	{
		UEFModularSettingsBase* Setting = SettingPair.Value;
		if (Setting->IsDirty())
		{
			Setting->SaveCurrentValue();
			Setting->Apply();
			Setting->ClearDirty();
			bAnyChanged = true;
			OnSettingsChanged.Broadcast(Setting);
		}
	}
    
	if (bAnyChanged)
	{
		SaveToDisk();
		UE_LOG(LogTemp, Log, TEXT("Applied and saved changed settings. Snapshot created."));
	}
}


void UEFModularSettingsSubsystem::RevertPendingChanges()
{
	for (const auto& SettingPair : Settings)
	{
		UEFModularSettingsBase* Setting = SettingPair.Value;
		Setting->RevertToSavedValue();
		OnSettingsChanged.Broadcast(Setting);
	}
    
	UE_LOG(LogTemp, Log, TEXT("All unapplied pending settings reverted to saved values."));
}


void UEFModularSettingsSubsystem::RevertToPreviousSettings()
{
	if (PreviousSettingsSnapshot.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("No previous settings snapshot exists to revert to."));
		return;
	}

	for (const auto& SnapshotPair : PreviousSettingsSnapshot)
	{
		if (UEFModularSettingsBase* Setting = Settings.FindRef(SnapshotPair.Key))
		{
			Setting->SetValueFromString(SnapshotPair.Value);
			Setting->SaveCurrentValue();
			Setting->Apply();
			
			// Broadcast change so UI updates
			OnSettingsChanged.Broadcast(Setting);
		}
	}
	
	SaveToDisk();
	UE_LOG(LogTemp, Log, TEXT("Reverted to previous settings snapshot and saved."));
}


void UEFModularSettingsSubsystem::RefreshAllSettings()
{
	for (const auto& SettingPair : Settings)
	{
		UEFModularSettingsBase* Setting = SettingPair.Value;
		Setting->RefreshValues();
		OnSettingsChanged.Broadcast(Setting);
	}
}


bool UEFModularSettingsSubsystem::HasPendingChanges() const
{
	for (const auto& SettingPair : Settings)
	{
		const UEFModularSettingsBase* Setting = SettingPair.Value;
		if (Setting->IsDirty())
		{
			return true;
		}
	}
	return false;
}


void UEFModularSettingsSubsystem::ResetToDefaults(FGameplayTag CategoryTag)
{
	for (const auto& SettingPair : Settings)
	{
		UEFModularSettingsBase* Setting = SettingPair.Value;
		Setting->ResetToDefault();
		OnSettingsChanged.Broadcast(Setting);
	}
	
	ApplyAllChanges(); // Apply and save
	UE_LOG(LogTemp, Log, TEXT("Settings reset to defaults."));
}


void UEFModularSettingsSubsystem::SaveToDisk()
{
	if (UEFSettingsSaveGame* SaveGameInstance = Cast<UEFSettingsSaveGame>(UGameplayStatics::CreateSaveGameObject(UEFSettingsSaveGame::StaticClass())))
	{
		for (const auto& SettingPair : Settings)
		{
			UEFModularSettingsBase* Setting = SettingPair.Value;
			SaveGameInstance->StoredSettings.Add(Setting->SettingTag, Setting->GetValueAsString());
		}

		UGameplayStatics::SaveGameToSlot(SaveGameInstance, SettingsSaveSlotName, SettingsUserIndex);
		UE_LOG(LogTemp, Log, TEXT("Modular settings saved to slot: %s"), *SettingsSaveSlotName);
	}
}


void UEFModularSettingsSubsystem::LoadFromDisk()
{
	if (UGameplayStatics::DoesSaveGameExist(SettingsSaveSlotName, SettingsUserIndex))
	{
		if (UEFSettingsSaveGame* LoadedGame = Cast<UEFSettingsSaveGame>(UGameplayStatics::LoadGameFromSlot(SettingsSaveSlotName, SettingsUserIndex)))
		{
			for (const auto& StoredPair : LoadedGame->StoredSettings)
			{
				if (UEFModularSettingsBase* Setting = Settings.FindRef(StoredPair.Key))
				{
					Setting->SetValueFromString(StoredPair.Value);
				}
			}

			UE_LOG(LogTemp, Log, TEXT("Modular settings loaded from slot: %s"), *SettingsSaveSlotName);
		}
	}
	else
	{
		if (ShouldSkipHardwareBenchmarkInPIE(this))
		{
			UE_LOG(LogTemp, Log, TEXT("No existing settings save found. Skipping hardware benchmark during PIE and using current/default settings."));
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("No existing settings save found. Running hardware benchmark to detect optimal settings."));
		
		// Use Unreal Engine's built-in hardware benchmark to auto-detect appropriate settings
		if (GEngine)
		{
			if (UGameUserSettings* UserSettings = GEngine->GetGameUserSettings())
			{
				// RunHardwareBenchmark analyzes GPU, CPU, and memory to determine optimal settings
				UserSettings->RunHardwareBenchmark();
				
				// Apply the benchmark results to all scalability settings
				UserSettings->ApplyHardwareBenchmarkResults();
				
				// Get the detected overall quality level for logging and syncing modular settings
				const int32 DetectedQuality = UserSettings->GetOverallScalabilityLevel();
				
				// Apply and save the benchmark-determined settings
				UserSettings->ApplySettings(false);
				UserSettings->SaveSettings();
				
				UE_LOG(LogTemp, Log, TEXT("Hardware benchmark complete. Detected optimal quality level: %d (0=Low, 1=Medium, 2=High, 3=Ultra, -1=Custom)"), DetectedQuality);
				
				// Sync the modular settings to match the benchmark results
				// This updates our settings objects to reflect what the engine is now using
				const FGameplayTag OverallQualityTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Graphics.OverallQuality"), false);
				if (OverallQualityTag.IsValid())
				{
					if (UEFModularSettingsBase* OverallSetting = Settings.FindRef(OverallQualityTag))
					{
						if (UEFOverallQualitySetting* OverallQualitySetting = Cast<UEFOverallQualitySetting>(OverallSetting))
						{
							OverallQualitySetting->SyncFromCurrentEngineState();
						}
						else
						{
							// Fallback for non-standard overall-quality implementations.
							FString DetectedValue;
							if (DetectedQuality == 0) DetectedValue = TEXT("Low");
							else if (DetectedQuality == 1) DetectedValue = TEXT("Medium");
							else if (DetectedQuality == 2) DetectedValue = TEXT("High");
							else if (DetectedQuality == 3) DetectedValue = TEXT("Ultra");
							else DetectedValue = TEXT("Custom"); // -1 or mixed results

							OverallSetting->SetValueFromString(DetectedValue);
						}
					}
				}

				SaveToDisk();
			}
		}
	}
}


void UEFModularSettingsSubsystem::SaveToDiskAsync()
{
	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [this]()
	{
		SaveToDisk();
		
		AsyncTask(ENamedThreads::GameThread, [this]()
		{
			OnSettingsSaved.Broadcast();
		});
	});
}


void UEFModularSettingsSubsystem::LoadFromDiskAsync()
{
	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [this]()
	{
		LoadFromDisk();
		
		AsyncTask(ENamedThreads::GameThread, [this]()
		{
			OnSettingsLoaded.Broadcast();
		});
	});
}


bool UEFModularSettingsSubsystem::HasSetting(FGameplayTag Tag) const
{
	return Settings.Contains(Tag);
}


void UEFModularSettingsSubsystem::RegisterSetting(UEFModularSettingsBase* Setting)
{
	if (Setting && Setting->SettingTag.IsValid())
	{
		Settings.Add(Setting->SettingTag, Setting);
		Setting->ModularSettingsSubsystem = this;
		Setting->OnRegistered();
		UE_LOG(LogTemp, Log, TEXT("Registered setting: %s"), *Setting->SettingTag.ToString());
	}
}


TArray<UEFModularSettingsBase*> UEFModularSettingsSubsystem::GetSettingsByCategory(FName Category) const
{
	TArray<UEFModularSettingsBase*> Result;
	
	for (const auto& SettingPair : Settings)
	{
		if (SettingPair.Value->ConfigCategory == Category)
		{
			Result.Add(SettingPair.Value);
		}
	}
	
	return Result;
}


void UEFModularSettingsSubsystem::HandleSetCommand(const TArray<FString>& Args)
{
	if (Args.Num() < 2)
	{
		UE_LOG(LogTemp, Warning, TEXT("Usage: Settings.Set <Tag> <Value>"));
		return;
	}

	const FString TagString = Args[0];
	const FString ValueString = EFModularSettingsConsole::JoinArgs(Args, 1);

	const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagString), false);
	if (!Tag.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Invalid Tag: %s"), *TagString);
		return;
	}

	ApplyConsoleSettingValue(Tag, ValueString, TEXT("Settings.Set"));
}


void UEFModularSettingsSubsystem::HandleUpscalerSetCommand(const TArray<FString>& Args)
{
	if (Args.Num() < 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("Usage: Settings.Upscaler.Set <None|DLSS|FSR|XeSS>"));
		LogSettingValue(EFModularSettingsConsole::MakeTag(TEXT("Settings.Graphics.Upscaler")), TEXT("Upscaler"));
		return;
	}

	ApplyConsoleSettingValue(EFModularSettingsConsole::MakeTag(TEXT("Settings.Graphics.Upscaler")), EFModularSettingsConsole::JoinArgs(Args), TEXT("Settings.Upscaler.Set"));
}


void UEFModularSettingsSubsystem::HandleUpscalerDLSSModeCommand(const TArray<FString>& Args)
{
	if (Args.Num() < 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("Usage: Settings.Upscaler.DLSS.Mode <Auto|DLAA|UltraQuality|Quality|Balanced|Performance|UltraPerformance>"));
		LogSettingValue(EFModularSettingsConsole::MakeTag(TEXT("Settings.Graphics.Upscaler.DLSS.Mode")), TEXT("DLSS Mode"));
		return;
	}

	ApplyConsoleSettingValue(EFModularSettingsConsole::MakeTag(TEXT("Settings.Graphics.Upscaler.DLSS.Mode")), EFModularSettingsConsole::JoinArgs(Args), TEXT("Settings.Upscaler.DLSS.Mode"));
}


void UEFModularSettingsSubsystem::HandleUpscalerDLSSRayReconstructionCommand(const TArray<FString>& Args)
{
	if (Args.Num() < 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("Usage: Settings.Upscaler.DLSS.RayReconstruction <true|false>"));
		LogSettingValue(EFModularSettingsConsole::MakeTag(TEXT("Settings.Graphics.Upscaler.DLSS.RayReconstruction")), TEXT("DLSS Ray Reconstruction"));
		return;
	}

	ApplyConsoleSettingValue(EFModularSettingsConsole::MakeTag(TEXT("Settings.Graphics.Upscaler.DLSS.RayReconstruction")), EFModularSettingsConsole::JoinArgs(Args), TEXT("Settings.Upscaler.DLSS.RayReconstruction"));
}


void UEFModularSettingsSubsystem::HandleUpscalerFSRModeCommand(const TArray<FString>& Args)
{
	if (Args.Num() < 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("Usage: Settings.Upscaler.FSR.Mode <NativeAA|Quality|Balanced|Performance|UltraPerformance>"));
		LogSettingValue(EFModularSettingsConsole::MakeTag(TEXT("Settings.Graphics.Upscaler.FSR.Mode")), TEXT("FSR Mode"));
		return;
	}

	ApplyConsoleSettingValue(EFModularSettingsConsole::MakeTag(TEXT("Settings.Graphics.Upscaler.FSR.Mode")), EFModularSettingsConsole::JoinArgs(Args), TEXT("Settings.Upscaler.FSR.Mode"));
}


void UEFModularSettingsSubsystem::HandleUpscalerFSRSharpnessCommand(const TArray<FString>& Args)
{
	if (Args.Num() < 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("Usage: Settings.Upscaler.FSR.Sharpness <0.0-1.0>"));
		LogSettingValue(EFModularSettingsConsole::MakeTag(TEXT("Settings.Graphics.Upscaler.FSR.Sharpness")), TEXT("FSR Sharpness"));
		return;
	}

	ApplyConsoleSettingValue(EFModularSettingsConsole::MakeTag(TEXT("Settings.Graphics.Upscaler.FSR.Sharpness")), EFModularSettingsConsole::JoinArgs(Args), TEXT("Settings.Upscaler.FSR.Sharpness"));
}


void UEFModularSettingsSubsystem::HandleUpscalerXeSSQualityModeCommand(const TArray<FString>& Args)
{
	if (Args.Num() < 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("Usage: Settings.Upscaler.XeSS.QualityMode <UltraPerformance|Performance|Balanced|Quality|UltraQuality|UltraQualityPlus|AntiAliasing>"));
		LogSettingValue(EFModularSettingsConsole::MakeTag(TEXT("Settings.Graphics.Upscaler.XeSS.QualityMode")), TEXT("XeSS Quality Mode"));
		return;
	}

	ApplyConsoleSettingValue(EFModularSettingsConsole::MakeTag(TEXT("Settings.Graphics.Upscaler.XeSS.QualityMode")), EFModularSettingsConsole::JoinArgs(Args), TEXT("Settings.Upscaler.XeSS.QualityMode"));
}


void UEFModularSettingsSubsystem::HandleUpscalerFrameGenerationCommand(const TArray<FString>& Args)
{
	if (Args.Num() < 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("Usage: Settings.Upscaler.FrameGeneration <true|false>"));
		LogSettingValue(EFModularSettingsConsole::MakeTag(TEXT("Settings.Graphics.Upscaler.FrameGeneration")), TEXT("Frame Generation"));
		return;
	}

	ApplyConsoleSettingValue(EFModularSettingsConsole::MakeTag(TEXT("Settings.Graphics.Upscaler.FrameGeneration")), EFModularSettingsConsole::JoinArgs(Args), TEXT("Settings.Upscaler.FrameGeneration"));
}


void UEFModularSettingsSubsystem::HandleUpscalerResolutionScaleCommand(const TArray<FString>& Args)
{
	if (Args.Num() < 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("Usage: Settings.Upscaler.ResolutionScale <25-200>"));
		LogSettingValue(EFModularSettingsConsole::MakeTag(TEXT("Settings.Graphics.Upscaler.ResolutionScale")), TEXT("Resolution Scale"));
		return;
	}

	ApplyConsoleSettingValue(EFModularSettingsConsole::MakeTag(TEXT("Settings.Graphics.Upscaler.ResolutionScale")), EFModularSettingsConsole::JoinArgs(Args), TEXT("Settings.Upscaler.ResolutionScale"));
}


void UEFModularSettingsSubsystem::HandleUpscalerStatusCommand(const TArray<FString>& Args)
{
	if (Args.Num() > 0 && Args[0].Equals(TEXT("help"), ESearchCase::IgnoreCase))
	{
		UE_LOG(LogTemp, Log, TEXT("Settings.Upscaler.Set <None|DLSS|FSR|XeSS>"));
		UE_LOG(LogTemp, Log, TEXT("Settings.Upscaler.DLSS.Mode <Auto|DLAA|UltraQuality|Quality|Balanced|Performance|UltraPerformance>"));
		UE_LOG(LogTemp, Log, TEXT("Settings.Upscaler.DLSS.RayReconstruction <true|false>"));
		UE_LOG(LogTemp, Log, TEXT("Settings.Upscaler.FSR.Mode <NativeAA|Quality|Balanced|Performance|UltraPerformance>"));
		UE_LOG(LogTemp, Log, TEXT("Settings.Upscaler.FSR.Sharpness <0.0-1.0>"));
		UE_LOG(LogTemp, Log, TEXT("Settings.Upscaler.XeSS.QualityMode <UltraPerformance|Performance|Balanced|Quality|UltraQuality|UltraQualityPlus|AntiAliasing>"));
		UE_LOG(LogTemp, Log, TEXT("Settings.Upscaler.FrameGeneration <true|false>"));
		UE_LOG(LogTemp, Log, TEXT("Settings.Upscaler.ResolutionScale <25-200>"));
	}

	LogUpscalerStatus();
}


void UEFModularSettingsSubsystem::HandleDisplayTestAspectCommand(const TArray<FString>& Args)
{
	if (Args.Num() < 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("Usage: Settings.Display.TestAspect <16:9|19:9|21:9|32:9|Native> [Windowed|Fullscreen|Borderless]"));
		return;
	}

	if (!GEngine)
	{
		UE_LOG(LogTemp, Warning, TEXT("Settings.Display.TestAspect requires a valid engine instance."));
		return;
	}

	UGameUserSettings* UserSettings = GEngine->GetGameUserSettings();
	if (!UserSettings)
	{
		UE_LOG(LogTemp, Warning, TEXT("Settings.Display.TestAspect requires GameUserSettings."));
		return;
	}

	const FString AspectArgument = Args[0].TrimStartAndEnd();
	EWindowMode::Type WindowMode = EWindowMode::Windowed;
	FString WindowModeName = TEXT("Windowed");

	if (Args.Num() >= 2 && !EFModularSettingsConsole::TryParseWindowMode(Args[1], WindowMode, WindowModeName))
	{
		UE_LOG(LogTemp, Warning, TEXT("Settings.Display.TestAspect invalid window mode '%s'. Expected Windowed, Fullscreen, or Borderless."), *Args[1]);
		return;
	}

	int32 MonitorWidth = 0;
	int32 MonitorHeight = 0;
	UEFMonitorLibrary::GetCurrentMonitorResolution(MonitorWidth, MonitorHeight);
	if (MonitorWidth <= 0 || MonitorHeight <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Settings.Display.TestAspect could not determine the current monitor resolution."));
		return;
	}

	FIntPoint TargetResolution = FIntPoint(MonitorWidth, MonitorHeight);
	FString AspectLabel = TEXT("Native");

	if (!AspectArgument.Equals(TEXT("native"), ESearchCase::IgnoreCase)
		&& !AspectArgument.Equals(TEXT("reset"), ESearchCase::IgnoreCase))
	{
		int32 AspectWidth = 0;
		int32 AspectHeight = 0;
		if (!EFModularSettingsConsole::TryParseAspectRatio(AspectArgument, AspectWidth, AspectHeight))
		{
			UE_LOG(LogTemp, Warning, TEXT("Settings.Display.TestAspect invalid aspect ratio '%s'. Expected values like 16:9, 19:9, 21:9, 32:9, or Native."), *AspectArgument);
			return;
		}

		AspectLabel = FString::Printf(TEXT("%d:%d"), AspectWidth, AspectHeight);
		const float TargetAspectRatio = static_cast<float>(AspectWidth) / static_cast<float>(AspectHeight);
		TargetResolution = EFModularSettingsConsole::ComputeLargestFittingResolution(MonitorWidth, MonitorHeight, TargetAspectRatio);
		if (TargetResolution.X <= 0 || TargetResolution.Y <= 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("Settings.Display.TestAspect could not compute a valid resolution for aspect %s on monitor %dx%d."),
				*AspectLabel,
				MonitorWidth,
				MonitorHeight);
			return;
		}
	}

	const float NativeAspect = static_cast<float>(MonitorWidth) / static_cast<float>(MonitorHeight);
	const float RequestedAspect = static_cast<float>(TargetResolution.X) / static_cast<float>(TargetResolution.Y);
	if (WindowMode == EWindowMode::WindowedFullscreen && !FMath::IsNearlyEqual(NativeAspect, RequestedAspect, 0.01f))
	{
		UE_LOG(LogTemp, Warning, TEXT("Settings.Display.TestAspect switched Borderless to Windowed because borderless always uses the monitor's native aspect ratio."));
		WindowMode = EWindowMode::Windowed;
		WindowModeName = TEXT("Windowed");
	}

	UserSettings->SetFullscreenMode(WindowMode);
	UserSettings->SetScreenResolution(TargetResolution);
	UserSettings->ApplySettings(false);
	UserSettings->SaveSettings();

	const FString ResolutionString = FString::Printf(TEXT("%dx%d"), TargetResolution.X, TargetResolution.Y);

	if (UEFModularSettingsMultiSelect* DisplayModeSetting = Cast<UEFModularSettingsMultiSelect>(GetSettingByTag(EFModularSettingsConsole::MakeTag(TEXT("Settings.Display.DisplayMode")))))
	{
		const int32 DisplayModeIndex = DisplayModeSetting->Values.IndexOfByPredicate([&WindowModeName](const FString& Candidate)
		{
			return Candidate.Equals(WindowModeName, ESearchCase::IgnoreCase);
		});

		if (DisplayModeSetting->Values.IsValidIndex(DisplayModeIndex))
		{
			DisplayModeSetting->SetSelectedIndex(DisplayModeIndex);
			DisplayModeSetting->SaveCurrentValue();
			DisplayModeSetting->ClearDirty();
			OnSettingsChanged.Broadcast(DisplayModeSetting);
		}
	}

	if (UEFModularSettingsMultiSelect* ResolutionSetting = Cast<UEFModularSettingsMultiSelect>(GetSettingByTag(EFModularSettingsConsole::MakeTag(TEXT("Settings.Graphics.Resolution")))))
	{
		ResolutionSetting->RefreshValues();
		if (!ResolutionSetting->Values.Contains(ResolutionString))
		{
			ResolutionSetting->Values.Add(ResolutionString);
			ResolutionSetting->DisplayNames.Add(FText::FromString(ResolutionString));
		}

		ResolutionSetting->SetValueFromString(ResolutionString);
		ResolutionSetting->SaveCurrentValue();
		ResolutionSetting->ClearDirty();
		OnSettingsChanged.Broadcast(ResolutionSetting);
	}

	SaveToDisk();

	UE_LOG(LogTemp, Log, TEXT("Settings.Display.TestAspect applied %s as %s on monitor %dx%d using %s mode."),
		*AspectLabel,
		*ResolutionString,
		MonitorWidth,
		MonitorHeight,
		*WindowModeName);
}


