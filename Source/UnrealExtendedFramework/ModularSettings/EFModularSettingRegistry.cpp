#include "EFModularSettingRegistry.h"
#include "EFModularSettingsSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UObject/UObjectIterator.h"

// Static instance
UEFModularSettingRegistry* UEFModularSettingRegistry::Instance = nullptr;

UEFModularSettingRegistry* UEFModularSettingRegistry::Get()
{
	if (!Instance)
	{
		Instance = NewObject<UEFModularSettingRegistry>();
		Instance->AddToRoot(); // Prevent garbage collection
		Instance->InitializeRegistry();
	}
	return Instance;
}

void UEFModularSettingRegistry::InitializeRegistry()
{
	UE_LOG(LogTemp, Log, TEXT("Initializing Modular Settings Registry..."));
	
	// Discover all setting classes
	DiscoverAllSettings();
	
	// Validate all discovered settings
	if (!ValidateAllSettings())
	{
		UE_LOG(LogTemp, Error, TEXT("Setting validation failed during registry initialization."));
		TArray<FString> Errors = GetValidationErrors();
		for (const FString& Error : Errors)
		{
			UE_LOG(LogTemp, Error, TEXT("Setting Error: %s"), *Error);
		}
	}
	
	// Register with subsystem if available
	if (UWorld* World = GEngine->GetWorldFromContextObject(this, EGetWorldErrorMode::LogAndReturnNull))
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			if (UEFModularSettingsSubsystem* Subsystem = GameInstance->GetSubsystem<UEFModularSettingsSubsystem>())
			{
				for (const auto& SettingPair : RegisteredSettings)
				{
					Subsystem->RegisterSetting(SettingPair.Value);
				}
			}
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("Registry initialized with %d settings."), RegisteredSettings.Num());
}

void UEFModularSettingRegistry::ShutdownRegistry()
{
	RegisteredSettings.Empty();
	
	if (Instance)
	{
		Instance->RemoveFromRoot();
		Instance = nullptr;
	}
	
	UE_LOG(LogTemp, Log, TEXT("Modular Settings Registry shut down."));
}

void UEFModularSettingRegistry::DiscoverAllSettings()
{
	RegisteredSettings.Empty();
	
	// Find all UClass objects that inherit from UEFModularSettingBase
	for (TObjectIterator<UClass> ClassIterator; ClassIterator; ++ClassIterator)
	{
		UClass* Class = *ClassIterator;
		
		// Check if this class inherits from UEFModularSettingBase and is not abstract
		if (Class->IsChildOf(UEFModularSettingBase::StaticClass()) && 
			!Class->HasAnyClassFlags(CLASS_Abstract) &&
			Class != UEFModularSettingBase::StaticClass())
		{
			// Create a default instance of this setting class
			UEFModularSettingBase* SettingInstance = NewObject<UEFModularSettingBase>(this, Class);
			
			if (SettingInstance && SettingInstance->SettingTag.IsValid())
			{
				RegisterSetting(SettingInstance);
				UE_LOG(LogTemp, Log, TEXT("Discovered setting class: %s with tag: %s"), *Class->GetName(), *SettingInstance->SettingTag.ToString());
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("Setting class %s has invalid or missing SettingTag."), *Class->GetName());
			}
		}
	}
}

void UEFModularSettingRegistry::DiscoverSettingsInModule(const FString& ModuleName)
{
	// This could be extended to discover settings only in specific modules
	// For now, we'll just call the full discovery
	DiscoverAllSettings();
}

void UEFModularSettingRegistry::RegisterSetting(UEFModularSettingBase* Setting)
{
	if (!Setting || !Setting->SettingTag.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Attempted to register invalid setting."));
		return;
	}
	
	if (RegisteredSettings.Contains(Setting->SettingTag))
	{
		UE_LOG(LogTemp, Warning, TEXT("Setting with tag %s is already registered. Overwriting."), 
			*Setting->SettingTag.ToString());
	}
	
	RegisteredSettings.Add(Setting->SettingTag, Setting);
	UE_LOG(LogTemp, Log, TEXT("Registered setting: %s"), *Setting->SettingTag.ToString());
}

void UEFModularSettingRegistry::UnregisterSetting(FGameplayTag SettingTag)
{
	if (RegisteredSettings.Remove(SettingTag))
	{
		UE_LOG(LogTemp, Log, TEXT("Unregistered setting: %s"), *SettingTag.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Attempted to unregister non-existent setting: %s"), *SettingTag.ToString());
	}
}

TArray<UEFModularSettingBase*> UEFModularSettingRegistry::GetSettingsByCategory(FName Category) const
{
	TArray<UEFModularSettingBase*> Result;
	
	for (const auto& SettingPair : RegisteredSettings)
	{
		if (SettingPair.Value->ConfigCategory == Category)
		{
			Result.Add(SettingPair.Value);
		}
	}
	
	return Result;
}

TArray<UEFModularSettingBase*> UEFModularSettingRegistry::GetAllSettings() const
{
	TArray<TObjectPtr<UEFModularSettingBase>> Results;
	RegisteredSettings.GenerateValueArray(Results);
	return Results;
}

UEFModularSettingBase* UEFModularSettingRegistry::FindSetting(FGameplayTag SettingTag) const
{
	return RegisteredSettings.FindRef(SettingTag);
}

bool UEFModularSettingRegistry::ValidateAllSettings() const
{
	TArray<FString> Errors;
	
	for (const auto& SettingPair : RegisteredSettings)
	{
		ValidateSettingConfiguration(SettingPair.Value, Errors);
	}
	
	return Errors.Num() == 0;
}

TArray<FString> UEFModularSettingRegistry::GetValidationErrors() const
{
	TArray<FString> Errors;
	
	for (const auto& SettingPair : RegisteredSettings)
	{
		ValidateSettingConfiguration(SettingPair.Value, Errors);
	}
	
	return Errors;
}

void UEFModularSettingRegistry::ValidateSettingConfiguration(UEFModularSettingBase* Setting, TArray<FString>& OutErrors) const
{
	if (!Setting)
	{
		OutErrors.Add(TEXT("Null setting found in registry."));
		return;
	}
	
	// Validate SettingTag
	if (!Setting->SettingTag.IsValid())
	{
		OutErrors.Add(FString::Printf(TEXT("Setting %s has invalid SettingTag."), *Setting->GetClass()->GetName()));
	}
	
	// Validate DisplayName
	if (Setting->DisplayName.IsEmpty())
	{
		OutErrors.Add(FString::Printf(TEXT("Setting %s has empty DisplayName."), *Setting->SettingTag.ToString()));
	}
	
	// Validate ConfigCategory
	if (Setting->ConfigCategory.IsNone())
	{
		OutErrors.Add(FString::Printf(TEXT("Setting %s has invalid ConfigCategory."), *Setting->SettingTag.ToString()));
	}
	
	// Type-specific validation
	if (UEFModularFloatSetting* FloatSetting = Cast<UEFModularFloatSetting>(Setting))
	{
		if (FloatSetting->Min >= FloatSetting->Max)
		{
			OutErrors.Add(FString::Printf(TEXT("Float setting %s has invalid Min/Max range."), *Setting->SettingTag.ToString()));
		}
	}
	
	if (UEFModularMultiSelectSetting* MultiSelectSetting = Cast<UEFModularMultiSelectSetting>(Setting))
	{
		if (MultiSelectSetting->Values.Num() == 0)
		{
			OutErrors.Add(FString::Printf(TEXT("Multi-select setting %s has no values."), *Setting->SettingTag.ToString()));
		}
		
		if (MultiSelectSetting->Values.Num() != MultiSelectSetting->DisplayNames.Num())
		{
			OutErrors.Add(FString::Printf(TEXT("Multi-select setting %s has mismatched Values and DisplayNames arrays."), *Setting->SettingTag.ToString()));
		}
	}
}

void UEFModularSettingRegistry::CreateDefaultInstancesOfSettingClasses()
{
	// This helper method creates default instances of all setting classes
	// It's called during discovery to ensure all settings are available
	DiscoverAllSettings();
}
