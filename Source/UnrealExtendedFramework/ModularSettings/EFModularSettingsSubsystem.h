// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Settings/EFModularSettingsBase.h"
#include "EFModularSettingsSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSettingsSaved);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSettingsLoaded);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSettingsChanged, UEFModularSettingsBase*, Setting);



UCLASS(BlueprintType, meta = (DisplayName = "Modular Settings Container"))
class UNREALEXTENDEDFRAMEWORK_API UEFModularSettingsContainer : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Instanced, Category = "Modular Settings")
	TArray<TObjectPtr<UEFModularSettingsBase>> Settings;
};

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Extended Settings"))
class UNREALEXTENDEDFRAMEWORK_API UEFModularProjectSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Config, Category = "Modular Settings", meta = (AllowedClasses = "/Script/UnrealExtendedFramework.EFModularSettingsContainer"))
	TArray<TSoftObjectPtr<UEFModularSettingsContainer>> LocalSettingsContainers;

	UPROPERTY(EditDefaultsOnly, Config, Category = "Modular Settings", meta = (AllowedClasses = "/Script/UnrealExtendedFramework.EFModularSettingsContainer"))
	TArray<TSoftObjectPtr<UEFModularSettingsContainer>> WorldSettingsContainers;

	UPROPERTY(EditDefaultsOnly, Config, Category = "Modular Settings", meta = (AllowedClasses = "/Script/UnrealExtendedFramework.EFModularSettingsContainer"))
	TArray<TSoftObjectPtr<UEFModularSettingsContainer>> PlayerSettingsContainers;
};


UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFModularSettingsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintAssignable, Category = "Modular Settings")
	FOnSettingsChanged OnSettingsChanged;
	
	virtual void Initialize(FSubsystemCollectionBase&) override;

	// Basic getters/setters
	// New method to apply all pending changes
	UFUNCTION(BlueprintCallable, Category = "Modular Settings")
	void ApplyAllChanges();

	// New method to revert pending (unapplied) changes
	UFUNCTION(BlueprintCallable, Category = "Modular Settings")
	void RevertPendingChanges();

	// New method to restore from the last applied snapshot
	UFUNCTION(BlueprintCallable, Category = "Modular Settings")
	void RevertToPreviousSettings();
	
	UFUNCTION(BlueprintCallable, Category = "Modular Settings")
	void RefreshAllSettings();

	// Check if there are unapplied changes
	UFUNCTION(BlueprintCallable, Category = "Modular Settings")
	bool HasPendingChanges() const;

	// Default system
	UFUNCTION(BlueprintCallable, Category = "Modular Settings")
	void ResetToDefaults(FGameplayTag CategoryTag);
	
	// Persistence
	UFUNCTION(BlueprintCallable, Category = "Modular Settings")
	void SaveToDisk();
	
	UFUNCTION(BlueprintCallable, Category = "Modular Settings")
	void LoadFromDisk();

	UFUNCTION(BlueprintCallable, Category = "Modular Settings")
	void SaveToDiskAsync();

	UFUNCTION(BlueprintCallable, Category = "Modular Settings")
	void LoadFromDiskAsync();
	
	UFUNCTION(BlueprintCallable, Category = "Modular Settings")
	bool HasSetting(FGameplayTag Tag) const;

	// Register setting (called by registry)
	void RegisterSetting(UEFModularSettingsBase* Setting);

	// Get setting by category
	UFUNCTION(BlueprintCallable, Category = "Modular Settings")
	TArray<UEFModularSettingsBase*> GetSettingsByCategory(FName Category) const;

	UPROPERTY(BlueprintAssignable, Category = "Modular Settings")
	FOnSettingsSaved OnSettingsSaved;
	
	UPROPERTY(BlueprintAssignable, Category = "Modular Settings")
	FOnSettingsLoaded OnSettingsLoaded;
	
	/* Storage */
	UPROPERTY()
	TMap<FGameplayTag, TObjectPtr<UEFModularSettingsBase>> Settings;
	
	// Snapshot Storage for reverting after apply
	UPROPERTY()
	TMap<FGameplayTag, FString> PreviousSettingsSnapshot;

	// Console Command
	void HandleSetCommand(const TArray<FString>& Args);
	struct IConsoleCommand* SetCommand = nullptr;
	
public:
	// Template method moved to header
	template<typename T>
	T* GetSetting(FGameplayTag Tag) const
	{
		if (const UEFModularSettingsBase* Setting = Settings.FindRef(Tag))
		{
			return Cast<T>(const_cast<UEFModularSettingsBase*>(Setting));
		}
		return nullptr;
	}

	UFUNCTION(BlueprintCallable, Category = "Modular Settings")
	UEFModularSettingsBase* GetSettingByTag(FGameplayTag Tag) const
	{
		return Settings.FindRef(Tag);
	}
};
