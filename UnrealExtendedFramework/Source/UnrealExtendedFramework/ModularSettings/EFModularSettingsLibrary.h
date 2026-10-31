// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameplayTagContainer.h"
#include "UnrealExtendedFramework/ModularSettings/Device/EFModularSettingsDeviceRules.h"
#include "EFModularSettingsLibrary.generated.h"

class UEFModularSettingsBase;

UENUM(BlueprintType)
enum class EEFSettingsSource : uint8
{
	Local,
	World,
	Player,
	Auto // Try to resolve automatically in order: Player -> World -> Local
};

UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFModularSettingsLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintPure, Category = "Modular Settings", meta = (WorldContext = "WorldContextObject"))
	static UEFModularSettingsBase* GetModularSetting(const UObject* WorldContextObject, FGameplayTag Tag, EEFSettingsSource Source = EEFSettingsSource::Auto, APlayerState* SpecificPlayer = nullptr);

	// True if a setting with this tag exists in the requested scope. Auto checks Player, World, then Local.
	UFUNCTION(BlueprintPure, Category = "Modular Settings", meta = (WorldContext = "WorldContextObject"))
	static bool HasModularSetting(const UObject* WorldContextObject, FGameplayTag Tag, EEFSettingsSource Source = EEFSettingsSource::Auto, APlayerState* SpecificPlayer = nullptr);
	
	UFUNCTION(BlueprintPure, Category = "Modular Settings", meta = (WorldContext = "WorldContextObject"))
	static bool GetModularBool(const UObject* WorldContextObject, FGameplayTag Tag, EEFSettingsSource Source = EEFSettingsSource::Auto, APlayerState* SpecificPlayer = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Modular Settings", meta = (WorldContext = "WorldContextObject"))
	static void SetModularBool(const UObject* WorldContextObject, FGameplayTag Tag, bool bValue, EEFSettingsSource Source = EEFSettingsSource::Auto, APlayerState* SpecificPlayer = nullptr, bool bApplyImmediately = true);

	UFUNCTION(BlueprintPure, Category = "Modular Settings", meta = (WorldContext = "WorldContextObject"))
	static float GetModularFloat(const UObject* WorldContextObject, FGameplayTag Tag, EEFSettingsSource Source = EEFSettingsSource::Auto, APlayerState* SpecificPlayer = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Modular Settings", meta = (WorldContext = "WorldContextObject"))
	static void SetModularFloat(const UObject* WorldContextObject, FGameplayTag Tag, float Value, EEFSettingsSource Source = EEFSettingsSource::Auto, APlayerState* SpecificPlayer = nullptr, bool bApplyImmediately = true);

	UFUNCTION(BlueprintPure, Category = "Modular Settings", meta = (WorldContext = "WorldContextObject"))
	static int32 GetModularSelectedIndex(const UObject* WorldContextObject, FGameplayTag Tag, EEFSettingsSource Source = EEFSettingsSource::Auto, APlayerState* SpecificPlayer = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Modular Settings", meta = (WorldContext = "WorldContextObject"))
	static bool SetModularSelectedIndex(const UObject* WorldContextObject, FGameplayTag Tag, int32 Index, EEFSettingsSource Source = EEFSettingsSource::Auto, APlayerState* SpecificPlayer = nullptr, bool bApplyImmediately = true);

	UFUNCTION(BlueprintPure, Category = "Modular Settings", meta = (WorldContext = "WorldContextObject"))
	static FString GetModularSelectedOption(const UObject* WorldContextObject, FGameplayTag Tag, EEFSettingsSource Source = EEFSettingsSource::Auto, APlayerState* SpecificPlayer = nullptr);
	
	UFUNCTION(BlueprintCallable, Category = "Modular Settings", meta = (WorldContext = "WorldContextObject"))
	static void AdjustModularIndex(const UObject* WorldContextObject, FGameplayTag Tag, int32 Amount, bool bWrap = true, EEFSettingsSource Source = EEFSettingsSource::Auto, APlayerState* SpecificPlayer = nullptr, bool bApplyImmediately = true);

	UFUNCTION(BlueprintPure, Category = "Modular Settings", meta = (WorldContext = "WorldContextObject"))
	static TArray<FText> GetModularOptions(const UObject* WorldContextObject, FGameplayTag Tag, EEFSettingsSource Source = EEFSettingsSource::Auto, APlayerState* SpecificPlayer = nullptr);

	/**
	 * The options this device offers, in order, with each one's index in the full option list. A settings
	 * row shows these; hidden options keep their index, so map a row position back through OutIndices.
	 */
	UFUNCTION(BlueprintPure, Category = "Modular Settings|Device", meta = (WorldContext = "WorldContextObject"))
	static TArray<FText> GetModularOfferedOptions(const UObject* WorldContextObject, FGameplayTag Tag, TArray<int32>& OutIndices, EEFSettingsSource Source = EEFSettingsSource::Auto, APlayerState* SpecificPlayer = nullptr);

	/** False when the setting does not exist or this device hides it. */
	UFUNCTION(BlueprintPure, Category = "Modular Settings|Device", meta = (WorldContext = "WorldContextObject"))
	static bool IsModularSettingVisible(const UObject* WorldContextObject, FGameplayTag Tag, EEFSettingsSource Source = EEFSettingsSource::Auto, APlayerState* SpecificPlayer = nullptr);

	/** False when the setting does not exist or this device locks or hides it. */
	UFUNCTION(BlueprintPure, Category = "Modular Settings|Device", meta = (WorldContext = "WorldContextObject"))
	static bool IsModularSettingEditable(const UObject* WorldContextObject, FGameplayTag Tag, EEFSettingsSource Source = EEFSettingsSource::Auto, APlayerState* SpecificPlayer = nullptr);

	/** Hidden when the setting does not exist. */
	UFUNCTION(BlueprintPure, Category = "Modular Settings|Device", meta = (WorldContext = "WorldContextObject"))
	static EEFSettingAvailability GetModularSettingAvailability(const UObject* WorldContextObject, FGameplayTag Tag, EEFSettingsSource Source = EEFSettingsSource::Auto, APlayerState* SpecificPlayer = nullptr);

	UFUNCTION(BlueprintPure, Category = "Modular Settings", meta = (WorldContext = "WorldContextObject"))
	static bool IsModularOptionLocked(const UObject* WorldContextObject, FGameplayTag Tag, int32 Index, EEFSettingsSource Source = EEFSettingsSource::Auto, APlayerState* SpecificPlayer = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Modular Settings", meta = (WorldContext = "WorldContextObject"))
	static void SetModularOptionLocked(const UObject* WorldContextObject, FGameplayTag Tag, int32 Index, bool bLocked, EEFSettingsSource Source = EEFSettingsSource::Auto, APlayerState* SpecificPlayer = nullptr);

	// Unified Apply
	UFUNCTION(BlueprintCallable, Category = "Modular Settings", meta = (WorldContext = "WorldContextObject"))
	static void ApplyModularSetting(const UObject* WorldContextObject, FGameplayTag Tag, EEFSettingsSource Source = EEFSettingsSource::Auto, APlayerState* SpecificPlayer = nullptr);
};
