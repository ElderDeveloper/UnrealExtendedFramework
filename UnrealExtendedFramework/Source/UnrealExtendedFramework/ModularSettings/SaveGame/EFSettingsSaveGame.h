// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "GameplayTagContainer.h"
#include "EFSettingsSaveGame.generated.h"

/** One device family's device-scoped values (graphics, display) that differ from that family's defaults. */
USTRUCT()
struct UNREALEXTENDEDFRAMEWORK_API FEFDeviceScopedValues
{
  GENERATED_BODY()

  UPROPERTY(VisibleAnywhere, Category = "Settings")
  TMap<FGameplayTag, FString> Values;
};

/**
 * SaveGame object to store all modular settings.
 * We store values as strings mapped to GameplayTags for maximum flexibility.
 *
 * Version 1 (DevilofthePlague2026 and DOP before device rules): StoredSettings held every local
 * setting's value. Version 2: only values that differ from the default are stored, user-scoped ones in
 * StoredSettings and device-scoped ones per device family in DeviceScopedSettings, so a player who never
 * touched a setting follows the device default, including after a patch changes it.
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFSettingsSaveGame : public USaveGame {
  GENERATED_BODY()

public:
  static constexpr int32 SparseSaveVersion = 2;

  // Save format version for migration. The default stays 1 on purpose: tagged serialization leaves out a
  // value equal to the default, so a version 1 file has no SaveVersion at all, and a default of 2 would
  // read such a file as version 2. Writers set SparseSaveVersion explicitly.
  UPROPERTY(VisibleAnywhere, Category = "Settings")
  int32 SaveVersion = 1;

  // Map of Setting Tag -> Value as String (for local subsystem settings). From version 2, user-scoped
  // settings only, and only values that differ from the default.
  UPROPERTY(VisibleAnywhere, Category = "Settings")
  TMap<FGameplayTag, FString> StoredSettings;

  // Version 2: device family -> device-scoped values that differ from that family's defaults. Every
  // family read from disk is written back, so a Steam Deck saving does not erase a PC's graphics from a
  // cloud-synced file.
  UPROPERTY(VisibleAnywhere, Category = "Settings")
  TMap<FString, FEFDeviceScopedValues> DeviceScopedSettings;

  // Map of Setting Tag -> Value as String (for player component settings)
  UPROPERTY(VisibleAnywhere, Category = "Settings")
  TMap<FGameplayTag, FString> PlayerSettings;
};
