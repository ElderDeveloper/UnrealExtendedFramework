// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "HAL/IConsoleManager.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "Engine/DeveloperSettings.h"
#include "Settings/EFDeveloperSettings.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Settings/EFModularSettingsBase.h"
#include "UnrealExtendedFramework/ModularSettings/Device/EFModularSettingsDeviceRules.h"
#include "EFModularSettingsSubsystem.generated.h"

class UEFSettingsSaveGame;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSettingsSaved);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSettingsLoaded);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSettingsChanged, UEFModularSettingsBase*, Setting);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDeviceRulesChanged);



UCLASS(BlueprintType, meta = (DisplayName = "Modular Settings Container"))
class UNREALEXTENDEDFRAMEWORK_API UEFModularSettingsContainer : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Instanced, Category = "Modular Settings")
	TArray<TObjectPtr<UEFModularSettingsBase>> Settings;
};

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Extended Settings"))
class UNREALEXTENDEDFRAMEWORK_API UEFModularProjectSettings : public UEFDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Config, Category = "Modular Settings", meta = (AllowedClasses = "/Script/UnrealExtendedFramework.EFModularSettingsContainer"))
	TArray<TSoftObjectPtr<UEFModularSettingsContainer>> LocalSettingsContainers;

	UPROPERTY(EditDefaultsOnly, Config, Category = "Modular Settings", meta = (AllowedClasses = "/Script/UnrealExtendedFramework.EFModularSettingsContainer"))
	TArray<TSoftObjectPtr<UEFModularSettingsContainer>> WorldSettingsContainers;

	UPROPERTY(EditDefaultsOnly, Config, Category = "Modular Settings", meta = (AllowedClasses = "/Script/UnrealExtendedFramework.EFModularSettingsContainer"))
	TArray<TSoftObjectPtr<UEFModularSettingsContainer>> PlayerSettingsContainers;

	/**
	 * Local settings that belong to the device rather than the player, as gameplay tags; a tag also
	 * covers its children (Settings.Graphics covers every graphics setting). They are saved per device
	 * family, so a PC and a Steam Deck sharing one cloud save each keep their own. A setting's own Scope
	 * overrides this list. List exact tags where a parent would catch personal settings too (FOV and
	 * language usually sit beside display settings).
	 */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Device Rules")
	TArray<FString> DeviceScopedSettings;

	/**
	 * Read [<Profile> ModularSettingsDeviceRules] sections from DeviceProfiles.ini and apply them to
	 * local settings (defaults, availability, options, ranges). Off, every setting keeps its authored
	 * default and stays visible.
	 */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Device Rules")
	bool bApplyDeviceRules = true;
};


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
	
	// True if the tag exists in any scope: Player (PlayerState component), World
	// (GameState component), or Local (this subsystem). Only local settings are
	// stored here; the other scopes are resolved through the current world.
	UFUNCTION(BlueprintCallable, Category = "Modular Settings")
	bool HasSetting(FGameplayTag Tag) const;

	// True only if the tag is a Local setting owned by this subsystem.
	UFUNCTION(BlueprintCallable, Category = "Modular Settings")
	bool HasLocalSetting(FGameplayTag Tag) const { return Settings.Contains(Tag); }

	// Register setting (called by registry)
	void RegisterSetting(UEFModularSettingsBase* Setting);

	// Get setting by category
	UFUNCTION(BlueprintCallable, Category = "Modular Settings")
	TArray<UEFModularSettingsBase*> GetSettingsByCategory(FName Category) const;

	UPROPERTY(BlueprintAssignable, Category = "Modular Settings")
	FOnSettingsSaved OnSettingsSaved;

	UPROPERTY(BlueprintAssignable, Category = "Modular Settings")
	FOnSettingsLoaded OnSettingsLoaded;

	/**
	 * The device rules were resolved again (device profile switch, or machine capabilities known once
	 * the PostEngineInit phase completed): availability, offered options and defaults may have changed.
	 * Settings pages rebuild their rows on this. OnSettingsChanged has already fired for every setting.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Modular Settings|Device")
	FOnDeviceRulesChanged OnDeviceRulesChanged;

	/** The device profile the rules were resolved for. */
	UFUNCTION(BlueprintPure, Category = "Modular Settings|Device")
	FString GetActiveDeviceProfileName() const { return DeviceRules.ActiveProfile; }

	/** The save bucket device-scoped settings use on this device ("Desktop", "SteamDeck"). */
	UFUNCTION(BlueprintPure, Category = "Modular Settings|Device")
	FString GetDeviceFamily() const { return DeviceRules.DeviceFamily; }

	/** Rule sections applied because a hardware rule matched this machine. */
	UFUNCTION(BlueprintPure, Category = "Modular Settings|Device")
	TArray<FString> GetMatchedHardwareRuleSections() const { return DeviceRules.HardwareSections; }

	/** Everything the device rules resolved to. */
	const FEFResolvedDeviceRules& GetDeviceRules() const { return DeviceRules; }

	/** The hardware the rules were evaluated against. */
	const FEFDeviceHardwareInfo& GetDeviceHardware() const { return DeviceRules.Hardware; }

	/** Resolves the device rules again and re-applies every local setting. */
	UFUNCTION(BlueprintCallable, Category = "Modular Settings|Device")
	void RefreshDeviceRules();

	/** Whether a setting is saved per device family (see UEFModularProjectSettings::DeviceScopedSettings). */
	UFUNCTION(BlueprintPure, Category = "Modular Settings|Device")
	bool IsDeviceScoped(const UEFModularSettingsBase* Setting) const;

	/* Storage */
	UPROPERTY()
	TMap<FGameplayTag, TObjectPtr<UEFModularSettingsBase>> Settings;
	
	// Snapshot Storage for reverting after apply
	UPROPERTY()
	TMap<FGameplayTag, FString> PreviousSettingsSnapshot;
	
public:
	// Typed lookup across all scopes (see GetSettingByTag).
	template<typename T>
	T* GetSetting(FGameplayTag Tag) const
	{
		return Cast<T>(GetSettingByTag(Tag));
	}

	// Finds the setting in any scope: Player (PlayerState component), World
	// (GameState component), then Local (this subsystem). Only local settings
	// are stored here; the other scopes are resolved through the current world.
	UFUNCTION(BlueprintCallable, Category = "Modular Settings")
	UEFModularSettingsBase* GetSettingByTag(FGameplayTag Tag) const;

	// Local-scope only: the settings owned by this subsystem. Use this for
	// anything that reads or writes the local save (console commands, the
	// library's Local source) so a same-named Player/World setting is not
	// mutated outside its component's replication path.
	UFUNCTION(BlueprintCallable, Category = "Modular Settings")
	UEFModularSettingsBase* GetLocalSettingByTag(FGameplayTag Tag) const
	{
		return Settings.FindRef(Tag);
	}

private:
	/** Takes a save read from disk (or none): migrates version 1, remembers every bucket, applies values. */
	void ProcessLoadedSave(UEFSettingsSaveGame* LoadedGame);
	void HandleAsyncSaveComplete(const FString& SlotName, int32 UserIndex, bool bSuccess);
	void HandleAsyncLoadComplete(const FString& SlotName, int32 UserIndex, class USaveGame* LoadedGame);

	/** Keeps the version 1 file as ModularSettingsSave_v1, then keeps the user-scoped values that are still valid. */
	void MigrateVersion1Save(const UEFSettingsSaveGame& LoadedGame);
	void BackupVersion1Save() const;

	/** A version 2 save of the current state: sparse, user values plus every device family's bucket. */
	UEFSettingsSaveGame* BuildSaveGame();

	/** What to write for a setting: false when nothing should be written (the value is the default). */
	bool DetermineValueToWrite(const UEFModularSettingsBase* Setting, FString& OutValue);

	/** The value the loaded save holds for a setting, from the bucket the setting belongs to. */
	const FString* FindStoredValue(const UEFModularSettingsBase* Setting) const;

	void ResolveDeviceRules();
	void ApplyDeviceRulesToSettings();
	void ResolveSettingDefaults();
	void ApplyStoredValues();
	void ApplyAllSettingsInOrder();
	TArray<UEFModularSettingsBase*> GetSettingsInApplyOrder() const;

	/** Re-resolve after a device profile switch or once machine capabilities are known. */
	void ReapplyDeviceRules(const TCHAR* Reason, bool bSaveCurrentFamilyFirst);
	void HandleActiveDeviceProfileChanged();
	void ScheduleDeviceRulesReapply(const TCHAR* Reason, bool bSaveCurrentFamilyFirst);

	void HandleDeviceDumpCommand(const TArray<FString>& Args);

	/** Resolved device rules for the active device profile. */
	FEFResolvedDeviceRules DeviceRules;

	/** User-scoped values as last read from or written to disk. */
	TMap<FGameplayTag, FString> LoadedUserValues;

	/** Every device family's device-scoped values as last read from or written to disk. */
	TMap<FString, TMap<FGameplayTag, FString>> LoadedDeviceBuckets;

	/** Saved values this device cannot apply (Hidden/Locked setting, hidden option); written back unchanged. */
	TMap<FGameplayTag, FString> HeldValues;

	/** Device-scoped tags from the project settings. */
	TArray<FGameplayTag> DeviceScopedTags;

	/**
	 * Set while saved values are being put into the settings. A preset applied on the way (Overall
	 * Quality pushing its sub-settings, possibly with immediate apply) can ask for a save; writing then
	 * would store a half-loaded state and overwrite the values still to be read, so saves wait.
	 */
	bool bApplyingStoredValues = false;

	FDelegateHandle DeviceProfileChangedHandle;
	FDelegateHandle LoadingPhaseHandle;
	FTSTicker::FDelegateHandle PendingReapplyHandle;
	bool bPendingReapplySavesFirst = false;
	FString PendingReapplyReason;

	void RegisterConsoleCommands();
	void UnregisterConsoleCommands();
	void RegisterConsoleCommand(const TCHAR* Name, const TCHAR* Help, const FConsoleCommandWithArgsDelegate& Delegate);
	bool ApplyConsoleSettingValue(FGameplayTag Tag, const FString& ValueString, const TCHAR* CommandLabel);
	void LogSettingValue(FGameplayTag Tag, const TCHAR* Label) const;
	void LogUpscalerStatus() const;

	void HandleSetCommand(const TArray<FString>& Args);
	void HandleUpscalerSetCommand(const TArray<FString>& Args);
	void HandleUpscalerDLSSModeCommand(const TArray<FString>& Args);
	void HandleUpscalerDLSSRayReconstructionCommand(const TArray<FString>& Args);
	void HandleUpscalerFSRModeCommand(const TArray<FString>& Args);
	void HandleUpscalerFSRSharpnessCommand(const TArray<FString>& Args);
	void HandleUpscalerXeSSQualityModeCommand(const TArray<FString>& Args);
	void HandleUpscalerFrameGenerationCommand(const TArray<FString>& Args);
	void HandleUpscalerResolutionScaleCommand(const TArray<FString>& Args);
	void HandleUpscalerStatusCommand(const TArray<FString>& Args);
	void HandleDisplayTestAspectCommand(const TArray<FString>& Args);

	TArray<FString> RegisteredConsoleCommands;
};
