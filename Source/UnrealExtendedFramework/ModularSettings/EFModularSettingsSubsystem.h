// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Settings/EFModularSettingsBase.h"
#include "EFModularSettingsSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSettingsSaved);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSettingsLoaded);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSettingsChanged, UEFModularSettingsBase*, Setting);

UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFModularSettingsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintAssignable, Category = "Modular Settings")
	FOnSettingsChanged OnSettingsChanged;
	
	virtual void Initialize(FSubsystemCollectionBase&) override;
	virtual void Deinitialize() override;

	// Basic getters/setters
	UFUNCTION(BlueprintCallable, Category = "Modular Settings")
	bool GetBool(FGameplayTag Tag) const;

	UFUNCTION(BlueprintCallable, Category = "Modular Settings")
	void SetBool(FGameplayTag Tag, bool bValue);

	UFUNCTION(BlueprintCallable, Category = "Modular Settings")
	float GetFloat(FGameplayTag Tag) const;

	UFUNCTION(BlueprintCallable, Category = "Modular Settings")
	void SetFloat(FGameplayTag Tag, float Value);

	UFUNCTION(BlueprintCallable, Category = "Modular Settings")
	int32 GetIndex(FGameplayTag Tag) const;

	UFUNCTION(BlueprintCallable, Category = "Modular Settings")
	void SetIndex(FGameplayTag Tag, int32 Index);

	UFUNCTION(BlueprintCallable, Category = "Modular Settings")
	void AddIndex(FGameplayTag Tag, int32 Amount);

	UFUNCTION(BlueprintCallable, Category = "Modular Settings")
	TArray<FText> GetOptions(FGameplayTag Tag) const;

	UFUNCTION(BlueprintCallable, Category = "Modular Settings")
	FText GetSelectedOption(FGameplayTag Tag) const;
	
	// Staging system
	UFUNCTION(BlueprintCallable, Category = "Modular Settings")
	void StageChanges();

	UFUNCTION(BlueprintCallable, Category = "Modular Settings")
	void ApplyStaged();

	UFUNCTION(BlueprintCallable, Category = "Modular Settings")
	void RevertStaged();

	UFUNCTION(BlueprintCallable, Category = "Modular Settings")
	bool HasStagedChanges() const;

	// Default system
	UFUNCTION(BlueprintCallable, Category = "Modular Settings")
	void ResetToDefaults(FGameplayTag CategoryTag);

	UFUNCTION(BlueprintCallable, Category = "Modular Settings")
	void SetAsUserDefault(FGameplayTag Tag);

	UFUNCTION(BlueprintCallable, Category = "Modular Settings")
	void LoadPlatformDefaults();
	
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
	
private:
	/* Storage */
	UPROPERTY()
	TMap<FGameplayTag, TObjectPtr<UEFModularSettingsBase>> Settings;

	/* Staging system */
	UPROPERTY()
	TMap<FGameplayTag, TObjectPtr<UEFModularSettingsBase>> StagedSettings;

	/* User defaults */
	UPROPERTY()
	TMap<FGameplayTag, FString> UserDefaults;

	bool bHasStagedChanges = false;

	// Helper methods
	void CopySettingValue(UEFModularSettingsBase* From, UEFModularSettingsBase* To);
	void SaveSettingToConfig(UEFModularSettingsBase* Setting);
	void LoadSettingFromConfig(UEFModularSettingsBase* Setting);
	FString GetConfigFilePath() const;
	
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

	UEFModularSettingsBase* GetSettingByTag(FGameplayTag Tag) const
	{
		return Settings.FindRef(Tag);
	}
};
