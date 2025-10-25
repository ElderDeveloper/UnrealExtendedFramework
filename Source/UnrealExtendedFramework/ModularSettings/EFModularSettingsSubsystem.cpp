// Fill out your copyright notice in the Description page of Project Settings.

#include "EFModularSettingsSubsystem.h"
#include "Engine/Engine.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Async/Async.h"


void UEFModularSettingsSubsystem::Initialize(FSubsystemCollectionBase& SubsystemCollectionBase)
{
	Super::Initialize(SubsystemCollectionBase);

	const UEFModularProjectSettings* ProjectSettings = GetDefault<UEFModularProjectSettings>();
	if (ProjectSettings)
	{
		for (const TSoftObjectPtr<UEFModularSettingsContainer>& ContainerPtr : ProjectSettings->SettingsContainers)
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

	ApplyAllChanges();
}

void UEFModularSettingsSubsystem::Deinitialize()
{
	// Save settings before shutdown
	SaveToDisk();
	Super::Deinitialize();
}



bool UEFModularSettingsSubsystem::GetBool(FGameplayTag Tag) const
{
	if (const UEFModularSettingsBool* BoolSetting = GetSetting<UEFModularSettingsBool>(Tag))
	{
		return BoolSetting->Value;
	}
	return false;
}

void UEFModularSettingsSubsystem::SetBool(FGameplayTag Tag, bool Value)
{
	if (UEFModularSettingsBool* BoolSetting = GetSetting<UEFModularSettingsBool>(Tag))
	{
		BoolSetting->SetValue(Value);
		OnSettingsChanged.Broadcast(BoolSetting);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Setting with tag %s not found or is not a Bool setting."), *Tag.ToString());
	}
}



float UEFModularSettingsSubsystem::GetFloat(FGameplayTag Tag) const
{
	if (const UEFModularSettingsFloat* FloatSetting = GetSetting<UEFModularSettingsFloat>(Tag))
	{
		return FloatSetting->Value;
	}
	return 0.f;
}

void UEFModularSettingsSubsystem::SetFloat(FGameplayTag Tag, float Value)
{
	if (UEFModularSettingsFloat* FloatSetting = GetSetting<UEFModularSettingsFloat>(Tag))
	{
		FloatSetting->SetValue(Value);
		OnSettingsChanged.Broadcast(FloatSetting);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Setting with tag %s not found or is not a Float setting."), *Tag.ToString());
	}
}



int32 UEFModularSettingsSubsystem::GetIndex(FGameplayTag Tag) const
{
	if (const UEFModularSettingsMultiSelect* MultiSelectSetting = GetSetting<UEFModularSettingsMultiSelect>(Tag))
	{
		return MultiSelectSetting->SelectedIndex;
	}
	return 0;
}

void UEFModularSettingsSubsystem::SetIndex(FGameplayTag Tag, int32 Index)
{
	if (UEFModularSettingsMultiSelect* MultiSelectSetting = GetSetting<UEFModularSettingsMultiSelect>(Tag))
	{
		if (Index >= 0 && Index < MultiSelectSetting->Values.Num())
		{
			MultiSelectSetting->SetSelectedIndex(Index);
			OnSettingsChanged.Broadcast(MultiSelectSetting);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Index %d is out of bounds for setting with tag %s."), Index, *Tag.ToString());
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Setting with tag %s not found or is not a Multi-select setting."), *Tag.ToString());
	}
}

void UEFModularSettingsSubsystem::AddIndex(FGameplayTag Tag, int32 Amount)
{
	if (UEFModularSettingsMultiSelect* MultiSelectSetting = GetSetting<UEFModularSettingsMultiSelect>(Tag))
	{
		int32 NewIndex = MultiSelectSetting->SelectedIndex + Amount;
		
		if (NewIndex < 0)
		{
			MultiSelectSetting->SetSelectedIndex(MultiSelectSetting->Values.Num() - 1);
			OnSettingsChanged.Broadcast(MultiSelectSetting);
			return;
		}
		
		if (NewIndex >= 0 && NewIndex < MultiSelectSetting->Values.Num())
		{
			MultiSelectSetting->SetSelectedIndex(NewIndex);
			OnSettingsChanged.Broadcast(MultiSelectSetting);
		}
		else
		{
			MultiSelectSetting->SetSelectedIndex(0);
			OnSettingsChanged.Broadcast(MultiSelectSetting);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Setting with tag %s not found or is not a Multi-select setting."), *Tag.ToString());
	}
}

TArray<FText> UEFModularSettingsSubsystem::GetOptions(FGameplayTag Tag) const
{
	if (const UEFModularSettingsMultiSelect* MultiSelectSetting = GetSetting<UEFModularSettingsMultiSelect>(Tag))
	{
		return MultiSelectSetting->DisplayNames;
	}
	return TArray<FText>();
}

FText UEFModularSettingsSubsystem::GetSelectedOption(FGameplayTag Tag) const
{
	if (const UEFModularSettingsMultiSelect* MultiSelectSetting = GetSetting<UEFModularSettingsMultiSelect>(Tag))
	{
		if (MultiSelectSetting->DisplayNames.IsValidIndex(MultiSelectSetting->SelectedIndex))
		{
			return MultiSelectSetting->DisplayNames[MultiSelectSetting->SelectedIndex];
		}
	}
	return FText::GetEmpty();
}



void UEFModularSettingsSubsystem::ApplyAllChanges()
{
	for (const auto& SettingPair : Settings)
	{
		UEFModularSettingsBase* Setting = SettingPair.Value;
		Setting->SaveCurrentValue();
		Setting->Apply();
	}
    
	SaveToDisk();
	UE_LOG(LogTemp, Log, TEXT("All settings applied and saved."));
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

bool UEFModularSettingsSubsystem::HasPendingChanges() const
{
	for (const auto& SettingPair : Settings)
	{
		const UEFModularSettingsBase* Setting = SettingPair.Value;
		if (Setting->GetValueAsString() != Setting->GetSavedValueAsString())
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
	
	SaveToDisk();
	UE_LOG(LogTemp, Log, TEXT("Settings reset to defaults."));
}



void UEFModularSettingsSubsystem::SaveToDisk()
{
	FString ConfigContent;
	TMap<FName, TArray<UEFModularSettingsBase*>> SettingsByCategory;
	
	for (const auto& SettingPair : Settings)
	{
		UEFModularSettingsBase* Setting = SettingPair.Value;
		SettingsByCategory.FindOrAdd(Setting->ConfigCategory).Add(Setting);
	}
	
	ConfigContent += TEXT("[/Settings/Meta]\n");
	ConfigContent += TEXT("ConfigVersion=1.0\n");
	ConfigContent += FString::Printf(TEXT("LastSaved=%s\n"), *FDateTime::Now().ToString());
	ConfigContent += FString::Printf(TEXT("Platform=%s\n\n"), ANSI_TO_TCHAR(FPlatformProperties::PlatformName()));
	
	for (const auto& CategoryPair : SettingsByCategory)
	{
		FName CategoryName = CategoryPair.Key;
		const TArray<UEFModularSettingsBase*>& CategorySettings = CategoryPair.Value;
		
		ConfigContent += FString::Printf(TEXT("[/Settings/%s]\n"), *CategoryName.ToString());
		
		for (UEFModularSettingsBase* Setting : CategorySettings)
		{
			FString SettingLine = FString::Printf(TEXT("%s=%s\n"), 
				*Setting->SettingTag.ToString(), 
				*Setting->GetValueAsString());
			ConfigContent += SettingLine;
		}
		ConfigContent += TEXT("\n");
	}
	
	FString ConfigPath = GetConfigFilePath();
	FFileHelper::SaveStringToFile(ConfigContent, *ConfigPath);
	
	UE_LOG(LogTemp, Log, TEXT("Modular settings saved to: %s"), *ConfigPath);
}

void UEFModularSettingsSubsystem::LoadFromDisk()
{
	FString ConfigPath = GetConfigFilePath();
	FString ConfigContent;
	
	if (FFileHelper::LoadFileToString(ConfigContent, *ConfigPath))
	{
		TArray<FString> Lines;
		ConfigContent.ParseIntoArray(Lines, TEXT("\n"), true);
		
		FString CurrentSection;
		for (const FString& Line : Lines)
		{
			FString TrimmedLine = Line.TrimStartAndEnd();
			if (TrimmedLine.IsEmpty() || TrimmedLine.StartsWith(TEXT(";")))
			{
				continue;
			}
			
			if (TrimmedLine.StartsWith(TEXT("[")) && TrimmedLine.EndsWith(TEXT("]")))
			{
				CurrentSection = TrimmedLine.Mid(1, TrimmedLine.Len() - 2);
				continue;
			}
			
			FString Key, Value;
			if (TrimmedLine.Split(TEXT("="), &Key, &Value))
			{
				Key = Key.TrimStartAndEnd();
				Value = Value.TrimStartAndEnd();
				
				if (CurrentSection.StartsWith(TEXT("/Settings/")) && !CurrentSection.Contains(TEXT("Meta")))
				{
					FGameplayTag Tag = FGameplayTag::RequestGameplayTag(*Key);
					if (UEFModularSettingsBase* Setting = Settings.FindRef(Tag))
					{
						Setting->SetValueFromString(Value);
					}
				}
			}
		}
		
		UE_LOG(LogTemp, Log, TEXT("Modular settings loaded from: %s"), *ConfigPath);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("No existing config file found. Using defaults."));
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

void UEFModularSettingsSubsystem::CopySettingValue(UEFModularSettingsBase* From, UEFModularSettingsBase* To)
{
	if (From && To)
	{
		FString Value = From->GetValueAsString();
		To->SetValueFromString(Value);
	}
}

FString UEFModularSettingsSubsystem::GetConfigFilePath() const
{
	return FPaths::ProjectSavedDir() / TEXT("Config") / TEXT("ModularSettings.ini");
}

