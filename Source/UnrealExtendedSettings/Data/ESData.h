// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ESData.generated.h"


UENUM()
enum EExtendedSettingsFloat
{
	Brightness,
	MouseSensitivityX,
	MouseSensitivityY
};


USTRUCT(BlueprintType)
struct FExtendedSettingsSaveStruct
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

	
	//Display
	
	UPROPERTY(BlueprintReadWrite,EditDefaultsOnly , Category="Display")
	float Brightness = 0;
	

	//Controls

	UPROPERTY(BlueprintReadWrite,EditDefaultsOnly , Category="Controls")
	bool MouseInvertX = 0;

	UPROPERTY(BlueprintReadWrite,EditDefaultsOnly , Category="Controls")
	bool MouseInvertY = 0;

	UPROPERTY(BlueprintReadWrite,EditDefaultsOnly , Category="Controls")
	bool Vibration = true;

	UPROPERTY(BlueprintReadWrite,EditDefaultsOnly , Category="Controls")
	float MouseSensitivityX = 50;

	UPROPERTY(BlueprintReadWrite,EditDefaultsOnly , Category="Controls")
	float MouseSensitivityY = 50;
	
	FExtendedSettingsSaveStruct()
	{
		
	}
	
};


UCLASS()
class UNREALEXTENDEDSETTINGS_API UESData : public UObject
{
	GENERATED_BODY()
};
