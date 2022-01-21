// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ESQualitySubsystem.generated.h"


#define EnsureGameSettings(Settings) if(!Settings) Settings = GetMutableDefault<UGameUserSettings>();

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSettingsQualityUpdated);

UCLASS(Config = DefaultSettingsPlugin)
class UNREALEXTENDEDSETTINGS_API UESQualitySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

	
private:

	UPROPERTY(Config , EditAnywhere , Category="Video")
	uint8 DisplayMode = 0;

	
	UPROPERTY(Config , EditAnywhere , Category="Video")
	FIntPoint ScreenResolution = {1920,1080};

	// Gets the current resolution scale as a normalized 0..1 value between MinScaleValue and MaxScaleValue
	UPROPERTY(Config , EditAnywhere , Category="Video")
	float ResolutionScale = 1;
	UPROPERTY(Config , EditAnywhere , Category="Video")
	float Brightness = 60;
	UPROPERTY(Config , EditAnywhere , Category="Video")
	uint8 OverallScalabilityLevel = 4;
	UPROPERTY(Config , EditAnywhere , Category="Video")
	uint8 MotionBlurQuality = 1;
	UPROPERTY(Config , EditAnywhere , Category="Video")
	bool Vsync = false;
	UPROPERTY(Config , EditAnywhere , Category="Video")
	uint8 AntiAliasingQuality = 3;


	UPROPERTY(Config , EditAnywhere , Category="Video Advanced")
	uint8 ViewDistance = 4;
	UPROPERTY(Config , EditAnywhere , Category="Video Advanced")
	uint8 ShadowQuality = 4;
	UPROPERTY(Config , EditAnywhere , Category="Video Advanced")
	uint8 VolumetricLighting = 4;
	UPROPERTY(Config , EditAnywhere , Category="Video Advanced")
	uint8 ScreenSpaceAmbientOcclusion = 4;
	UPROPERTY(Config , EditAnywhere , Category="Video Advanced")
	uint8 DepthOfFieldQuality = 4;
	UPROPERTY(Config , EditAnywhere , Category="Video Advanced")
	uint8 ScreenSpaceReflectionsQuality = 3;
	UPROPERTY(Config , EditAnywhere , Category="Video Advanced")
	uint8 GlobalReflections = 3;
	UPROPERTY(Config , EditAnywhere , Category="Video Advanced")
	int32 BloomQuality=4;
	UPROPERTY(Config , EditAnywhere , Category="Video Advanced")
	uint8 TextureQuality = 4;
	UPROPERTY(Config , EditAnywhere , Category="Video Advanced")
	uint8 TextureFiltering = 4;
	UPROPERTY(Config , EditAnywhere , Category="Video Advanced")
	uint8 FoliageQuality = 3;
	UPROPERTY(Config , EditAnywhere , Category="Video Advanced")
	uint8 VisualEffectsQuality = 4;
	UPROPERTY(Config , EditAnywhere , Category="Video Advanced")
	uint8 PostProcessingQuality = 4;
	UPROPERTY(Config , EditAnywhere , Category="Video Advanced")
	bool Tessellation = false;
	UPROPERTY(Config , EditAnywhere , Category="Video Advanced")
	uint8 LensFlareQuality = 4;
	UPROPERTY(Config , EditAnywhere , Category="Video Advanced")
	int32 Gamma = 2.2;
	
	UPROPERTY(Config , EditAnywhere , Category="Video Ray Trace")
	bool NvidiaDlls = false;

	UPROPERTY(Config , EditAnywhere , Category="Video Ray Trace")
	uint8 RayTracingPreset = 3;
	UPROPERTY(Config , EditAnywhere , Category="Video Ray Trace")
	bool RayTracedReflections = true;
	UPROPERTY(Config , EditAnywhere , Category="Video Ray Trace")
	bool RayTracedTransparentReflections = true;
	UPROPERTY(Config , EditAnywhere , Category="Video Ray Trace")
	bool RayTracedIndirectDiffuseLighting = true;
	UPROPERTY(Config , EditAnywhere , Category="Video Ray Trace")
	bool RayTracedContactShadows = true;
	UPROPERTY(Config , EditAnywhere , Category="Video Ray Trace")
	bool RayTracedDebris = false;

protected:

	
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override {}


public:

	UPROPERTY(BlueprintAssignable)
	FOnSettingsQualityUpdated OnSettingsQualityUpdated;



	UFUNCTION(BlueprintCallable , Category="Set Settings|Display")
	TEnumAsByte<EWindowMode::Type> GetDisplayMode() const ;
	
	UFUNCTION(BlueprintCallable , Category="Set Settings|Display")
	void SetDisplayMode(TEnumAsByte<EWindowMode::Type> Value , bool ApplyImmediately = false) ;
	void SetDisplayMode(uint8 Value , bool ApplyImmediately = false) ;
	

	// Returns the overall scalability level (can return -1 if the settings are custom)
	UFUNCTION(BlueprintPure , Category="Get Settings|Graphics")
	int32 GetOverallScalabilityLevel() const;
	
	// Changes all scalability settings at once based on a single overall quality level
	// @param Value 0:low, 1:medium, 2:high, 3:epic, 4:cinematic
	UFUNCTION(BlueprintCallable , Category="Set Settings|Graphics")
	void SetOverallScalabilityLevel(int32 Value , bool ApplyImmediately = false);

	

	// Gets the texture quality (0..4, higher is better)
	// @param Value 0:low, 1:medium, 2:high, 3:epic, 4:cinematic  (gets clamped if needed)
	UFUNCTION(BlueprintPure , Category="Get Settings|Graphics")
	int32 GetTextureQuality() const ;

	// Sets the texture quality (0..4, higher is better)
	// @param Value 0:low, 1:medium, 2:high, 3:epic, 4:cinematic  (gets clamped if needed)
	UFUNCTION(BlueprintCallable , Category="Set Settings|Graphics")
	void SetTexturesQuality(int32 Value , bool ApplyImmediately = false) ;

	

	// Gets the shadow quality (0..4, higher is better)
	// @param Value 0:low, 1:medium, 2:high, 3:epic, 4:cinematic (gets clamped if needed)
	UFUNCTION(BlueprintPure , Category="Get Settings|Graphics")
	int32 GetShadowQuality() const;

	// Sets the shadow quality (0..4, higher is better)
	// @param Value 0:low, 1:medium, 2:high, 3:epic, 4:cinematic (gets clamped if needed)
	UFUNCTION(BlueprintCallable , Category="Set Settings|Graphics")
	void SetShadowsQuality(int32 Value , bool ApplyImmediately = false) ;



	// Gets the foliage quality (0..4, higher is better)
	// @param Value 0:low, 1:medium, 2:high, 3:epic, 4:cinematic (gets clamped if needed)
	UFUNCTION(BlueprintPure , Category="Get Settings|Graphics")
	int32 GetFoliageQuality() const;
	
	// Sets the foliage quality (0..4, higher is better)
	// @param Value 0:low, 1:medium, 2:high, 3:epic, 4:cinematic (gets clamped if needed)
	UFUNCTION(BlueprintCallable , Category="Set Settings|Graphics")
	void SetFoliageQuality(int32 Value , bool ApplyImmediately = false);

	
	// Gets the anti-aliasing quality (0..4, higher is better)
	// @param Value 0:low, 1:medium, 2:high, 3:epic, 4:cinematic (gets clamped if needed)
	UFUNCTION(BlueprintPure , Category="Get Settings|Graphics")
	int32 GetAAQuality() const;

	// Sets the anti-aliasing quality (0..4, higher is better)
	// @param Value 0:low, 1:medium, 2:high, 3:epic, 4:cinematic (gets clamped if needed)
	UFUNCTION(BlueprintCallable , Category="Set Settings|Graphics")
	void SetAAQuality(int32 Value , bool ApplyImmediately = false) ;

	

	// Gets the view distance quality (0..4, higher is better)
	// @param Value 0:low, 1:medium, 2:high, 3:epic, 4:cinematic (gets clamped if needed)
	UFUNCTION(BlueprintPure , Category="Get Settings|Graphics")
	int32 GetViewDistance() const;


	// Sets the view distance quality (0..4, higher is better)
	// @param Value 0:low, 1:medium, 2:high, 3:epic, 4:cinematic (gets clamped if needed)
	UFUNCTION(BlueprintCallable , Category="Set Settings|Graphics")
	void SetViewDistance(int32 Value , bool ApplyImmediately = false)  ;

	

	// Gets the visual effects quality (0..4, higher is better)
	// @param Value 0:low, 1:medium, 2:high, 3:epic, 4:cinematic (gets clamped if needed)
	UFUNCTION(BlueprintPure , Category="Get Settings|Graphics")
	int32 GetVisualEffectQuality() const ;

	// Sets the visual effects quality (0..4, higher is better)
	// @param Value 0:low, 1:medium, 2:high, 3:epic, 4:cinematic (gets clamped if needed)
	UFUNCTION(BlueprintCallable , Category="Set Settings|Graphics")
	void SetVisualEffectQuality(int32 Value , bool ApplyImmediately = false) ;


	// Gets the post-processing quality (0..4, higher is better)
	// @param Value 0:low, 1:medium, 2:high, 3:epic, 4:cinematic (gets clamped if needed)
	UFUNCTION(BlueprintPure , Category="Get Settings|Graphics")
	int32 GetPostProcessingQuality() const;

	// Sets the post-processing quality (0..4, higher is better)
	// @param Value 0:low, 1:medium, 2:high, 3:epic, 4:cinematic (gets clamped if needed)
	UFUNCTION(BlueprintCallable , Category="Set Settings|Graphics")
	void SetPostProcessingQuality(int32 Value , bool ApplyImmediately = false)  ;

	
	
	UFUNCTION(BlueprintPure , Category="Get Settings|Graphics")
	int32 GetMotionBlurQuality() const;

	UFUNCTION(BlueprintCallable , Category="Set Settings|Graphics")
	void SetMotionBlurQuality(int32 Value , bool ApplyImmediately = false)  ;

	
	
	UFUNCTION(BlueprintPure , Category="Get Settings|Graphics")
	uint8 GetLensFlareQuality() const;

	UFUNCTION(BlueprintCallable , Category="Set Settings|Graphics")
	void SetLensFlareQuality(uint8 Value , bool ApplyImmediately = false);

	
	
	UFUNCTION(BlueprintPure , Category="Get Settings|Graphics")
	uint8 GetSSReflections() const;

	UFUNCTION(BlueprintCallable , Category="Set Settings|Graphics")
	void SetSSReflections(uint8 Value , bool ApplyImmediately = false);

	
	
	UFUNCTION(BlueprintPure , Category="Get Settings|Graphics")
	int32 GetBloomQuality() const;

	UFUNCTION(BlueprintCallable , Category="Set Settings|Graphics")
	void SetBloomQuality(int32 Value , bool ApplyImmediately = false);

	
	// Gets the current resolution scale as a normalized 0..1 value between MinScaleValue and MaxScaleValue
	UFUNCTION(BlueprintPure , Category="Get Settings|Graphics")
	float GetResolutionScale() const;

	// Sets the current resolution scale as a normalized 0..1 value between MinScaleValue and MaxScaleValue
	UFUNCTION(BlueprintCallable , Category="Set Settings|Graphics")
	void SetResolutionScale(float Value , bool ApplyImmediately = false) ;

	
	/** Returns the user setting for game screen resolution, in pixels. */
	UFUNCTION(BlueprintPure , Category="Get Settings|Graphics")
	FIntPoint GetScreenResolution() const;

	/** Sets the user setting for game screen resolution, in pixels. */
	UFUNCTION(BlueprintCallable , Category="Set Settings|Display")
	void SetScreenResolution(FIntPoint Value , bool ApplyImmediately = false);


	/** Returns the user setting for vsync. */
	UFUNCTION(BlueprintCallable , Category="Set Settings|Display")
	bool GetVSynch() const ;

	/** Sets the user setting for vsync. See UGameUserSettings::bUseVSync. */
	UFUNCTION(BlueprintCallable , Category="Set Settings|Display")
	void SetVSynch(bool Value , bool ApplyImmediately = false) ;



	UFUNCTION(BlueprintCallable , Category="Set Settings|Display")
	float GetGamma() const;
	
	UFUNCTION(BlueprintCallable , Category="Set Settings|Display")
	void SetGamma(float Value , bool ApplyImmediately = false) ;

	

	UFUNCTION(BlueprintCallable , Category="Set Settings")
	void ApplyAllQualitySettings(bool ApplyImmediately = false);


private:

	
	void ExecuteConsoleCommand(FString Str , FString Value ,FString Prefix = " ");

	IConsoleVariable* GetConsoleVariable(const TCHAR* Name) const;
	
	void CheckQualitySettingsConfigChanges();


public:


	//UFUNCTION(BlueprintCallable , Category="Set Settings|Display")
	//FORCEINLINE TEnumAsByte<EWindowMode::Type> GetConfigDisplayMode() const {	return DisplayMode  ; }
	
	UFUNCTION(BlueprintPure , Category="Get Settings|Graphics")
	FORCEINLINE int32 GetConfigQualityPreset() const {	return OverallScalabilityLevel ; }

	// Gets the texture quality (0..4, higher is better)
	// @param Value 0:low, 1:medium, 2:high, 3:epic, 4:cinematic  (gets clamped if needed)
	UFUNCTION(BlueprintPure , Category="Get Settings|Graphics")
	FORCEINLINE int32 GetConfigTextureQuality() const  {	return  TextureQuality; } 

	// Gets the shadow quality (0..4, higher is better)
	// @param Value 0:low, 1:medium, 2:high, 3:epic, 4:cinematic (gets clamped if needed)
	UFUNCTION(BlueprintPure , Category="Get Settings|Graphics")
	FORCEINLINE int32 GetConfigShadowQuality() const  {	return ShadowQuality ; }


	// Gets the foliage quality (0..4, higher is better)
	// @param Value 0:low, 1:medium, 2:high, 3:epic, 4:cinematic (gets clamped if needed)
	UFUNCTION(BlueprintPure , Category="Get Settings|Graphics")
	FORCEINLINE int32 GetConfigFoliageQuality() const  {	return FoliageQuality ; }

	// Gets the anti-aliasing quality (0..4, higher is better)
	// @param Value 0:low, 1:medium, 2:high, 3:epic, 4:cinematic (gets clamped if needed)
	UFUNCTION(BlueprintPure , Category="Get Settings|Graphics")
	FORCEINLINE int32 GetConfigAAQuality() const {	return AntiAliasingQuality ; } 

	// Gets the view distance quality (0..4, higher is better)
	// @param Value 0:low, 1:medium, 2:high, 3:epic, 4:cinematic (gets clamped if needed)
	UFUNCTION(BlueprintPure , Category="Get Settings|Graphics")
	FORCEINLINE int32 GetConfigViewDistance() const  {	return ViewDistance ; }

	// Gets the visual effects quality (0..4, higher is better)
	// @param Value 0:low, 1:medium, 2:high, 3:epic, 4:cinematic (gets clamped if needed)
	UFUNCTION(BlueprintPure , Category="Get Settings|Graphics")
	FORCEINLINE int32 GetConfigVisualEffectQuality() const   {	return VisualEffectsQuality ; }

	// Gets the post-processing quality (0..4, higher is better)
	// @param Value 0:low, 1:medium, 2:high, 3:epic, 4:cinematic (gets clamped if needed)
	UFUNCTION(BlueprintPure , Category="Get Settings|Graphics")
	FORCEINLINE int32 GetConfigPostProcessingQuality() const {	return PostProcessingQuality ; }
	
	UFUNCTION(BlueprintPure , Category="Get Settings|Graphics")
	FORCEINLINE int32 GetConfigMotionBlurQuality() const  {	return MotionBlurQuality ; }
	
	UFUNCTION(BlueprintPure , Category="Get Settings|Graphics")
	FORCEINLINE uint8 GetConfigLensFlareQuality() const  {	return LensFlareQuality ; }
	
	UFUNCTION(BlueprintPure , Category="Get Settings|Graphics")
	FORCEINLINE uint8 GetConfigSSReflections() const  {	return ScreenSpaceReflectionsQuality ; }
	
	UFUNCTION(BlueprintPure , Category="Get Settings|Graphics")
	FORCEINLINE int32 GetConfigBloomQuality() const  {	return BloomQuality ; }

	// Gets the current resolution scale as a normalized 0..1 value between MinScaleValue and MaxScaleValue
	UFUNCTION(BlueprintPure , Category="Get Settings|Graphics")
	FORCEINLINE float GetConfigResolutionScale() const  {	return ResolutionScale ; }
	
	/** Returns the user setting for game screen resolution, in pixels. */
	UFUNCTION(BlueprintPure , Category="Get Settings|Graphics")
	FORCEINLINE FIntPoint GetConfigScreenResolution() const  {	return ScreenResolution ; }

	/** Returns the user setting for vsync. */
	UFUNCTION(BlueprintCallable , Category="Set Settings|Display")
	FORCEINLINE bool GetConfigVSynch() const   {	return Vsync ; }
	
	UFUNCTION(BlueprintCallable , Category="Set Settings|Display")
	FORCEINLINE float GetConfigGamma() const  {	return Gamma ; }
};
