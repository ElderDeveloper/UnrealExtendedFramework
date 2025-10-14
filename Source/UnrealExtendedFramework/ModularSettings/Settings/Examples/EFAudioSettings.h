#pragma once

#include "CoreMinimal.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "UnrealExtendedFramework/ModularSettings/Settings/EFModularSettingsBase.h"
#include "EFAudioSettings.generated.h"

UCLASS(Blueprintable,EditInlineNew,  DisplayName= "Extended Master Volume")
class UNREALEXTENDEDFRAMEWORK_API UEFModularAudioSettingMasterVolume : public UEFModularSettingsFloat
{
	GENERATED_BODY()
public:
	UEFModularAudioSettingMasterVolume() : UEFModularSettingsFloat()
	{
		DefaultValue = 1.0f;
		ConfigCategory = TEXT("Audio");
		DisplayName = NSLOCTEXT("Settings", "MasterVolume", "Master Volume");
		SettingTag = FGameplayTag::RequestGameplayTag(FName("Settings.Audio.MasterVolume"));
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TObjectPtr<USoundMix> GlobalSoundMix;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TObjectPtr<USoundClass> SoundClass;

	virtual void Apply_Implementation() override;
};


UCLASS(Blueprintable,EditInlineNew,  DisplayName= "Extended Music Volume")
class UNREALEXTENDEDFRAMEWORK_API UEFModularAudioSettingMusicVolume : public UEFModularSettingsFloat
{
	GENERATED_BODY()
public:
	UEFModularAudioSettingMusicVolume() : UEFModularSettingsFloat()
	{
		DefaultValue = 1.0f;
		ConfigCategory = TEXT("Audio");
		DisplayName = NSLOCTEXT("Settings", "MusicVolume", "Music Volume");
		SettingTag = FGameplayTag::RequestGameplayTag(FName("Settings.Audio.MusicVolume"));
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TObjectPtr<USoundMix> GlobalSoundMix;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TObjectPtr<USoundClass> SoundClass;
	
	virtual void Apply_Implementation() override;
};


UCLASS(Blueprintable,EditInlineNew,  DisplayName= "Extended SFX Volume")
class UNREALEXTENDEDFRAMEWORK_API UEFModularAudioSettingSFXVolume : public UEFModularSettingsFloat
{
	GENERATED_BODY()
public:
	UEFModularAudioSettingSFXVolume() : UEFModularSettingsFloat()
	{
		DefaultValue = 1.0f;
		ConfigCategory = TEXT("Audio");
		DisplayName = NSLOCTEXT("Settings", "SFXVolume", "SFX Volume");
		SettingTag = FGameplayTag::RequestGameplayTag(FName("Settings.Audio.SFXVolume"));
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TObjectPtr<USoundMix> GlobalSoundMix;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TObjectPtr<USoundClass> SoundClass;
	
	virtual void Apply_Implementation() override;
};


UCLASS(Blueprintable,EditInlineNew,  DisplayName= "Extended Voice Volume")
class UNREALEXTENDEDFRAMEWORK_API UEFModularAudioSettingVoiceVolume : public UEFModularSettingsFloat
{
	GENERATED_BODY()
public:
	UEFModularAudioSettingVoiceVolume() : UEFModularSettingsFloat()
	{
		DefaultValue = 1.0f;
		ConfigCategory = TEXT("Audio");
		DisplayName = NSLOCTEXT("Settings", "VoiceVolume", "Voice Volume");
		SettingTag = FGameplayTag::RequestGameplayTag(FName("Settings.Audio.VoiceVolume"));
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TObjectPtr<USoundMix> GlobalSoundMix;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TObjectPtr<USoundClass> SoundClass;

	virtual void Apply_Implementation() override;
};


UCLASS(Blueprintable,EditInlineNew,  DisplayName= "Extended Ambient Volume")
class UNREALEXTENDEDFRAMEWORK_API UEFModularAudioSettingAmbientVolume : public UEFModularSettingsFloat
{
	GENERATED_BODY()
public:
	UEFModularAudioSettingAmbientVolume() : UEFModularSettingsFloat()
	{
		DefaultValue = 1.0f;
		ConfigCategory = TEXT("Audio");
		DisplayName = NSLOCTEXT("Settings", "AmbientVolume", "Ambient Volume");
		SettingTag = FGameplayTag::RequestGameplayTag(FName("Settings.Audio.AmbientVolume"));
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TObjectPtr<USoundMix> GlobalSoundMix;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TObjectPtr<USoundClass> SoundClass;

	virtual void Apply_Implementation() override;
};	


inline void UEFModularAudioSettingMasterVolume::Apply_Implementation()
{
	if (GlobalSoundMix && SoundClass)
	{
		UGameplayStatics::SetSoundMixClassOverride(GetWorld(), GlobalSoundMix.Get(), SoundClass.Get(), Value, 1.0f, 0.0f);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("GlobalSoundMix or SoundClass is not valid!"));
	}
}

inline void UEFModularAudioSettingMusicVolume::Apply_Implementation()
{
	if (GlobalSoundMix && SoundClass)
	{
		UGameplayStatics::SetSoundMixClassOverride(GetWorld(), GlobalSoundMix.Get(), SoundClass.Get(), Value, 1.0f, 0.0f);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("GlobalSoundMix or SoundClass is not valid!"));
	}
}

inline void UEFModularAudioSettingSFXVolume::Apply_Implementation()
{
	if (GlobalSoundMix && SoundClass)
	{
		UGameplayStatics::SetSoundMixClassOverride(GetWorld(), GlobalSoundMix.Get(), SoundClass.Get(), Value, 1.0f, 0.0f);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("GlobalSoundMix or SoundClass is not valid!"));
	}
}

inline void UEFModularAudioSettingVoiceVolume::Apply_Implementation()
{
	if (GlobalSoundMix && SoundClass)
	{
		UGameplayStatics::SetSoundMixClassOverride(GetWorld(), GlobalSoundMix.Get(), SoundClass.Get(), Value, 1.0f, 0.0f);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("GlobalSoundMix or SoundClass is not valid!"));
	}
}

inline void UEFModularAudioSettingAmbientVolume::Apply_Implementation()
{
	if (GlobalSoundMix && SoundClass)
	{
		UGameplayStatics::SetSoundMixClassOverride(GetWorld(), GlobalSoundMix.Get(), SoundClass.Get(), Value, 1.0f, 0.0f);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("GlobalSoundMix or SoundClass is not valid!"));
	}
}

 
