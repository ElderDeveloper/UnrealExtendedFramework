// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ESAudioSubsystem.generated.h"


UENUM(BlueprintType)
enum EExtendedAudioSettingsType
{
	Master,
	Ambient,
	Effect,
	Music,
	UserInterface,
	Voice
};
/**
 * 
 */
UCLASS()
class UNREALEXTENDEDSETTINGS_API UESAudioSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
	UESAudioSubsystem();
	
	UPROPERTY()
	USoundMix* MasterMix;
	UPROPERTY()
	USoundMix* AmbientMix;
	UPROPERTY()
	USoundMix* EffectMix;
	UPROPERTY()
	USoundMix* MusicMix;
	UPROPERTY()
	USoundMix* UserInterfaceMix;
	UPROPERTY()
	USoundMix* VoiceMix;


	UPROPERTY()
	USoundClass* MasterClass;
	UPROPERTY()
	USoundClass* AmbientClass;
	UPROPERTY()
	USoundClass* EffectClass;
	UPROPERTY()
	USoundClass* MusicClass;
	UPROPERTY()
	USoundClass* UserInterfaceClass;
	UPROPERTY()
	USoundClass* VoiceClass;
	
public:
	
	UFUNCTION(BlueprintCallable , Category="Audio Settings") 
	void UpdateSoundMixVolumeAdjuster(TEnumAsByte<EExtendedAudioSettingsType> AudioSettingsType , float VolumeAdjuster = 1);
	
	UFUNCTION(BlueprintCallable , Category="Audio Settings")
	void UpdateSoundMixPitchAdjuster(TEnumAsByte<EExtendedAudioSettingsType> AudioSettingsType ,float PitchAdjuster = 1);

	UFUNCTION(BlueprintCallable , Category="Audio Settings")
	void UpdateSoundClassVolume(TEnumAsByte<EExtendedAudioSettingsType> AudioSettingsType ,float Volume = 1);

	UFUNCTION(BlueprintCallable , Category="Audio Settings")
	void UpdateSoundClassPitch(TEnumAsByte<EExtendedAudioSettingsType> AudioSettingsType ,float Pitch = 1);

	void PushAudioMixModifiers();
};
