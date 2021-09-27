// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UEExtendedSettingsLibrary.generated.h"

/**
 * 
 */

class UUEExtendedSettingsSubsystem;

UCLASS()
class UEEXPANDEDFRAMEWORK_API UUEExtendedSettingsLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:


	//Subsystem
	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject"))
	static UUEExtendedSettingsSubsystem* GetSettingsSubsystem(const UObject* WorldContextObject);
	
	
	
	//Gameplay
	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject"))
	static int32 GetDifficulty(const UObject* WorldContextObject);

	
	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject"))
	static int32 GetLanguage(const UObject* WorldContextObject);

	
	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject"))
	static int32 GetSubtitleLanguage(const UObject* WorldContextObject);

	
	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject"))
	static bool GetSubtitles(const UObject* WorldContextObject);

	
	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject"))
	static bool GetColorBlindMode(const UObject* WorldContextObject);




	

	//Graphics
	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject"))
	static int32 GetQualityPreset(const UObject* WorldContextObject);

	
	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject"))
	static int32 GetTextures(const UObject* WorldContextObject);

	
	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject"))
	static int32 GetShadows(const UObject* WorldContextObject);

	
	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject"))
	static int32 GetFoliage (const UObject* WorldContextObject);

	
	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject"))
	static int32 GetAAMethod(const UObject* WorldContextObject);

	
	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject"))
	static int32 GetAAQuality(const UObject* WorldContextObject);

	
	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject"))
	static int32 GetViewDistance(const UObject* WorldContextObject) ;

	
	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject"))
	static int32 GetViewEffects(const UObject* WorldContextObject);

	
	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject"))
	static bool GetMotionBlur(const UObject* WorldContextObject);

	
	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject"))
	static bool GetLensFlares(const UObject* WorldContextObject);

	
	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject"))
	static bool GetSSReflections(const UObject* WorldContextObject);

	
	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject"))
	static bool GetBloom(const UObject* WorldContextObject);

	
	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject"))
	static float GetResolutionScale(const UObject* WorldContextObject);




	

	//Display
	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject"))
	static int32 GetResolution(const UObject* WorldContextObject);


	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject"))
	static int32 GetDisplayMode(const UObject* WorldContextObject);


	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject"))
	static float GetGamma(const UObject* WorldContextObject);


	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject"))
	static bool GetVSynch(const UObject* WorldContextObject);



	

	
	//Audio
	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject"))
	static float GetMaster(const UObject* WorldContextObject);


	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject"))
	static float GetMusic(const UObject* WorldContextObject);


	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject"))
	static float GetSFX(const UObject* WorldContextObject);


	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject"))
	static float GetVoices(const UObject* WorldContextObject);


	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject"))
	static float GetUI(const UObject* WorldContextObject);




	

	//Controls
	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject"))
	static bool GetInvertX(const UObject* WorldContextObject);


	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject"))
	static bool GetInvertY(const UObject* WorldContextObject);


	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject"))
	static float GetVibration(const UObject* WorldContextObject);


	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject"))
	static float GetMouseSensitivity(const UObject* WorldContextObject);




	//<<<<<<<<<<<<<<<<<<<<<<<<< SET SETTINGS >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	//<<<<<<<<<<<<<<<<<<<<<<<<< SET SETTINGS >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>



		//Gameplay
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static void SetDifficulty(const UObject* WorldContextObject , int32 Difficulty);

	/*
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static void SetLanguage(const UObject* WorldContextObject , int32 Language);

	
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static void SetSubtitleLanguage(const UObject* WorldContextObject , int32 SubtitleLanguage);

	
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static void SetSubtitles(const UObject* WorldContextObject , bool Subtitles);

	
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static void SetColorBlindMode(const UObject* WorldContextObject, bool ColorBlindMode);

	

	//Graphics
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static void SetQualityPreset(const UObject* WorldContextObject, int32 QualityPreset);

	
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static void SetTextures(const UObject* WorldContextObject, int32 Textures);

	
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static void SetShadows(const UObject* WorldContextObject, int32 Shadows);

	
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static void SetFoliage (const UObject* WorldContextObject, int32 Foliage);

	
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static void SetAAMethod(const UObject* WorldContextObject, int32 AAMethod);

	
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static void SetAAQuality(const UObject* WorldContextObject, int32 AAQuality);

	
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static void SetViewDistance(const UObject* WorldContextObject, int32 ViewDistance) ;

	
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static void SetViewEffects(const UObject* WorldContextObject, int32 ViewEffects);

	
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static void SetMotionBlur(const UObject* WorldContextObject, bool MotionBlur);

	
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static void SetLensFlares(const UObject* WorldContextObject, bool LensFlares);

	
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static void SetSSReflections(const UObject* WorldContextObject, bool SSReflection);

	
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static void SetBloom(const UObject* WorldContextObject, bool Bloom);

	
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static void SetResolutionScale(const UObject* WorldContextObject, float ResolutionScale);



	//Display
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static void SetResolution(const UObject* WorldContextObject, int32 Resolution);


	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static void SetDisplayMode(const UObject* WorldContextObject, int32 DisplayMode);


	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static void SetGamma(const UObject* WorldContextObject, float Gamma);


	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static void SetVSynch(const UObject* WorldContextObject, bool VSynch);

	
	//Audio
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static void SetMaster(const UObject* WorldContextObject, float Master);


	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static void SetMusic(const UObject* WorldContextObject, float Music);


	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static void SetSFX(const UObject* WorldContextObject, float SFX);


	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static void SetVoices(const UObject* WorldContextObject, float Voices);


	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static void SetUI(const UObject* WorldContextObject, float UI);

	

	//Controls
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static void SetInvertX(const UObject* WorldContextObject, bool InvertX);


	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static void SetInvertY(const UObject* WorldContextObject, bool InvertY);


	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static void SetVibration(const UObject* WorldContextObject, float Vibration);


	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static void SetMouseSensitivity(const UObject* WorldContextObject, float MouseSensitivity);
	*/
	
};
