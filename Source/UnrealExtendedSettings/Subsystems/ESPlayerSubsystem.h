// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UnrealExtendedSettings/Data/ESData.h"
#include "UObject/Object.h"
#include "ESPlayerSubsystem.generated.h"


class USaveGame;
#define ReturnIfGameUserSettings  if(!GameUserSettings) return 

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FFloatSettingsUpdate , TEnumAsByte<EExtendedSettingsFloat> , SettingsType , float , Value);



UCLASS()
class UNREALEXTENDEDSETTINGS_API UESPlayerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

		
public:

	UPROPERTY(BlueprintAssignable)
	FFloatSettingsUpdate FloatSettingsUpdate;

	FString UESettingsSaveName = "UESettings";

	UFUNCTION(BlueprintCallable)
	void UpdateSettingsFloatValue(TEnumAsByte<EExtendedSettingsFloat> SettingsType , float Value);

	void SaveSettings();
	

	UFUNCTION()
	void OnSaveGameLoadComplete(const FString& SaveGameSlot, const int32 PlayerIndex, USaveGame* SettingsSaveObject);

	
protected:
	

	UPROPERTY()
	FExtendedSettingsSaveStruct SettingsStruct;

public:
	//Gameplay
	UFUNCTION(BlueprintPure , Category="Get Settings|Graphics")	FORCEINLINE	int32 GetDifficulty() const {	return SettingsStruct.Difficulty;	}

	UFUNCTION(BlueprintPure , Category="Get Settings|Graphics")	FORCEINLINE int32 GetLanguage() const {	return SettingsStruct.Language;	}

	UFUNCTION(BlueprintPure , Category="Get Settings|Graphics")	FORCEINLINE int32 GetSubtitleLanguage() const {	return SettingsStruct.SubtitleLanguage;	}

	UFUNCTION(BlueprintPure , Category="Get Settings|Graphics")	FORCEINLINE	bool GetSubtitles() const {	return SettingsStruct.Subtitles;	}
	
	UFUNCTION(BlueprintPure , Category="Get Settings|Graphics")	FORCEINLINE	bool GetColorBlindMode() const {	return SettingsStruct.ColorBlindMode;	}

	


	
	//Controls
	UFUNCTION(BlueprintPure , Category="Get Settings|Controls")	FORCEINLINE	bool GetMouseInvertX() const {	return SettingsStruct.MouseInvertX;	}

	UFUNCTION(BlueprintPure , Category="Get Settings|Controls")	FORCEINLINE	bool GetMouseInvertY() const {	return SettingsStruct.MouseInvertY;	}
	
	UFUNCTION(BlueprintPure , Category="Get Settings|Controls")	FORCEINLINE	float GetMouseSensitivityX() const {	return SettingsStruct.MouseSensitivityX;	}

	UFUNCTION(BlueprintPure , Category="Get Settings|Controls")	FORCEINLINE	float GetMouseSensitivityY() const {	return SettingsStruct.MouseSensitivityY;	}

	UFUNCTION(BlueprintPure , Category="Get Settings|Controls")	FORCEINLINE	float GetUseVibration() const {	return SettingsStruct.Vibration;	}


	//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< SET SETTINGS >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>


		//Gameplay
	UFUNCTION(BlueprintCallable , Category="Set Settings|Gameplay")		void SetDifficulty(int32 Value) { SettingsStruct.Difficulty = Value;	}

	UFUNCTION(BlueprintCallable , Category="Set Settings|Gameplay")		void SetLanguage(int32 Value)  {	 SettingsStruct.Language = Value;	}

	UFUNCTION(BlueprintCallable , Category="Set Settings|Gameplay")		void SetSubtitleLanguage(int32 Value)  {	 SettingsStruct.SubtitleLanguage = Value;	}

	UFUNCTION(BlueprintCallable , Category="Set Settings|Gameplay")		void SetUseSubtitles(bool Value)  {	 SettingsStruct.Subtitles = Value;	}
	
	UFUNCTION(BlueprintCallable , Category="Set Settings|Gameplay")		void SetUseColorBlindMode(bool Value)  {	 SettingsStruct.ColorBlindMode = Value;	}


	

	//Controls
	UFUNCTION(BlueprintCallable , Category="Set Settings|Controls")		void SetMouseInvertX( bool Value)  {	 SettingsStruct.MouseInvertX = Value;	}

	UFUNCTION(BlueprintCallable , Category="Set Settings|Controls")		void SetMouseInvertY(bool Value)  {	 SettingsStruct.MouseInvertY = Value;	}

	UFUNCTION(BlueprintCallable , Category="Set Settings|Controls")		void SetUseVibration(bool Vibration)  {	 SettingsStruct.Vibration = Vibration;	}

	UFUNCTION(BlueprintCallable , Category="Set Settings|Controls")		void SetMouseSensitivityX(float Value)  {	 SettingsStruct.MouseSensitivityX = Value;}

	UFUNCTION(BlueprintCallable , Category="Set Settings|Controls")		void SetMouseSensitivityY(float Value)  {	 SettingsStruct.MouseSensitivityY = Value;}
};
