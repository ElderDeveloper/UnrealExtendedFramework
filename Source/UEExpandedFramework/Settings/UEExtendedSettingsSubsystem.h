// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UEExtendedSettingsData.h"
#include "Subsystems/EngineSubsystem.h"
#include "UEExtendedSettingsSubsystem.generated.h"

/**
 * 
 */
UCLASS(config = GameUserSettings, configdonotcheckdefaults)
class UEEXPANDEDFRAMEWORK_API UUEExtendedSettingsSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()


public:

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override {}

protected:

	UPROPERTY()
	FSettingsSaveStruct SettingsStruct;


public:

	//Gameplay
	UFUNCTION(BlueprintPure , Category="Get Settings|Graphics")	FORCEINLINE	int32 GetDifficulty() const {	return SettingsStruct.Difficulty;	}

	UFUNCTION(BlueprintPure , Category="Get Settings|Graphics")	FORCEINLINE int32 GetLanguage() const {	return SettingsStruct.Language;	}

	UFUNCTION(BlueprintPure , Category="Get Settings|Graphics")	FORCEINLINE int32 GetSubtitleLanguage() const {	return SettingsStruct.SubtitleLanguage;	}

	UFUNCTION(BlueprintPure , Category="Get Settings|Graphics")	FORCEINLINE	bool GetSubtitles() const {	return SettingsStruct.Subtitles;	}
	
	UFUNCTION(BlueprintPure , Category="Get Settings|Graphics")	FORCEINLINE	bool GetColorBlindMode() const {	return SettingsStruct.ColorBlindMode;	}

	//Graphics
	UFUNCTION(BlueprintPure , Category="Get Settings|Graphics")	FORCEINLINE	int32 GetQualityPreset() const {	return SettingsStruct.QualityPreset;	}

	UFUNCTION(BlueprintPure , Category="Get Settings|Graphics")	FORCEINLINE	int32 GetTextures() const {	return SettingsStruct.Textures;	}

	UFUNCTION(BlueprintPure , Category="Get Settings|Graphics")	FORCEINLINE	int32 GetShadows() const {	return SettingsStruct.Shadows;	}

	UFUNCTION(BlueprintPure , Category="Get Settings|Graphics")	FORCEINLINE	int32 GetFoliage() const {	return SettingsStruct.Foliage;	}

	UFUNCTION(BlueprintPure , Category="Get Settings|Graphics")	FORCEINLINE	int32 GetAAMethod() const {	return SettingsStruct.AAMethod;	}

	UFUNCTION(BlueprintPure , Category="Get Settings|Graphics")	FORCEINLINE	int32 GetAAQuality() const {	return SettingsStruct.AAQuality;	}
	
	UFUNCTION(BlueprintPure , Category="Get Settings|Graphics")	FORCEINLINE	int32 GetViewDistance() const {	return SettingsStruct.ViewDistance;	}
	
	UFUNCTION(BlueprintPure , Category="Get Settings|Graphics")	FORCEINLINE	int32 GetViewEffects() const {	return SettingsStruct.ViewEffects;	}
	
	UFUNCTION(BlueprintPure , Category="Get Settings|Graphics")	FORCEINLINE	bool GetMotionBlur() const {	return SettingsStruct.MotionBlur;	}
	
	UFUNCTION(BlueprintPure , Category="Get Settings|Graphics")	FORCEINLINE	bool GetLensFlares() const {	return SettingsStruct.LensFlares;	}
	
	UFUNCTION(BlueprintPure , Category="Get Settings|Graphics")	FORCEINLINE	bool GetSSReflections() const {	return SettingsStruct.SSReflections;	}
	
	UFUNCTION(BlueprintPure , Category="Get Settings|Graphics")	FORCEINLINE	bool GetBloom() const {	return SettingsStruct.Bloom;	}
	
	UFUNCTION(BlueprintPure , Category="Get Settings|Graphics")	FORCEINLINE	float GetResolutionScale() const {	return SettingsStruct.ResolutionScale;	}

	//Display
	UFUNCTION(BlueprintPure , Category="Get Settings|Display")	FORCEINLINE	int32 GetResolution() const {	return SettingsStruct.Resolution;	}

	UFUNCTION(BlueprintPure , Category="Get Settings|Display")	FORCEINLINE	int32 GetDisplayMode() const {	return SettingsStruct.DisplayMode;	}

	UFUNCTION(BlueprintPure , Category="Get Settings|Display")	FORCEINLINE	float GetGamma() const {	return SettingsStruct.Gamma;	}

	UFUNCTION(BlueprintPure , Category="Get Settings|Display")	FORCEINLINE	bool GetVSynch() const {	return SettingsStruct.VSynch;	}


	//Audio
	UFUNCTION(BlueprintPure , Category="Get Settings|Audio")	FORCEINLINE	float GetMaster() const {	return SettingsStruct.Master;	}

	UFUNCTION(BlueprintPure , Category="Get Settings|Audio")	FORCEINLINE	float GetMusic() const {	return SettingsStruct.Music;	}

	UFUNCTION(BlueprintPure , Category="Get Settings|Audio")	FORCEINLINE	float GetSFX() const {	return SettingsStruct.SFX;	}

	UFUNCTION(BlueprintPure , Category="Get Settings|Audio")	FORCEINLINE	float GetVoices() const {	return SettingsStruct.Voices;	}

	UFUNCTION(BlueprintPure , Category="Get Settings|Audio")	FORCEINLINE	float GetUI() const {	return SettingsStruct.UI;	}


	//Controls
	UFUNCTION(BlueprintPure , Category="Get Settings|Controls")	FORCEINLINE	bool GetInvertX() const {	return SettingsStruct.InvertX;	}

	UFUNCTION(BlueprintPure , Category="Get Settings|Controls")	FORCEINLINE	bool GetInvertY() const {	return SettingsStruct.InvertY;	}

	UFUNCTION(BlueprintPure , Category="Get Settings|Controls")	FORCEINLINE	float GetVibration() const {	return SettingsStruct.Vibration;	}

	UFUNCTION(BlueprintPure , Category="Get Settings|Controls")	FORCEINLINE	float GetMouseSensitivity() const {	return SettingsStruct.MouseSensitivity;	}


	//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< SET SETTINGS >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>


		//Gameplay
	UFUNCTION(BlueprintCallable , Category="Set Settings|Gameplay")		void SetDifficulty(int32 Difficulty) { SettingsStruct.Difficulty = Difficulty;	}

	UFUNCTION(BlueprintCallable , Category="Set Settings|Gameplay")		void SetLanguage(int32 Language)  {	 SettingsStruct.Language = Language;	}

	UFUNCTION(BlueprintCallable , Category="Set Settings|Gameplay")		void SetSubtitleLanguage(int32 SubtitleLanguage)  {	 SettingsStruct.SubtitleLanguage = SubtitleLanguage;	}

	UFUNCTION(BlueprintCallable , Category="Set Settings|Gameplay")		void SetSubtitles(bool Subtitles)  {	 SettingsStruct.Subtitles = Subtitles;	}
	
	UFUNCTION(BlueprintCallable , Category="Set Settings|Gameplay")		void SetColorBlindMode(bool ColorBlindMode)  {	 SettingsStruct.ColorBlindMode = ColorBlindMode;	}


	
	//Graphics
	UFUNCTION(BlueprintCallable , Category="Set Settings|Graphics")		void SetQualityPreset(int32 QualityPreset)  {	 SettingsStruct.QualityPreset = QualityPreset;	}

	UFUNCTION(BlueprintCallable , Category="Set Settings|Graphics")		void SetTextures(int32 Textures)  {	 SettingsStruct.Textures = Textures;	}

	UFUNCTION(BlueprintCallable , Category="Set Settings|Graphics")		void SetShadows(int32 Shadows)  {	 SettingsStruct.Shadows = Shadows;	}

	UFUNCTION(BlueprintCallable , Category="Set Settings|Graphics")		void SetFoliage(int32 Foliage)  {	 SettingsStruct.Foliage = Foliage;	}

	UFUNCTION(BlueprintCallable , Category="Set Settings|Graphics")		void SetAAMethod(int32 AAMethod)  {	 SettingsStruct.AAMethod = AAMethod;	}

	UFUNCTION(BlueprintCallable , Category="Set Settings|Graphics")		void SetAAQuality(int32 AAQuality)  {	 SettingsStruct.AAQuality = AAQuality;	}
	
	UFUNCTION(BlueprintCallable , Category="Set Settings|Graphics")		void SetViewDistance(int32 ViewDistance)  {	 SettingsStruct.ViewDistance = ViewDistance;	}
	
	UFUNCTION(BlueprintCallable , Category="Set Settings|Graphics")		void SetViewEffects(int32 ViewEffects)  {	 SettingsStruct.ViewEffects = ViewEffects;	}
	
	UFUNCTION(BlueprintCallable , Category="Set Settings|Graphics")		void SetMotionBlur(bool MotionBlur)  {	 SettingsStruct.MotionBlur = MotionBlur;	}
	
	UFUNCTION(BlueprintCallable , Category="Set Settings|Graphics")		void SetLensFlares(bool LensFlares)  {	 SettingsStruct.LensFlares = LensFlares;	}
	
	UFUNCTION(BlueprintCallable , Category="Set Settings|Graphics")		void SetSSReflections(bool SSReflection)  {	 SettingsStruct.SSReflections = SSReflection;	}
	
	UFUNCTION(BlueprintCallable , Category="Set Settings|Graphics")		void SetBloom(bool Bloom)  {	 SettingsStruct.Bloom = Bloom;	}
	
	UFUNCTION(BlueprintCallable , Category="Set Settings|Graphics")		void SetResolutionScale(float ResolutionScale)  {	 SettingsStruct.ResolutionScale = ResolutionScale;	}


	
	//Display
	UFUNCTION(BlueprintCallable , Category="Set Settings|Display")		void SetResolution(int32 Resolution)  {	 SettingsStruct.Resolution = Resolution;	}

	UFUNCTION(BlueprintCallable , Category="Set Settings|Display")		void SetDisplayMode(int32 DisplayMode)  {	 SettingsStruct.DisplayMode = DisplayMode;	}

	UFUNCTION(BlueprintCallable , Category="Set Settings|Display")		void SetGamma(float Gamma)  {	 SettingsStruct.Gamma = Gamma;	}

	UFUNCTION(BlueprintCallable , Category="Set Settings|Display")		void SetVSynch(bool VSynch)  {	 SettingsStruct.VSynch = VSynch;	}


	//Audio
	UFUNCTION(BlueprintCallable , Category="Set Settings|Audio")		void SetMaster(float Master)  {	 SettingsStruct.Master = Master;	}

	UFUNCTION(BlueprintCallable , Category="Set Settings|Audio")		void SetMusic(float Music)  {	 SettingsStruct.Music = Music;	}

	UFUNCTION(BlueprintCallable , Category="Set Settings|Audio")		void SetSFX(float SFX)  {	 SettingsStruct.SFX = SFX;	}

	UFUNCTION(BlueprintCallable , Category="Set Settings|Audio")		void SetVoices(float Voices)  {	 SettingsStruct.Voices = Voices;	}

	UFUNCTION(BlueprintCallable , Category="Set Settings|Audio")		void SetUI(float UI)  {	 SettingsStruct.UI = UI;	}


	//Controls
	UFUNCTION(BlueprintCallable , Category="Set Settings|Controls")		void SetInvertX( bool InvertX)  {	 SettingsStruct.InvertX = InvertX;	}

	UFUNCTION(BlueprintCallable , Category="Set Settings|Controls")		void SetInvertY(bool InvertY)  {	 SettingsStruct.InvertY = InvertY;	}

	UFUNCTION(BlueprintCallable , Category="Set Settings|Controls")		void SetVibration(float Vibration)  {	 SettingsStruct.Vibration = Vibration;	}

	UFUNCTION(BlueprintCallable , Category="Set Settings|Controls")		void SetMouseSensitivity(float MouseSensitivity)  {	 SettingsStruct.MouseSensitivity = MouseSensitivity;	}
};
