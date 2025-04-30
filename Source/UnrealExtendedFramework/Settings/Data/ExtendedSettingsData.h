 // Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"
#include "ExtendedSettingsData.generated.h"


USTRUCT(Blueprintable, BlueprintType)
struct FExtendedGameplaySettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere,Config, BlueprintReadWrite, Category = "Settings")
	float FieldOfView = 90.0f;

	UPROPERTY(EditAnywhere,Config, BlueprintReadWrite, Category = "Settings")
	bool bHeadBob = true;

	UPROPERTY(EditAnywhere,Config, BlueprintReadWrite, Category = "Settings")
	bool bCrosshair = true;

	UPROPERTY(EditAnywhere,Config, BlueprintReadWrite, Category = "Settings")
	bool bFPSCounter = false;

	UPROPERTY(EditAnywhere,Config, BlueprintReadWrite, Category = "Settings")
	bool bNameTemplate = true;

	UPROPERTY(EditAnywhere,Config, BlueprintReadWrite, Category = "Settings")
	bool bDepthOfField = true;

	
};


USTRUCT(Blueprintable, BlueprintType)
struct FExtendedAudioSettings
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere,Config, BlueprintReadOnly, Category = "Settings")
	TArray<FName> AudioOutputDevices;

	UPROPERTY(EditAnywhere,Config, BlueprintReadWrite, Category = "Settings")
	FName AudioOutputDeviceName;

	UPROPERTY(VisibleAnywhere,Config, BlueprintReadOnly, Category = "Settings")
	TArray<FName> AudioInputDevices;

	UPROPERTY(EditAnywhere,Config, BlueprintReadWrite, Category = "Settings")
	FName AudioInputDeviceName;

	UPROPERTY(EditAnywhere,Config, BlueprintReadWrite, Category = "Settings")
	float MasterVolume = 100.0f;

	UPROPERTY(EditAnywhere,Config, BlueprintReadWrite, Category = "Settings")
	float MusicVolume = 80.0f;

	UPROPERTY(EditAnywhere,Config, BlueprintReadWrite, Category = "Settings")
	float EffectsVolume = 100.0f;

	UPROPERTY(EditAnywhere,Config, BlueprintReadWrite, Category = "Settings")
	float VoiceChatVolume = 100.0f;

	UPROPERTY(EditAnywhere,Config, BlueprintReadWrite, Category = "Settings")
	bool bVoiceChatEnabled = true;

	UPROPERTY(EditAnywhere,Config, BlueprintReadWrite, Category = "Settings")
	bool bMuteJumpScare = false;

};


USTRUCT(Blueprintable, BlueprintType)
struct FExtendedGraphicsSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere,Config, BlueprintReadWrite, Category = "Settings")
	int32 OverallQuality = 3;

	UPROPERTY(EditAnywhere,Config, BlueprintReadWrite, Category = "Settings")
	int32 ViewDistance = 3;
	
	UPROPERTY(EditAnywhere,Config, BlueprintReadWrite, Category = "Settings")
	int32 PostProcessing = 3;

	UPROPERTY(EditAnywhere,Config, BlueprintReadWrite, Category = "Settings")
	int32 EffectsQuality = 3;

	UPROPERTY(EditAnywhere,Config, BlueprintReadWrite, Category = "Settings")
	int32 ShadowQuality = 3;

	UPROPERTY(EditAnywhere,Config, BlueprintReadWrite, Category = "Settings")
	int32 FoliageQuality = 3;

	UPROPERTY(EditAnywhere,Config, BlueprintReadWrite, Category = "Settings")
	int32 TextureQuality = 3;

	UPROPERTY(EditAnywhere,Config, BlueprintReadWrite, Category = "Settings")
	int32 ShaderQuality = 3;

	UPROPERTY(EditAnywhere,Config, BlueprintReadWrite, Category = "Settings")
	int32 ResolutionScale = 100;
	
	// Default constructor
	FExtendedGraphicsSettings()
	{
		// Default values are already set in property declarations
	}

	// Constructor with parameters
	FExtendedGraphicsSettings(
		int32 InOverallQuality,
		int32 InViewDistance,
		int32 InPostProcessing,
		int32 InEffectsQuality,
		int32 InShadowQuality,
		int32 InFoliageQuality,
		int32 InTextureQuality,
		int32 InShaderQuality,
		int32 InResolutionScale)
	{
		OverallQuality = InOverallQuality;
		ViewDistance = InViewDistance;
		PostProcessing = InPostProcessing;
		EffectsQuality = InEffectsQuality;
		ShadowQuality = InShadowQuality;
		FoliageQuality = InFoliageQuality;
		TextureQuality = InTextureQuality;
		ShaderQuality = InShaderQuality;
		ResolutionScale = InResolutionScale;
	}

	FExtendedGraphicsSettings GetOverallGraphicSettings(int32 Quality)
	{
		FExtendedGraphicsSettings Result;
		
		// Clamp quality between 0 (Low) and 4 (Epic)
		Quality = FMath::Clamp(Quality, 0, 4);
		
		// Set all quality settings based on the overall quality level
		Result.OverallQuality = Quality;
		Result.ViewDistance = Quality;
		Result.PostProcessing = Quality;
		Result.EffectsQuality = Quality;
		Result.ShadowQuality = Quality;
		Result.FoliageQuality = Quality;
		Result.TextureQuality = Quality;
		Result.ShaderQuality = Quality;
		
		// Resolution scale adjustments based on quality
		switch (Quality)
		{
		case 0: // Low
			Result.ResolutionScale = 75;
			break;
		case 1: // Medium
			Result.ResolutionScale = 85;
			break;
		case 2: // High
			Result.ResolutionScale = 90;
			break;
		case 3: // Ultra
			Result.ResolutionScale = 100;
			break;
		case 4: // Epic
			Result.ResolutionScale = 100;
			break;
		}
		
		return Result;
	}
};


USTRUCT(Blueprintable, BlueprintType)
struct FExtendedDisplaySettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere,Config, BlueprintReadWrite, Category = "Settings")
	TArray<FName> ScreenResolutions;

	UPROPERTY(EditAnywhere,Config, BlueprintReadWrite, Category = "Settings")
	FName ScreenResolution;

	UPROPERTY(EditAnywhere,Config, BlueprintReadWrite, Category = "Settings")
	TArray<FName> DisplayModes = { "Windowed", "Fullscreen", "Windowed Fullscreen" };

	UPROPERTY(EditAnywhere,Config, BlueprintReadWrite, Category = "Settings")
	FName DisplayMode = "Fullscreen";

	UPROPERTY(EditAnywhere,Config, BlueprintReadWrite, Category = "Settings")
	bool VerticalSync = true;

	UPROPERTY(EditAnywhere,Config, BlueprintReadWrite, Category = "Settings")
	TArray<FName> FrameRateLimits = { "30", "60", "90", "120", "144", "240","Unlimited"};

	UPROPERTY(EditAnywhere,Config, BlueprintReadWrite, Category = "Settings")
	FName FrameRateLimit = "60";

	UPROPERTY(EditAnywhere,Config, BlueprintReadWrite, Category = "Settings")
	TArray<FName> AntiAliasingModes = { "None", "FXAA", "TAA", "MSAA" };

	UPROPERTY(EditAnywhere,Config, BlueprintReadWrite, Category = "Settings")
	FName AntiAliasingMode = "TAA";
};


USTRUCT(Blueprintable, BlueprintType)
struct FExtendedDifficultySettings : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FName DifficultyName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FText DifficultyDisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FText DifficultyDescription;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	TMap<FGameplayTag, float> DifficultyCustomSettings;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	float EnemyDamageMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	float PlayerHealthMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	float ResourceSpawnRate = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	bool bEnablePermadeath = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	int32 MaxRespawnLimit = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	float EnemyDetectionRadiusMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	float PuzzleComplexityMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	float EnemySpeedMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	float EnemySpawnRateMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	float StaminaDrainMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	float StaminaRegenMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	float LanternFuelConsumptionRate = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	float LanternFuelRechargeRate = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	float HealingItemEffectiveness = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	bool bEnableJumpScares = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	bool bShowEnemyIndicators = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	TArray<FName> DisabledEnemyTypes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	float PlayerNoiseMultiplier = 1.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	bool bEnableSanityEffects = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	float PlayerSanityDrainRate = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	float EnemyAggression = 1.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	bool bShowObjectiveMarkers = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cooperative|Gameplay")
	float DifficultyScalingPerPlayer = 0.25f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cooperative|Gameplay")
	float TeamReviveTimeScale = 1.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cooperative|Resources")
	float ResourceScalingPerPlayer = 0.75f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cooperative|Communication")
	float ProximityVoiceChatDistance = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cooperative|Gameplay")
	float TeamSeparationPenaltyMultiplier = 1.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cooperative|Gameplay")
	float SharedSanityEnabled = true;

};

