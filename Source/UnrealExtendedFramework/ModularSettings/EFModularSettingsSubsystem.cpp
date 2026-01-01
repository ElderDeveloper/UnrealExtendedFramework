// Fill out your copyright notice in the Description page of Project Settings.

#include "EFModularSettingsSubsystem.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "SaveGame/EFSettingsSaveGame.h"
#include "Async/Async.h"

static const FString SettingsSaveSlotName = TEXT("ModularSettingsSave");
static const int32 SettingsUserIndex = 0;

void UEFModularSettingsSubsystem::Initialize(FSubsystemCollectionBase& SubsystemCollectionBase)
{
	Super::Initialize(SubsystemCollectionBase);

	const UEFModularProjectSettings* ProjectSettings = GetDefault<UEFModularProjectSettings>();
	if (ProjectSettings)
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

	// Sync settings that use GameUserSettings APIs from engine state BEFORE loading from disk
	// This ensures the settings reflect actual engine state, not just defaults
	for (const auto& SettingPair : Settings)
	{
		UEFModularSettingsBase* Setting = SettingPair.Value;
		if (Setting)
		{
			Setting->SyncFromEngine();
		}
	}

	LoadFromDisk();

	// Force apply all settings to ensure engine state matches loaded settings
	for (const auto& SettingPair : Settings)
	{
		UEFModularSettingsBase* Setting = SettingPair.Value;
		if (Setting)
		{
			// Ensure the "PreviousValue" matches what we just loaded
			Setting->SaveCurrentValue();
			
			// Force apply to engine
			Setting->Apply();
			
			// Mark as clean since it's now applied
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
	for (const auto& SettingPair : Settings)
	{
		UEFModularSettingsBase* Setting = SettingPair.Value;
		if (Setting->IsDirty())
		{
			Setting->SaveCurrentValue();
			Setting->Apply();
			Setting->ClearDirty();
			bAnyChanged = true;
		}
	}
    
	if (bAnyChanged)
	{
		SaveToDisk();
		UE_LOG(LogTemp, Log, TEXT("Applied and saved changed settings."));
	}
}


void UEFModularSettingsSubsystem::RevertAllChanges()
{
	for (const auto& SettingPair : Settings)
	{
		UEFModularSettingsBase* Setting = SettingPair.Value;
		Setting->RevertToSavedValue();
		OnSettingsChanged.Broadcast(Setting);
	}
    
	UE_LOG(LogTemp, Log, TEXT("All settings reverted to saved values."));
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
		UE_LOG(LogTemp, Log, TEXT("No existing settings save found. Using defaults."));
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


void UEFModularSettingsSubsystem::RequestSafeChange(FGameplayTag Tag, FString NewValue, float RevertTime)
{
	if (UEFModularSettingsBase* Setting = GetSettingByTag(Tag))
	{
		// Cancel any existing revert
		if (IsRevertPending())
		{
			ConfirmChange();
		}

		// Store current value for revert
		PendingRevertTag = Tag;
		PendingRevertValue = Setting->GetValueAsString();

		// Apply new value
		Setting->SetValueFromString(NewValue);
		Setting->Apply();
		
		// Start timer
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(RevertTimerHandle, this, &UEFModularSettingsSubsystem::RevertPendingChange, RevertTime, false);
		}

		OnSafeChangeRequested.Broadcast(RevertTime);
		UE_LOG(LogTemp, Log, TEXT("Requested safe change for %s. Reverting in %f seconds."), *Tag.ToString(), RevertTime);
	}
}


void UEFModularSettingsSubsystem::ConfirmChange()
{
	if (IsRevertPending())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(RevertTimerHandle);
		}
		
		// Save the new value (make it permanent)
		if (UEFModularSettingsBase* Setting = GetSettingByTag(PendingRevertTag))
		{
			Setting->SaveCurrentValue();
			SaveToDisk(); // Persist immediately
		}

		PendingRevertTag = FGameplayTag();
		PendingRevertValue.Empty();
		
		UE_LOG(LogTemp, Log, TEXT("Confirmed safe change."));
	}
}


void UEFModularSettingsSubsystem::RevertPendingChange()
{
	if (IsRevertPending())
	{
		if (UEFModularSettingsBase* Setting = GetSettingByTag(PendingRevertTag))
		{
			Setting->SetValueFromString(PendingRevertValue);
			Setting->Apply();
			Setting->SaveCurrentValue(); // Restore saved state
			
			OnSettingsChanged.Broadcast(Setting);
		}

		PendingRevertTag = FGameplayTag();
		PendingRevertValue.Empty();
		
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(RevertTimerHandle);
		}

		OnSafeChangeReverted.Broadcast();
		UE_LOG(LogTemp, Log, TEXT("Reverted safe change."));
	}
}

bool UEFModularSettingsSubsystem::IsRevertPending() const
{
	return PendingRevertTag.IsValid();
}
