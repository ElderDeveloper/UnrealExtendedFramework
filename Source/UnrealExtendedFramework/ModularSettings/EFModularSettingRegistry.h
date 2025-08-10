#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Settings/EFModularSettingBase.h"
#include "EFModularSettingRegistry.generated.h"

UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFModularSettingRegistry : public UObject
{
	GENERATED_BODY()
	
public:
	// Singleton access
	static UEFModularSettingRegistry* Get();
	
	// Registration methods
	void RegisterSetting(UEFModularSettingBase* Setting);
	void UnregisterSetting(FGameplayTag SettingTag);
	
	// Discovery methods
	void DiscoverAllSettings();
	void DiscoverSettingsInModule(const FString& ModuleName);
	
	// Query methods
	TArray<UEFModularSettingBase*> GetSettingsByCategory(FName Category) const;
	TArray<UEFModularSettingBase*> GetAllSettings() const;
	UEFModularSettingBase* FindSetting(FGameplayTag SettingTag) const;
	
	// Validation
	bool ValidateAllSettings() const;
	TArray<FString> GetValidationErrors() const;
	
	// Initialization
	void InitializeRegistry();
	void ShutdownRegistry();
	
private:
	// Singleton instance
	static UEFModularSettingRegistry* Instance;
	
	// Storage
	UPROPERTY()
	TMap<FGameplayTag, TObjectPtr<UEFModularSettingBase>> RegisteredSettings;
	
	// Helper methods
	void CreateDefaultInstancesOfSettingClasses();
	void ValidateSettingConfiguration(UEFModularSettingBase* Setting, TArray<FString>& OutErrors) const;
	
	// Prevent direct instantiation
	UEFModularSettingRegistry() = default;
	virtual ~UEFModularSettingRegistry() = default;
};
