// Fill out your copyright notice in the Description page of Project Settings.


#include "ExtendedSettingsLibrary.h"
#include "GameplayTagContainer.h"

UExtendedSettingsSubsystem* UExtendedSettingsLibrary::GetExtendedSettingsSubsystem(const UObject* WorldContextObject)
{
	if (WorldContextObject)
	{
		if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
		{
			if (UGameInstance* GameInstance = World->GetGameInstance())
			{
				return GameInstance->GetSubsystem<UExtendedSettingsSubsystem>();
			}
		}
	}
	return nullptr;
}


// Gameplay Settings
FExtendedGameplaySettings UExtendedSettingsLibrary::GetGameplaySettings(const UObject* WorldContextObject)
{
	UExtendedSettingsSubsystem* SettingsSubsystem = GetExtendedSettingsSubsystem(WorldContextObject);
	return SettingsSubsystem ? SettingsSubsystem->GetGameplaySettings() : FExtendedGameplaySettings();
}

void UExtendedSettingsLibrary::SetGameplaySettings(const UObject* WorldContextObject, const FExtendedGameplaySettings& GameplaySettings)
{
	if (UExtendedSettingsSubsystem* SettingsSubsystem = GetExtendedSettingsSubsystem(WorldContextObject))
	{
		SettingsSubsystem->SetGameplaySettings(GameplaySettings);
	}
}

void UExtendedSettingsLibrary::ApplyGameplaySettings(const UObject* WorldContextObject, const FExtendedGameplaySettings& GameplaySettings)
{
	if (UExtendedSettingsSubsystem* SettingsSubsystem = GetExtendedSettingsSubsystem(WorldContextObject))
	{
		SettingsSubsystem->ApplyGameplaySettings(GameplaySettings);
	}
}

// Audio Settings
FExtendedAudioSettings UExtendedSettingsLibrary::GetAudioSettings(const UObject* WorldContextObject)
{
	UExtendedSettingsSubsystem* SettingsSubsystem = GetExtendedSettingsSubsystem(WorldContextObject);
	return SettingsSubsystem ? SettingsSubsystem->GetAudioSettings() : FExtendedAudioSettings();
}

void UExtendedSettingsLibrary::SetAudioSettings(const UObject* WorldContextObject, const FExtendedAudioSettings& AudioSettings)
{
	if (UExtendedSettingsSubsystem* SettingsSubsystem = GetExtendedSettingsSubsystem(WorldContextObject))
	{
		SettingsSubsystem->SetAudioSettings(AudioSettings);
	}
}

void UExtendedSettingsLibrary::ApplyAudioSettings(const UObject* WorldContextObject, const FExtendedAudioSettings& AudioSettings)
{
	if (UExtendedSettingsSubsystem* SettingsSubsystem = GetExtendedSettingsSubsystem(WorldContextObject))
	{
		SettingsSubsystem->ApplyAudioSettings(AudioSettings);
	}
}

// Graphics Settings
FExtendedGraphicsSettings UExtendedSettingsLibrary::GetGraphicsSettings(const UObject* WorldContextObject)
{
	UExtendedSettingsSubsystem* SettingsSubsystem = GetExtendedSettingsSubsystem(WorldContextObject);
	return SettingsSubsystem ? SettingsSubsystem->GetGraphicsSettings() : FExtendedGraphicsSettings();
}

void UExtendedSettingsLibrary::SetGraphicsSettings(const UObject* WorldContextObject, const FExtendedGraphicsSettings& GraphicsSettings)
{
	if (UExtendedSettingsSubsystem* SettingsSubsystem = GetExtendedSettingsSubsystem(WorldContextObject))
	{
		SettingsSubsystem->SetGraphicsSettings(GraphicsSettings);
	}
}

void UExtendedSettingsLibrary::ApplyGraphicsSettings(const UObject* WorldContextObject, const FExtendedGraphicsSettings& GraphicsSettings)
{
	if (UExtendedSettingsSubsystem* SettingsSubsystem = GetExtendedSettingsSubsystem(WorldContextObject))
	{
		SettingsSubsystem->ApplyGraphicsSettings(GraphicsSettings);
	}
}

// Display Settings
FExtendedDisplaySettings UExtendedSettingsLibrary::GetDisplaySettings(const UObject* WorldContextObject)
{
	UExtendedSettingsSubsystem* SettingsSubsystem = GetExtendedSettingsSubsystem(WorldContextObject);
	return SettingsSubsystem ? SettingsSubsystem->GetDisplaySettings() : FExtendedDisplaySettings();
}

void UExtendedSettingsLibrary::SetDisplaySettings(const UObject* WorldContextObject, const FExtendedDisplaySettings& DisplaySettings)
{
	if (UExtendedSettingsSubsystem* SettingsSubsystem = GetExtendedSettingsSubsystem(WorldContextObject))
	{
		SettingsSubsystem->SetDisplaySettings(DisplaySettings);
	}
}

void UExtendedSettingsLibrary::ApplyDisplaySettings(const UObject* WorldContextObject, const FExtendedDisplaySettings& DisplaySettings)
{
	if (UExtendedSettingsSubsystem* SettingsSubsystem = GetExtendedSettingsSubsystem(WorldContextObject))
	{
		SettingsSubsystem->ApplyDisplaySettings(DisplaySettings);
	}
}

FExtendedGraphicsSettings UExtendedSettingsLibrary::GetOverallGraphicSettings(int32 OverallQuality)
{
	return FExtendedGraphicsSettings().GetOverallGraphicSettings(OverallQuality);
}
 

// General Settings Operations
void UExtendedSettingsLibrary::ApplyAllSettings(const UObject* WorldContextObject)
{
	if (UExtendedSettingsSubsystem* SettingsSubsystem = GetExtendedSettingsSubsystem(WorldContextObject))
	{
		SettingsSubsystem->ApplySettings();
	}
}


void UExtendedSettingsLibrary::RevertAllSettings(const UObject* WorldContextObject)
{
	if (UExtendedSettingsSubsystem* SettingsSubsystem = GetExtendedSettingsSubsystem(WorldContextObject))
	{
		SettingsSubsystem->RevertSettings();
	}
}


void UExtendedSettingsLibrary::SaveAllSettings(const UObject* WorldContextObject)
{
	if (UExtendedSettingsSubsystem* SettingsSubsystem = GetExtendedSettingsSubsystem(WorldContextObject))
	{
		SettingsSubsystem->SaveExtendedSettings();
	}
}


void UExtendedSettingsLibrary::FindAndApplyBestSettings(const UObject* WorldContextObject)
{
	if (UExtendedSettingsSubsystem* SettingsSubsystem = GetExtendedSettingsSubsystem(WorldContextObject))
	{
		SettingsSubsystem->FindAndApplyBestSettings();
	}
}


// Audio Device Management
void UExtendedSettingsLibrary::UpdateAudioDeviceLists(const UObject* WorldContextObject)
{
	if (UExtendedSettingsSubsystem* SettingsSubsystem = GetExtendedSettingsSubsystem(WorldContextObject))
	{
		SettingsSubsystem->UpdateAudioDeviceLists();
	}
}


// Display Management
void UExtendedSettingsLibrary::UpdateScreenResolutionList(const UObject* WorldContextObject)
{
	if (UExtendedSettingsSubsystem* SettingsSubsystem = GetExtendedSettingsSubsystem(WorldContextObject))
	{
		SettingsSubsystem->UpdateScreenResolutionList();
	}
}


FExtendedDifficultySettings UExtendedSettingsLibrary::GetDifficultySettings(const uint8& DifficultyLevel)
{
	const UExtendedSettingsDeveloperSettings* Settings = GetDefault<UExtendedSettingsDeveloperSettings>();
	if (Settings && Settings->DifficultySettingsTable.IsValid())
	{
		const auto Rows = Settings->DifficultySettingsTable.Get()->GetRowNames();
		if (Rows.IsValidIndex(DifficultyLevel))
		{
			const FName RowName = Rows[DifficultyLevel];
			if (FExtendedDifficultySettings* DifficultySettings =
				Settings->DifficultySettingsTable.Get()->FindRow<FExtendedDifficultySettings>(RowName, TEXT("")))
			{
				return *DifficultySettings;
			}
		}
		else
			UE_LOG(LogTemp, Warning, TEXT("UExtendedSettingsLibrary::GetDifficultySettings Invalid Difficulty Level: %d"), DifficultyLevel);
	}
	return FExtendedDifficultySettings();
}


TArray<FExtendedDifficultySettings> UExtendedSettingsLibrary::GetAvailableDifficultyLevels()
{
	const UExtendedSettingsDeveloperSettings* Settings = GetDefault<UExtendedSettingsDeveloperSettings>();
	TArray<FExtendedDifficultySettings> DifficultyLevels;

	if (Settings && Settings->DifficultySettingsTable.IsValid())
	{
		const auto Rows = Settings->DifficultySettingsTable.Get()->GetRowNames();
		for (const FName& RowName : Rows)
		{
			if (FExtendedDifficultySettings* DifficultySettings =
				Settings->DifficultySettingsTable.Get()->FindRow<FExtendedDifficultySettings>(RowName, TEXT("")))
			{
				DifficultyLevels.Add(*DifficultySettings);
			}
		}
	}
	return DifficultyLevels;
}

