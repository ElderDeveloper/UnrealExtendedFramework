// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "../Data/ExtendedSettingsData.h"
#include "../ExtendedSettingsSubsystem.h"
#include "ExtendedSettingsLibrary.generated.h"

/**
 * Blueprint Function Library for easy access to Extended Settings Subsystem
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UExtendedSettingsLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	// Helper function to get the Extended Settings Subsystem
	static UExtendedSettingsSubsystem* GetExtendedSettingsSubsystem(const UObject* WorldContextObject);

	// Gameplay Settings
	UFUNCTION(BlueprintPure, Category = "Extended Settings|Gameplay", meta = (WorldContext = "WorldContextObject"))
	static FExtendedGameplaySettings GetGameplaySettings(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Extended Settings|Gameplay", meta = (WorldContext = "WorldContextObject"))
	static void SetGameplaySettings(const UObject* WorldContextObject, const FExtendedGameplaySettings& GameplaySettings);

	UFUNCTION(BlueprintCallable, Category = "Extended Settings|Gameplay", meta = (WorldContext = "WorldContextObject"))
	static void ApplyGameplaySettings(const UObject* WorldContextObject, const FExtendedGameplaySettings& GameplaySettings);

	// Audio Settings
	UFUNCTION(BlueprintPure, Category = "Extended Settings|Audio", meta = (WorldContext = "WorldContextObject"))
	static FExtendedAudioSettings GetAudioSettings(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Extended Settings|Audio", meta = (WorldContext = "WorldContextObject"))
	static void SetAudioSettings(const UObject* WorldContextObject, const FExtendedAudioSettings& AudioSettings);

	UFUNCTION(BlueprintCallable, Category = "Extended Settings|Audio", meta = (WorldContext = "WorldContextObject"))
	static void ApplyAudioSettings(const UObject* WorldContextObject, const FExtendedAudioSettings& AudioSettings);

	// Graphics Settings
	UFUNCTION(BlueprintPure, Category = "Extended Settings|Graphics", meta = (WorldContext = "WorldContextObject"))
	static FExtendedGraphicsSettings GetGraphicsSettings(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Extended Settings|Graphics", meta = (WorldContext = "WorldContextObject"))
	static void SetGraphicsSettings(const UObject* WorldContextObject, const FExtendedGraphicsSettings& GraphicsSettings);

	UFUNCTION(BlueprintCallable, Category = "Extended Settings|Graphics", meta = (WorldContext = "WorldContextObject"))
	static void ApplyGraphicsSettings(const UObject* WorldContextObject, const FExtendedGraphicsSettings& GraphicsSettings);

	// Display Settings
	UFUNCTION(BlueprintPure, Category = "Extended Settings|Display", meta = (WorldContext = "WorldContextObject"))
	static FExtendedDisplaySettings GetDisplaySettings(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Extended Settings|Display", meta = (WorldContext = "WorldContextObject"))
	static void SetDisplaySettings(const UObject* WorldContextObject, const FExtendedDisplaySettings& DisplaySettings);

	UFUNCTION(BlueprintCallable, Category = "Extended Settings|Display", meta = (WorldContext = "WorldContextObject"))
	static void ApplyDisplaySettings(const UObject* WorldContextObject, const FExtendedDisplaySettings& DisplaySettings);

	// Helper function to get the overall quality setting
	UFUNCTION(BlueprintPure, Category = "Extended Settings|Graphics")
	static FExtendedGraphicsSettings GetOverallGraphicSettings(int32 OverallQuality);

	// General Settings Operations
	UFUNCTION(BlueprintCallable, Category = "Extended Settings", meta = (WorldContext = "WorldContextObject"))
	static void ApplyAllSettings(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Extended Settings", meta = (WorldContext = "WorldContextObject"))
	static void RevertAllSettings(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Extended Settings", meta = (WorldContext = "WorldContextObject"))
	static void SaveAllSettings(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Extended Settings", meta = (WorldContext = "WorldContextObject"))
	static void FindAndApplyBestSettings(const UObject* WorldContextObject);

	// Audio Device Management
	UFUNCTION(BlueprintCallable, Category = "Extended Settings|Audio", meta = (WorldContext = "WorldContextObject"))
	static void UpdateAudioDeviceLists(const UObject* WorldContextObject);

	// Display Management
	UFUNCTION(BlueprintCallable, Category = "Extended Settings|Display", meta = (WorldContext = "WorldContextObject"))
	static void UpdateScreenResolutionList(const UObject* WorldContextObject);

	 // Get Difficulty Settings
	UFUNCTION(BlueprintPure, Category = "Extended Settings|Difficulty")
	static FExtendedDifficultySettings GetDifficultySettings(const uint8& DifficultyLevel);

	// Get available difficulty levels
	UFUNCTION(BlueprintPure, Category = "Extended Settings|Difficulty")
	static TArray<FExtendedDifficultySettings> GetAvailableDifficultyLevels();
};
