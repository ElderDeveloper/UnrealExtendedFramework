// Fill out your copyright notice in the Description page of Project Settings.


#include "ESAudioSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundMix.h"
#include "Sound/SoundClass.h"


UESAudioSubsystem::UESAudioSubsystem()
{
	MasterMix			= ConstructorHelpers::FObjectFinder<USoundMix>(TEXT("SoundMix'/UnrealExtendedFramework/Settings/AudioClasses/Mix_Master.Mix_Master'")).Object;
	AmbientMix			= ConstructorHelpers::FObjectFinder<USoundMix>(TEXT("SoundMix'/UnrealExtendedFramework/Settings/AudioClasses/Mix_Ambient.Mix_Ambient'")).Object;
	EffectMix			= ConstructorHelpers::FObjectFinder<USoundMix>(TEXT("SoundMix'/UnrealExtendedFramework/Settings/AudioClasses/Mix_Effect.Mix_Effect'")).Object;
	MusicMix			= ConstructorHelpers::FObjectFinder<USoundMix>(TEXT("SoundMix'/UnrealExtendedFramework/Settings/AudioClasses/Mix_Music.Mix_Music'")).Object;
	UserInterfaceMix	= ConstructorHelpers::FObjectFinder<USoundMix>(TEXT("SoundMix'/UnrealExtendedFramework/Settings/AudioClasses/Mix_UserInterface.Mix_UserInterface'")).Object;
	VoiceMix			= ConstructorHelpers::FObjectFinder<USoundMix>(TEXT("SoundMix'/UnrealExtendedFramework/Settings/AudioClasses/Mix_Voice.Mix_Voice'")).Object;


	MasterClass			= ConstructorHelpers::FObjectFinder<USoundClass>(TEXT("SoundClass'/UnrealExtendedFramework/Settings/AudioClasses/SC_Master.SC_Master'")).Object;
	AmbientClass		= ConstructorHelpers::FObjectFinder<USoundClass>(TEXT("SoundClass'/UnrealExtendedFramework/Settings/AudioClasses/SC_Ambient.SC_Ambient'")).Object;
	EffectClass			= ConstructorHelpers::FObjectFinder<USoundClass>(TEXT("SoundClass'/UnrealExtendedFramework/Settings/AudioClasses/SC_Effect.SC_Effect'")).Object;
	MusicClass			= ConstructorHelpers::FObjectFinder<USoundClass>(TEXT("SoundClass'/UnrealExtendedFramework/Settings/AudioClasses/SC_Music.SC_Music'")).Object;
	UserInterfaceClass	= ConstructorHelpers::FObjectFinder<USoundClass>(TEXT("SoundClass'/UnrealExtendedFramework/Settings/AudioClasses/SC_UserInterface.SC_UserInterface'")).Object;
	VoiceClass			= ConstructorHelpers::FObjectFinder<USoundClass>(TEXT("SoundClass'/UnrealExtendedFramework/Settings/AudioClasses/SC_Voice.SC_Voice'")).Object;
}


void UpdateSettingsSoundMixVolume(const UObject* WorldContextObject, class USoundMix* InSoundMixModifier, class USoundClass* InSoundClass, float Volume)
{
	
	if (WorldContextObject && InSoundMixModifier && InSoundClass)
	UGameplayStatics::SetSoundMixClassOverride(WorldContextObject,InSoundMixModifier,InSoundClass,Volume,InSoundClass->Properties.Pitch);
}

void UpdateSettingsSoundMixPitch(const UObject* WorldContextObject, class USoundMix* InSoundMixModifier, class USoundClass* InSoundClass, float Pitch)
{
	if (WorldContextObject && InSoundMixModifier && InSoundClass)
		UGameplayStatics::SetSoundMixClassOverride(WorldContextObject,InSoundMixModifier,InSoundClass,InSoundClass->Properties.Volume,Pitch);
}





void UESAudioSubsystem::UpdateSoundMixVolumeAdjuster(TEnumAsByte<EExtendedAudioSettingsType> AudioSettingsType,float VolumeAdjuster)
{
	switch (AudioSettingsType)
	{
	case Master:if(MasterMix->SoundClassEffects.IsValidIndex(0))MasterMix->SoundClassEffects[0].VolumeAdjuster = VolumeAdjuster;
		PushAudioMixModifiers();
		UpdateSettingsSoundMixVolume(GetWorld(),MasterMix,MasterClass,VolumeAdjuster);
		break;
		
	case Ambient:if(AmbientMix->SoundClassEffects.IsValidIndex(0))AmbientMix->SoundClassEffects[0].VolumeAdjuster = VolumeAdjuster;
		PushAudioMixModifiers();
		UpdateSettingsSoundMixVolume(GetWorld(),AmbientMix,AmbientClass,VolumeAdjuster);
		break;
		
	case Effect:if(EffectMix->SoundClassEffects.IsValidIndex(0))EffectMix->SoundClassEffects[0].VolumeAdjuster = VolumeAdjuster;
		PushAudioMixModifiers();
		UpdateSettingsSoundMixVolume(GetWorld(),EffectMix,EffectClass,VolumeAdjuster);
		break;
		
	case Music:if(MusicMix->SoundClassEffects.IsValidIndex(0))MusicMix->SoundClassEffects[0].VolumeAdjuster = VolumeAdjuster;
		PushAudioMixModifiers();
		UpdateSettingsSoundMixVolume(GetWorld(),MusicMix,MusicClass,VolumeAdjuster);
		break;
		
	case UserInterface:if(UserInterfaceMix->SoundClassEffects.IsValidIndex(0))UserInterfaceMix->SoundClassEffects[0].VolumeAdjuster = VolumeAdjuster;
		PushAudioMixModifiers();
		UpdateSettingsSoundMixVolume(GetWorld(),UserInterfaceMix,UserInterfaceClass,VolumeAdjuster);
		break;
		
	case Voice:if(VoiceMix->SoundClassEffects.IsValidIndex(0))VoiceMix->SoundClassEffects[0].VolumeAdjuster = VolumeAdjuster;
		PushAudioMixModifiers();
		UpdateSettingsSoundMixVolume(GetWorld(),VoiceMix,VoiceClass,VolumeAdjuster);
		break;

	default:break;
	}
}

void UESAudioSubsystem::UpdateSoundMixPitchAdjuster(TEnumAsByte<EExtendedAudioSettingsType> AudioSettingsType,float PitchAdjuster)
{
	switch (AudioSettingsType)
	{
	case Master:if(MasterMix->SoundClassEffects.IsValidIndex(0))MasterMix->SoundClassEffects[0].PitchAdjuster = PitchAdjuster;
		PushAudioMixModifiers();
		UpdateSettingsSoundMixPitch(GetWorld(),MasterMix,MasterClass,PitchAdjuster);
		break;
		
	case Ambient:if(AmbientMix->SoundClassEffects.IsValidIndex(0))AmbientMix->SoundClassEffects[0].PitchAdjuster = PitchAdjuster;
		PushAudioMixModifiers();
		UpdateSettingsSoundMixPitch(GetWorld(),AmbientMix,AmbientClass,PitchAdjuster);
		break;
		
	case Effect:if(EffectMix->SoundClassEffects.IsValidIndex(0))EffectMix->SoundClassEffects[0].PitchAdjuster = PitchAdjuster;
		PushAudioMixModifiers();
		UpdateSettingsSoundMixPitch(GetWorld(),EffectMix,EffectClass,PitchAdjuster);
		break;
		
	case Music:if(MusicMix->SoundClassEffects.IsValidIndex(0))MusicMix->SoundClassEffects[0].PitchAdjuster = PitchAdjuster;
		PushAudioMixModifiers();
		UpdateSettingsSoundMixPitch(GetWorld(),MusicMix,MusicClass,PitchAdjuster);
		break;
		
	case UserInterface:if(UserInterfaceMix->SoundClassEffects.IsValidIndex(0))UserInterfaceMix->SoundClassEffects[0].PitchAdjuster = PitchAdjuster;
		PushAudioMixModifiers();
		UpdateSettingsSoundMixPitch(GetWorld(),UserInterfaceMix,UserInterfaceClass,PitchAdjuster);
		break;
		
	case Voice:if(VoiceMix->SoundClassEffects.IsValidIndex(0))VoiceMix->SoundClassEffects[0].PitchAdjuster = PitchAdjuster;
		PushAudioMixModifiers();
		UpdateSettingsSoundMixVolume(GetWorld(),VoiceMix,VoiceClass,PitchAdjuster);
		break;

	default:break;
	}
}

void UESAudioSubsystem::UpdateSoundClassVolume(TEnumAsByte<EExtendedAudioSettingsType> AudioSettingsType, float Volume)
{
	switch (AudioSettingsType)
	{
	case Master:if(MasterClass) MasterClass->Properties.Volume = Volume;
		PushAudioMixModifiers();
		UpdateSettingsSoundMixVolume(GetWorld(),MasterMix ,MasterClass , Volume);
		break;
		
	case Ambient:if(AmbientClass) AmbientClass->Properties.Volume = Volume;
		PushAudioMixModifiers();
		UpdateSettingsSoundMixVolume(GetWorld(),AmbientMix ,AmbientClass , Volume);
		break;
		
	case Effect:if(EffectClass) EffectClass->Properties.Volume = Volume;
		PushAudioMixModifiers();
		UpdateSettingsSoundMixVolume(GetWorld(),EffectMix , EffectClass, Volume);
		break;
		
	case Music:if(MusicClass) MusicClass->Properties.Volume = Volume;
		PushAudioMixModifiers();
		UpdateSettingsSoundMixVolume(GetWorld(),MusicMix ,MusicClass , Volume);
		break;
		
	case UserInterface:if(UserInterfaceClass) UserInterfaceClass->Properties.Volume = Volume;
		PushAudioMixModifiers();
		UpdateSettingsSoundMixVolume(GetWorld(), UserInterfaceMix, UserInterfaceClass, Volume);
		break;
		
	case Voice:if(VoiceClass) VoiceClass->Properties.Volume = Volume;
		PushAudioMixModifiers();
		UpdateSettingsSoundMixVolume(GetWorld(),VoiceMix ,VoiceClass , Volume);
		break;

	default:break;
	}
}

void UESAudioSubsystem::UpdateSoundClassPitch(TEnumAsByte<EExtendedAudioSettingsType> AudioSettingsType, float Pitch)
{
	switch (AudioSettingsType)
	{
	case Master:if(MasterClass) MasterClass->Properties.Pitch = Pitch;
		PushAudioMixModifiers();
		UpdateSettingsSoundMixPitch(GetWorld(),MasterMix ,MasterClass , Pitch);
		break;
		
	case Ambient:if(AmbientClass) AmbientClass->Properties.Pitch = Pitch;
		PushAudioMixModifiers();
		UpdateSettingsSoundMixPitch(GetWorld(),AmbientMix ,AmbientClass , Pitch);
		break;
		
	case Effect:if(EffectClass) EffectClass->Properties.Pitch = Pitch;
		PushAudioMixModifiers();
		UpdateSettingsSoundMixPitch(GetWorld(),EffectMix , EffectClass, Pitch);
		break;
		
	case Music:if(MusicClass) MusicClass->Properties.Pitch = Pitch;
		PushAudioMixModifiers();
		UpdateSettingsSoundMixPitch(GetWorld(),MusicMix ,MusicClass , Pitch);
		break;
		
	case UserInterface:if(UserInterfaceClass) UserInterfaceClass->Properties.Pitch = Pitch;
		PushAudioMixModifiers();
		UpdateSettingsSoundMixPitch(GetWorld(), UserInterfaceMix, UserInterfaceClass, Pitch);
		break;
		
	case Voice:if(VoiceClass) VoiceClass->Properties.Pitch = Pitch;
		PushAudioMixModifiers();
		UpdateSettingsSoundMixPitch(GetWorld(),VoiceMix ,VoiceClass , Pitch);
		break;

	default:break;
	}
}



void UESAudioSubsystem::PushAudioMixModifiers()
{
	UGameplayStatics::PushSoundMixModifier(GetWorld(),MasterMix);
	UGameplayStatics::PushSoundMixModifier(GetWorld(),AmbientMix);
	UGameplayStatics::PushSoundMixModifier(GetWorld(),EffectMix);
	UGameplayStatics::PushSoundMixModifier(GetWorld(),MusicMix);
	UGameplayStatics::PushSoundMixModifier(GetWorld(),UserInterfaceMix);
	UGameplayStatics::PushSoundMixModifier(GetWorld(),VoiceMix);
}
