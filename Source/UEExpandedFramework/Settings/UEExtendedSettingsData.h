// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UEExtendedSettingsData.generated.h"

USTRUCT(BlueprintType)
struct FSettingsSaveStruct
{
	GENERATED_BODY()

//Gameplay
	
	UPROPERTY(BlueprintReadWrite,EditDefaultsOnly , Category="Gameplay")
	uint8 Difficulty = 0;

	UPROPERTY(BlueprintReadWrite,EditDefaultsOnly , Category="Gameplay")
	uint8 Language = 0;

	UPROPERTY(BlueprintReadWrite,EditDefaultsOnly , Category="Gameplay")
	uint8 SubtitleLanguage = 0;

	UPROPERTY(BlueprintReadWrite,EditDefaultsOnly , Category="Gameplay")
	bool Subtitles = 0;

	UPROPERTY(BlueprintReadWrite,EditDefaultsOnly , Category="Gameplay")
	bool ColorBlindMode = 0;

//Graphics

	UPROPERTY(BlueprintReadWrite,EditDefaultsOnly , Category="Graphics")
	uint8 QualityPreset = 0;

	UPROPERTY(BlueprintReadWrite,EditDefaultsOnly , Category="Graphics")
	uint8 Textures = 0;

	UPROPERTY(BlueprintReadWrite,EditDefaultsOnly , Category="Graphics")
	uint8 Shadows = 0;

	UPROPERTY(BlueprintReadWrite,EditDefaultsOnly , Category="Graphics")
	uint8 Foliage = 0;

	UPROPERTY(BlueprintReadWrite,EditDefaultsOnly , Category="Graphics")
	uint8 AAMethod = 0;

	UPROPERTY(BlueprintReadWrite,EditDefaultsOnly , Category="Graphics")
	uint8 AAQuality = 0;

	UPROPERTY(BlueprintReadWrite,EditDefaultsOnly , Category="Graphics")
	uint8 ViewDistance = 0;

	UPROPERTY(BlueprintReadWrite,EditDefaultsOnly , Category="Graphics")
	uint8 ViewEffects = 0;

	UPROPERTY(BlueprintReadWrite,EditDefaultsOnly , Category="Graphics")
	bool MotionBlur = 0;

	UPROPERTY(BlueprintReadWrite,EditDefaultsOnly , Category="Graphics")
	bool LensFlares = 0;

	UPROPERTY(BlueprintReadWrite,EditDefaultsOnly , Category="Graphics")
	bool SSReflections = 0;

	UPROPERTY(BlueprintReadWrite,EditDefaultsOnly , Category="Graphics")
	bool Bloom = 0;

	UPROPERTY(BlueprintReadWrite,EditDefaultsOnly , Category="Graphics")
	float ResolutionScale = 0;
	
//Display

	UPROPERTY(BlueprintReadWrite,EditDefaultsOnly , Category="Display")
	uint8 Resolution = 0;

	UPROPERTY(BlueprintReadWrite,EditDefaultsOnly , Category="Display")
	uint8 DisplayMode = 0;

	UPROPERTY(BlueprintReadWrite,EditDefaultsOnly , Category="Display")
	float Gamma = 0;

	UPROPERTY(BlueprintReadWrite,EditDefaultsOnly , Category="Display")
	bool VSynch = 0;

//Audio

	UPROPERTY(BlueprintReadWrite,EditDefaultsOnly , Category="Audio")
	float Master = 100;

	UPROPERTY(BlueprintReadWrite,EditDefaultsOnly , Category="Audio")
	float Music = 100;

	UPROPERTY(BlueprintReadWrite,EditDefaultsOnly , Category="Audio")
	float SFX = 100;

	UPROPERTY(BlueprintReadWrite,EditDefaultsOnly , Category="Audio")
	float Voices = 100;

	UPROPERTY(BlueprintReadWrite,EditDefaultsOnly , Category="Audio")
	float UI = 100;

//Controls

	UPROPERTY(BlueprintReadWrite,EditDefaultsOnly , Category="Controls")
	bool InvertX = 0;

	UPROPERTY(BlueprintReadWrite,EditDefaultsOnly , Category="Controls")
	bool InvertY = 0;

	UPROPERTY(BlueprintReadWrite,EditDefaultsOnly , Category="Controls")
	float Vibration = 50;

	UPROPERTY(BlueprintReadWrite,EditDefaultsOnly , Category="Controls")
	float MouseSensitivity = 50;

	FSettingsSaveStruct()
	{
		
	}
	
};


UCLASS()
class UEEXPANDEDFRAMEWORK_API UUEExtendedSettingsData : public UObject
{
	GENERATED_BODY()
};
