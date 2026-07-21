// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UnrealExtendedFramework/ModularSettings/Settings/EFModularSettingsBase.h"
#include "EFPlayerSettingsComponent.generated.h"



DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerSettingChanged,UEFModularSettingsBase *, Setting);

// For asset-based settings (loaded from containers/data assets)
USTRUCT(BlueprintType)
struct FEFPlayerSettingDefinition {
  GENERATED_BODY()

  // Unique key for the setting
  UPROPERTY(EditAnywhere, BlueprintReadWrite)
  FGameplayTag Tag;

  // A template object (typically from a data asset/container) that will be
  // duplicated on both server and clients. Replicating a soft object path lets
  // clients resolve it and DuplicateObject to build the same subobject graph.
  UPROPERTY(EditAnywhere, BlueprintReadWrite)
  TSoftObjectPtr<UEFModularSettingsBase> Template;

  // Current value as string. The server keeps this in sync whenever a setting
  // changes. Late-joining clients apply this after creating settings from the
  // template, ensuring they always see the server's current state regardless
  // of subobject property replication timing.
  UPROPERTY()
  FString CurrentValue;
};

// For runtime-created settings (created via AddBoolSetting, etc.)
// Contains all the data needed to reconstruct settings on clients
USTRUCT(BlueprintType)
struct FEFRuntimeSettingDefinition {
  GENERATED_BODY()

  UPROPERTY()
  FGameplayTag Tag;

  UPROPERTY()
  FText DisplayName;

  UPROPERTY()
  FName ConfigCategory = NAME_None;

  UPROPERTY()
  uint8 SettingType = 0; // 0: Bool, 1: Float, 2: MultiSelect

  // Bool settings
  UPROPERTY()
  bool BoolDefaultValue = false;

  UPROPERTY()
  bool BoolValue = false;

  // Float settings
  UPROPERTY()
  float FloatDefaultValue = 0.f;

  UPROPERTY()
  float FloatValue = 0.f;

  UPROPERTY()
  float FloatMin = 0.f;

  UPROPERTY()
  float FloatMax = 1.f;

  // MultiSelect settings
  UPROPERTY()
  TArray<FString> MultiSelectValues;

  UPROPERTY()
  TArray<FText> MultiSelectDisplayNames;

  UPROPERTY()
  FString MultiSelectDefaultValue;

  UPROPERTY()
  int32 MultiSelectIndex = 0;
};

// Load lifecycle state machine for the player settings component.
// Prevents race conditions between BeginPlay, replication, and disk I/O.
UENUM()
enum class EPlayerSettingsLoadState : uint8
{
  WaitingForOwner,      // BeginPlay ran but can't identify local player yet
  WaitingForSettings,   // Local player identified, waiting for settings to exist
  LoadingFromDisk,      // Load from disk in progress
  SyncingWithServer,    // Sending loaded values to server via RPCs
  Ready                 // All settings loaded and synced
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class UNREALEXTENDEDFRAMEWORK_API UEFPlayerSettingsComponent
    : public UActorComponent {
  GENERATED_BODY()

public:
  UEFPlayerSettingsComponent();

  virtual void GetLifetimeReplicatedProps(
      TArray<FLifetimeProperty> &OutLifetimeProps) const override;
  virtual bool ReplicateSubobjects(class UActorChannel *Channel,
                                   class FOutBunch *Bunch,
                                   FReplicationFlags *RepFlags) override;

  UPROPERTY(BlueprintAssignable, Category = "Modular Settings")
  FOnPlayerSettingChanged OnSettingChanged;

  UFUNCTION(BlueprintCallable, Category = "Modular Settings")
  UEFModularSettingsBase *GetSettingByTag(FGameplayTag Tag) const;

  UFUNCTION(BlueprintCallable, Category = "Modular Settings")
  TArray<UEFModularSettingsBase *> GetSettingsByCategory(FName Category) const;

  // Creates and adds a new Bool setting at runtime.
  // Returns the created setting, or existing setting if Tag already exists.
  UFUNCTION(BlueprintCallable, Category = "Modular Settings")
  UEFModularSettingsBool *AddBoolSetting(FGameplayTag Tag, FText DisplayName,
                                         FName ConfigCategory,
                                         bool DefaultValue, bool InitialValue);

  // Creates and adds a new Float setting at runtime.
  // Returns the created setting, or existing setting if Tag already exists.
  UFUNCTION(BlueprintCallable, Category = "Modular Settings")
  UEFModularSettingsFloat *
  AddFloatSetting(FGameplayTag Tag, FText DisplayName, FName ConfigCategory,
                  float DefaultValue, float InitialValue, float Min, float Max);

  // Creates and adds a new MultiSelect setting at runtime.
  // Returns the created setting, or existing setting if Tag already exists.
  UFUNCTION(BlueprintCallable, Category = "Modular Settings")
  UEFModularSettingsMultiSelect *
  AddMultiSelectSetting(FGameplayTag Tag, FText DisplayName,
                        FName ConfigCategory, const TArray<FString> &Values,
                        const TArray<FText> &ValueDisplayNames,
                        int32 DefaultIndex, int32 InitialIndex);

protected:
  virtual void BeginPlay() override;

  UPROPERTY(EditDefaultsOnly, Instanced, Category = "Modular Settings")
  TArray<TObjectPtr<UEFModularSettingsBase>> DefaultSettings;

  // Instanced settings (created via DuplicateObject). Values replicate via
  // subobject replication.
  UPROPERTY(ReplicatedUsing = OnRep_Settings)
  TArray<TObjectPtr<UEFModularSettingsBase>> Settings;

  // Replicated "what exists" list for asset-based settings (from
  // containers/data assets).
  UPROPERTY(ReplicatedUsing = OnRep_SettingDefinitions)
  TArray<FEFPlayerSettingDefinition> SettingDefinitions;

  // Replicated definitions for runtime-created settings (AddBoolSetting, etc.)
  // Contains all data needed to reconstruct settings on clients.
  UPROPERTY(ReplicatedUsing = OnRep_RuntimeSettingDefinitions)
  TArray<FEFRuntimeSettingDefinition> RuntimeSettingDefinitions;

  // Centralized local creation (used by BeginPlay defaults and by OnRep for
  // runtime adds).
  UEFModularSettingsBase *
  AddSettingFromTemplate_Local(UEFModularSettingsBase *Template);

  // Create a setting from runtime definition data
  UEFModularSettingsBase *
  CreateSettingFromRuntimeDefinition(const FEFRuntimeSettingDefinition &Def);

public:
  // Client-to-Server request. bFromLoad suppresses the client-side save to
  // prevent circular save-during-load.
  UFUNCTION(BlueprintCallable, Category = "Modular Settings")
  void RequestUpdateSetting(FGameplayTag Tag, const FString &NewValue, bool bFromLoad = false);

  // Save all settings to disk (local player only). Uses coalescing timer to
  // batch rapid changes into a single write.
  UFUNCTION(BlueprintCallable, Category = "Modular Settings|Persistence")
  void SavePlayerSettings();

  // Load settings from disk into PendingLoadedValues (local player only).
  UFUNCTION(BlueprintCallable, Category = "Modular Settings|Persistence")
  void LoadPlayerSettings();

  // Try to apply pending loaded values to settings that now exist.
  // Called after settings are created via replication or template instantiation.
  void TryApplyPendingValues();

protected:
  UFUNCTION(Server, Reliable, WithValidation)
  void ServerUpdateSetting(FGameplayTag Tag, const FString &NewValue);

  // Get the save slot name for player settings
  FString GetPlayerSettingsSaveSlotName() const;

  // Check if this component belongs to the local player
  bool IsLocalPlayerComponent() const;

private:
  UFUNCTION()
  void OnRep_Settings();

  UFUNCTION()
  void OnRep_RuntimeSettingDefinitions();

  UFUNCTION()
  void OnRep_SettingDefinitions();

  // ---- Load state machine ----
  EPlayerSettingsLoadState LoadState = EPlayerSettingsLoadState::WaitingForOwner;

  // Values loaded from disk, waiting to be applied once their settings exist.
  // Entries are removed as they are successfully applied.
  TMap<FGameplayTag, FString> PendingLoadedValues;

  // Timer for retrying IsLocalPlayerComponent() when PlayerState isn't ready yet.
  FTimerHandle OwnerRetryTimer;
  int32 OwnerRetryCount = 0;
  static constexpr int32 MaxOwnerRetries = 50; // 50 * 0.1s = 5 seconds max
  void RetryIdentifyOwner();

  // ---- Save coalescing ----
  FTimerHandle SaveCoalesceTimer;
  bool bSavePending = false;
  void RequestSave();       // Starts/restarts a 0.5s coalescing timer
  void ExecutePendingSave(); // Actually writes to disk

  // ---- Migration ----
  // Attempts to migrate from old shared save slot if new slot doesn't exist
  bool TryMigrateFromOldSaveFormat();

  // Keeps SettingDefinitions[].CurrentValue in sync on the server.
  void UpdateDefinitionCurrentValue(FGameplayTag Tag, const FString& NewValue);

  static const FString OldSaveSlotName;     // "PlayerSettingsSave"
  static const FString NewSaveSlotName;     // "EFPlayerSettings"
  static const int32 SaveUserIndex;
};
