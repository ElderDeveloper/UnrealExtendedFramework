// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Data/EFSettingsData.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "EFSettingsSubsystem.generated.h"

class USoundMix;
class UEFAudioDeviceManager;
struct FEFAudioDeviceInfo;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnExtendedSettingsChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnExtendedSettingsApplied);

UCLASS(Config=GameUserSettings, ProjectUserConfig)
class UNREALEXTENDEDFRAMEWORK_API UEFDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:

	UPROPERTY(Config, EditAnywhere,BlueprintReadWrite, Category = "Settings" )
	FExtendedGameplaySettings GameplaySettings;

	UPROPERTY(Config, EditAnywhere,BlueprintReadWrite, Category = "Settings" )
	FExtendedGraphicsSettings GraphicsSettings;

	UPROPERTY(Config, EditAnywhere,BlueprintReadWrite, Category = "Settings" )
	FExtendedDisplaySettings DisplaySettings;

	UPROPERTY(Config, EditAnywhere,BlueprintReadWrite, Category = "Audio" )
	FExtendedAudioSettings AudioSettings;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Settings")
	uint8 DifficultyLevel;
	
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Settings")
	TSoftObjectPtr<UDataTable> DifficultySettingsTable;
	
	UPROPERTY(Config)
	bool bCheckedBestSettings = false;

	void SaveExtendedSettings()
	{
		SaveConfig();
	}
	
};


UCLASS(Config=Game, DefaultConfig)
class UNREALEXTENDEDFRAMEWORK_API UEFAudioDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TSoftObjectPtr<USoundMix> GlobalSoundMix;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TSoftObjectPtr<USoundClass> MasterSoundClass;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TSoftObjectPtr<USoundClass> MusicSoundClass;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TSoftObjectPtr<USoundClass> EffectsSoundClass;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TSoftObjectPtr<USoundClass> VoiceSoundClass;
};


UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFSettingsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintPure, Category = "Extended Settings")
    UEFAudioDeviceManager* GetAudioDeviceManager() const { return EFAudioDeviceManager; }

	UFUNCTION(BlueprintPure, Category = "Extended Settings")
	FExtendedGameplaySettings GetGameplaySettings() const	{ return TemporaryGameplaySettings;	}

	UFUNCTION(BlueprintPure, Category = "Extended Settings")
	FExtendedAudioSettings GetAudioSettings() const { return TemporaryAudioSettings;	}

	UFUNCTION(BlueprintPure, Category = "Extended Settings")
	FExtendedGraphicsSettings GetGraphicsSettings() const	{ return TemporaryGraphicsSettings;	}

	UFUNCTION(BlueprintPure, Category = "Extended Settings")
	FExtendedDisplaySettings GetDisplaySettings() const { return TemporaryDisplaySettings; }

	UFUNCTION(BlueprintPure, Category = "Extended Settings")
	UEFDeveloperSettings* GetExtendedSettings() const	{ return ExtendedSettings;	}

	UFUNCTION(BlueprintCallable, Category = "Extended Settings")
	void SetGameplaySettings(const FExtendedGameplaySettings& GameplaySettings);

	UFUNCTION(BlueprintCallable, Category = "Extended Settings")
	void SetAudioSettings(const FExtendedAudioSettings& AudioSettings);

	UFUNCTION(BlueprintCallable, Category = "Extended Settings")
	void SetGraphicsSettings(const FExtendedGraphicsSettings& GraphicsSettings);

	UFUNCTION(BlueprintCallable, Category = "Extended Settings")
	void SetDisplaySettings(const FExtendedDisplaySettings& DisplaySettings);

	UFUNCTION(BlueprintCallable, Category = "Extended Settings")
	void SaveExtendedSettings() const;

	UFUNCTION(BlueprintCallable, Category = "Extended Settings")
	void FindAndApplyBestSettings();

	// Apply settings
	UFUNCTION(BlueprintCallable, Category = "Extended Settings")
	void ApplySettings();

	// Apply settings to the game engine
	UFUNCTION(BlueprintCallable, Category = "Extended Settings")
	void ApplyGameplaySettings(const FExtendedGameplaySettings& Settings);

	UFUNCTION(BlueprintCallable, Category = "Extended Settings")
	void ApplyAudioSettings(const FExtendedAudioSettings& Settings);

	UFUNCTION(BlueprintCallable, Category = "Extended Settings")
	void ApplyGraphicsSettings(const FExtendedGraphicsSettings& Settings);

	UFUNCTION(BlueprintCallable, Category = "Extended Settings")
	void ApplyDisplaySettings(const FExtendedDisplaySettings& Settings);

	// Revert settings to their current state
	UFUNCTION(BlueprintCallable, Category = "Extended Settings")
	void RevertSettings();

    // Audio device event handlers
    UFUNCTION()
    void OnAudioDevicesChanged(const TArray<FEFAudioDeviceInfo>& AudioDevices);
    
    UFUNCTION()
    void OnAudioDeviceDisconnected(const FEFAudioDeviceInfo& DisconnectedDevice);
    
    // Update audio device lists in settings
    UFUNCTION(BlueprintCallable, Category = "Extended Settings")
    void UpdateAudioDeviceLists();
    
    // Update screen resolution list in settings
    UFUNCTION(BlueprintCallable, Category = "Extended Settings")
    void UpdateScreenResolutionList(bool bForceBestResolution = false);

protected:
	UPROPERTY()
	UEFDeveloperSettings* ExtendedSettings;

	UPROPERTY()
	UEFAudioDeveloperSettings* AudioMixer;

	// Store temporary settings for applying
	FExtendedGameplaySettings TemporaryGameplaySettings;
	FExtendedAudioSettings TemporaryAudioSettings;
	FExtendedGraphicsSettings TemporaryGraphicsSettings;
	FExtendedDisplaySettings TemporaryDisplaySettings;

	// Store temporary settings before applying new ones
	void StoreTemporarySettings();
	virtual void PostInitProperties() override;
private:
	UPROPERTY()
	UEFAudioDeviceManager* EFAudioDeviceManager;

public:
	UPROPERTY( BlueprintAssignable, Category = "Extended Settings")
	FOnExtendedSettingsChanged OnExtendedSettingsChanged;

	UPROPERTY( BlueprintAssignable, Category = "Extended Settings")
	FOnExtendedSettingsApplied OnExtendedSettingsApplied;
};


