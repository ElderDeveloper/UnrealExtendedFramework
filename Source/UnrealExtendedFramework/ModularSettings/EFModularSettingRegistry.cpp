#include "EFModularSettingRegistry.h"
#include "EFModularSettingsSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UObject/UObjectIterator.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/AssetManager.h"
#include "Engine/Blueprint.h"

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

	// Note: Settings discovery is now handled by the subsystem during its initialization
	// This ensures proper world context and initialization order

	UE_LOG(LogTemp, Log, TEXT("Registry initialized."));
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
	
	// Find all UClass objects that inherit from UEFModularSettingsBase
	for (TObjectIterator<UClass> ClassIterator; ClassIterator; ++ClassIterator)
	{
		UClass* Class = *ClassIterator;
		
		// Check if this class inherits from UEFModularSettingsBase and is not abstract
		if (Class->IsChildOf(UEFModularSettingsBase::StaticClass()) && 
			!Class->HasAnyClassFlags(CLASS_Abstract) &&
			Class != UEFModularSettingsBase::StaticClass())
		{
			// Create a default instance of this setting class
			UEFModularSettingsBase* SettingInstance = NewObject<UEFModularSettingsBase>(this, Class);
			
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

	// Now discover Blueprint settings using Asset Registry
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	// Ensure asset registry is loaded
	AssetRegistry.SearchAllAssets(true);

	TArray<FAssetData> AssetDataList;
	FARFilter Filter;
	Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;

	AssetRegistry.GetAssets(Filter, AssetDataList);

	for (const FAssetData& AssetData : AssetDataList)
	{
		// Check if this blueprint's parent class is UEFModularSettingsBase
		const FString ParentClassPath = AssetData.GetTagValueRef<FString>(FBlueprintTags::ParentClassPath);
  
		if (ParentClassPath.Contains(TEXT("EFModularSettingBase")))
		{
			// Load the blueprint asset
			UBlueprint* Blueprint = Cast<UBlueprint>(AssetData.GetAsset());
   
			if (Blueprint && Blueprint->GeneratedClass)
			{
				UClass* GeneratedClass = Blueprint->GeneratedClass;
    
				if (GeneratedClass->IsChildOf(UEFModularSettingsBase::StaticClass()) &&
				 !GeneratedClass->HasAnyClassFlags(CLASS_Abstract))
				{
					// Create an instance of the blueprint class
					UEFModularSettingsBase* SettingInstance = NewObject<UEFModularSettingsBase>(this, GeneratedClass);
     
					if (SettingInstance && SettingInstance->SettingTag.IsValid())
					{
						RegisterSetting(SettingInstance);
						UE_LOG(LogTemp, Log, TEXT("Discovered Blueprint setting: %s with tag: %s"), 
						 *Blueprint->GetName(), *SettingInstance->SettingTag.ToString());
					}
					else
					{
						UE_LOG(LogTemp, Warning, TEXT("Blueprint setting %s has invalid or missing SettingTag."), 
						 *Blueprint->GetName());
					}
				}
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("Settings discovery complete. Total registered: %d"), RegisteredSettings.Num());
}

void UEFModularSettingRegistry::DiscoverSettingsInModule(const FString& ModuleName)
{
	// This could be extended to discover settings only in specific modules
	// For now, we'll just call the full discovery
	DiscoverAllSettings();
}

void UEFModularSettingRegistry::RegisterSetting(UEFModularSettingsBase* Setting)
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

TArray<UEFModularSettingsBase*> UEFModularSettingRegistry::GetSettingsByCategory(FName Category) const
{
	TArray<UEFModularSettingsBase*> Result;
	
	for (const auto& SettingPair : RegisteredSettings)
	{
		if (SettingPair.Value->ConfigCategory == Category)
		{
			Result.Add(SettingPair.Value);
		}
	}
	
	return Result;
}

TArray<UEFModularSettingsBase*> UEFModularSettingRegistry::GetAllSettings() const
{
	TArray<TObjectPtr<UEFModularSettingsBase>> Results;
	RegisteredSettings.GenerateValueArray(Results);
	return Results;
}

UEFModularSettingsBase* UEFModularSettingRegistry::FindSetting(FGameplayTag SettingTag) const
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

void UEFModularSettingRegistry::ValidateSettingConfiguration(UEFModularSettingsBase* Setting, TArray<FString>& OutErrors) const
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
	if (UEFModularSettingsFloat* FloatSetting = Cast<UEFModularSettingsFloat>(Setting))
	{
		if (FloatSetting->Min >= FloatSetting->Max)
		{
			OutErrors.Add(FString::Printf(TEXT("Float setting %s has invalid Min/Max range."), *Setting->SettingTag.ToString()));
		}
	}
	
	if (UEFModularSettingsMultiSelect* MultiSelectSetting = Cast<UEFModularSettingsMultiSelect>(Setting))
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
