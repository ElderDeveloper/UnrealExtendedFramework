// Fill out your copyright notice in the Description page of Project Settings.

#include "EFModularSettingsSubsystem.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "SaveGame/EFSettingsSaveGame.h"
#include "Async/Async.h"
#include "GameFramework/GameUserSettings.h"

static const FString SettingsSaveSlotName = TEXT("ModularSettingsSave");
static const int32 SettingsUserIndex = 0;

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

	// Register Console Command
	SetCommand = IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("Settings.Set"),
		TEXT("Set a modular setting value. Usage: Settings.Set <Tag> <Value>"),
		FConsoleCommandWithArgsDelegate::CreateUObject(this, &UEFModularSettingsSubsystem::HandleSetCommand),
		ECVF_Default
	);
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
						// Map the detected level to our setting values
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

	FString TagString = Args[0];
	FString ValueString = Args[1];
	
	// Handle values with spaces (reconstruct the string)
	if (Args.Num() > 2)
	{
		for (int32 i = 2; i < Args.Num(); ++i)
		{
			ValueString += TEXT(" ") + Args[i];
		}
	}

	FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagString));
	if (!Tag.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Invalid Tag: %s"), *TagString);
		return;
	}

	if (UEFModularSettingsBase* Setting = GetSettingByTag(Tag))
	{
		Setting->SetValueFromString(ValueString);
		Setting->SaveCurrentValue();
		Setting->Apply();
		Setting->ClearDirty();
		SaveToDisk(); // Optional: Auto-save on console command
		
		UE_LOG(LogTemp, Log, TEXT("Set %s to %s"), *TagString, *ValueString);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Setting not found: %s"), *TagString);
	}
}


