// Fill out your copyright notice in the Description page of Project Settings.


#include "EFSettingsLibrary.h"
#include "GameplayTagContainer.h"

UEFSettingsSubsystem* UEFSettingsLibrary::GetEFSettingsSubsystem(const UObject* WorldContextObject)
{
	if (WorldContextObject)
	{
		if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
		{
			if (UGameInstance* GameInstance = World->GetGameInstance())
			{
				return GameInstance->GetSubsystem<UEFSettingsSubsystem>();
			}
		}
	}
	return nullptr;
}


// Gameplay Settings
FExtendedGameplaySettings UEFSettingsLibrary::GetGameplaySettings(const UObject* WorldContextObject)
{
	UEFSettingsSubsystem* SettingsSubsystem = GetEFSettingsSubsystem(WorldContextObject);
	return SettingsSubsystem ? SettingsSubsystem->GetGameplaySettings() : FExtendedGameplaySettings();
}

void UEFSettingsLibrary::SetGameplaySettings(const UObject* WorldContextObject, const FExtendedGameplaySettings& GameplaySettings)
{
	if (UEFSettingsSubsystem* SettingsSubsystem = GetEFSettingsSubsystem(WorldContextObject))
	{
		SettingsSubsystem->SetGameplaySettings(GameplaySettings);
	}
}

void UEFSettingsLibrary::ApplyGameplaySettings(const UObject* WorldContextObject, const FExtendedGameplaySettings& GameplaySettings)
{
	if (UEFSettingsSubsystem* SettingsSubsystem = GetEFSettingsSubsystem(WorldContextObject))
	{
		SettingsSubsystem->ApplyGameplaySettings(GameplaySettings);
	}
}

// Audio Settings
FExtendedAudioSettings UEFSettingsLibrary::GetAudioSettings(const UObject* WorldContextObject)
{
	UEFSettingsSubsystem* SettingsSubsystem = GetEFSettingsSubsystem(WorldContextObject);
	return SettingsSubsystem ? SettingsSubsystem->GetAudioSettings() : FExtendedAudioSettings();
}

void UEFSettingsLibrary::SetAudioSettings(const UObject* WorldContextObject, const FExtendedAudioSettings& AudioSettings)
{
	if (UEFSettingsSubsystem* SettingsSubsystem = GetEFSettingsSubsystem(WorldContextObject))
	{
		SettingsSubsystem->SetAudioSettings(AudioSettings);
	}
}

void UEFSettingsLibrary::ApplyAudioSettings(const UObject* WorldContextObject, const FExtendedAudioSettings& AudioSettings)
{
	if (UEFSettingsSubsystem* SettingsSubsystem = GetEFSettingsSubsystem(WorldContextObject))
	{
		SettingsSubsystem->ApplyAudioSettings(AudioSettings);
	}
}

// Graphics Settings
FExtendedGraphicsSettings UEFSettingsLibrary::GetGraphicsSettings(const UObject* WorldContextObject)
{
	UEFSettingsSubsystem* SettingsSubsystem = GetEFSettingsSubsystem(WorldContextObject);
	return SettingsSubsystem ? SettingsSubsystem->GetGraphicsSettings() : FExtendedGraphicsSettings();
}

void UEFSettingsLibrary::SetGraphicsSettings(const UObject* WorldContextObject, const FExtendedGraphicsSettings& GraphicsSettings)
{
	if (UEFSettingsSubsystem* SettingsSubsystem = GetEFSettingsSubsystem(WorldContextObject))
	{
		SettingsSubsystem->SetGraphicsSettings(GraphicsSettings);
	}
}

void UEFSettingsLibrary::ApplyGraphicsSettings(const UObject* WorldContextObject, const FExtendedGraphicsSettings& GraphicsSettings)
{
	if (UEFSettingsSubsystem* SettingsSubsystem = GetEFSettingsSubsystem(WorldContextObject))
	{
		SettingsSubsystem->ApplyGraphicsSettings(GraphicsSettings);
	}
}

// Display Settings
FExtendedDisplaySettings UEFSettingsLibrary::GetDisplaySettings(const UObject* WorldContextObject)
{
	UEFSettingsSubsystem* SettingsSubsystem = GetEFSettingsSubsystem(WorldContextObject);
	return SettingsSubsystem ? SettingsSubsystem->GetDisplaySettings() : FExtendedDisplaySettings();
}

void UEFSettingsLibrary::SetDisplaySettings(const UObject* WorldContextObject, const FExtendedDisplaySettings& DisplaySettings)
{
	if (UEFSettingsSubsystem* SettingsSubsystem = GetEFSettingsSubsystem(WorldContextObject))
	{
		SettingsSubsystem->SetDisplaySettings(DisplaySettings);
	}
}

void UEFSettingsLibrary::ApplyDisplaySettings(const UObject* WorldContextObject, const FExtendedDisplaySettings& DisplaySettings)
{
	if (UEFSettingsSubsystem* SettingsSubsystem = GetEFSettingsSubsystem(WorldContextObject))
	{
		SettingsSubsystem->ApplyDisplaySettings(DisplaySettings);
	}
}

FExtendedGraphicsSettings UEFSettingsLibrary::GetOverallGraphicSettings(int32 OverallQuality)
{
	return FExtendedGraphicsSettings().GetOverallGraphicSettings(OverallQuality);
}
 

// General Settings Operations
void UEFSettingsLibrary::ApplyAllSettings(const UObject* WorldContextObject)
{
	if (UEFSettingsSubsystem* SettingsSubsystem = GetEFSettingsSubsystem(WorldContextObject))
	{
		SettingsSubsystem->ApplySettings();
	}
}


void UEFSettingsLibrary::RevertAllSettings(const UObject* WorldContextObject)
{
	if (UEFSettingsSubsystem* SettingsSubsystem = GetEFSettingsSubsystem(WorldContextObject))
	{
		SettingsSubsystem->RevertSettings();
	}
}


void UEFSettingsLibrary::SaveAllSettings(const UObject* WorldContextObject)
{
	if (UEFSettingsSubsystem* SettingsSubsystem = GetEFSettingsSubsystem(WorldContextObject))
	{
		SettingsSubsystem->SaveExtendedSettings();
	}
}


void UEFSettingsLibrary::FindAndApplyBestSettings(const UObject* WorldContextObject)
{
	if (UEFSettingsSubsystem* SettingsSubsystem = GetEFSettingsSubsystem(WorldContextObject))
	{
		SettingsSubsystem->FindAndApplyBestSettings();
	}
}


// Audio Device Management
void UEFSettingsLibrary::UpdateAudioDeviceLists(const UObject* WorldContextObject)
{
	if (UEFSettingsSubsystem* SettingsSubsystem = GetEFSettingsSubsystem(WorldContextObject))
	{
		SettingsSubsystem->UpdateAudioDeviceLists();
	}
}


// Display Management
void UEFSettingsLibrary::UpdateScreenResolutionList(const UObject* WorldContextObject)
{
	if (UEFSettingsSubsystem* SettingsSubsystem = GetEFSettingsSubsystem(WorldContextObject))
	{
		SettingsSubsystem->UpdateScreenResolutionList();
	}
}


FExtendedDifficultySettings UEFSettingsLibrary::GetDifficultySettings(const uint8& DifficultyLevel)
{
	const UEFDeveloperSettings* Settings = GetDefault<UEFDeveloperSettings>();
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
			UE_LOG(LogTemp, Warning, TEXT("UEFSettingsLibrary::GetDifficultySettings Invalid Difficulty Level: %d"), DifficultyLevel);
	}
	return FExtendedDifficultySettings();
}


TArray<FExtendedDifficultySettings> UEFSettingsLibrary::GetAvailableDifficultyLevels()
{
	const UEFDeveloperSettings* Settings = GetDefault<UEFDeveloperSettings>();
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

