#pragma once

#include "CoreMinimal.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "UnrealExtendedFramework/ModularSettings/Settings/EFModularSettingBase.h"
#include "EFAudioSettings.generated.h"

UENUM(BlueprintType)
enum class EAudioSettingType : uint8
{
	MasterVolume UMETA(DisplayName = "Master Volume"),
	MusicVolume UMETA(DisplayName = "Music Volume"),
	SFXVolume UMETA(DisplayName = "SFX Volume"),
	VoiceVolume UMETA(DisplayName = "Voice Volume"),
	AmbientVolume UMETA(DisplayName = "Ambient Volume")
};

USTRUCT(BlueprintType,Blueprintable)
struct FModularAudioSetting
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio Settings")
	EAudioSettingType AudioType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio Settings")
	TObjectPtr<USoundClass> SoundClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio Settings")
	float Value = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio Settings")
	float Min = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio Settings")
	float Max = 1.0f;

	FModularAudioSetting()
		: AudioType(EAudioSettingType::MasterVolume), Value(1.0f), Min(0.0f), Max(1.0f)
	{}
	FModularAudioSetting(EAudioSettingType InAudioType, float InValue, float InMin = 0.0f, float InMax = 1.0f)
		: AudioType(InAudioType), Value(InValue), Min(InMin), Max(InMax)
	{}

	void SetValueWithString(const FString& ValueString)
	{
		Value = FMath::Clamp(FCString::Atof(*ValueString), Min, Max);
	}
	
};

UCLASS(Abstract, Blueprintable, EditInlineNew)
class UNREALEXTENDEDFRAMEWORK_API UEFModularAudioSettingMasterVolume : public UEFModularSettingBase
{
	GENERATED_BODY()
public:
	UEFModularAudioSettingMasterVolume() : UEFModularSettingBase()
	{
		DefaultValue = "1.0";
		ConfigCategory = TEXT("Audio");
		DisplayName = NSLOCTEXT("Settings", "MasterVolume", "Master Volume");
		SettingTag = FGameplayTag::RequestGameplayTag(FName("Settings.Audio.MasterVolume"));
		MasterVolume = FModularAudioSetting(EAudioSettingType::MasterVolume, 1.0f, 0.0f, 1.0f);
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TObjectPtr<USoundMix> GlobalSoundMix;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio Settings")
	FModularAudioSetting MasterVolume;

	virtual void Apply() override;
	virtual FString GetValueAsString() const override { return FString::SanitizeFloat(MasterVolume.Value); }
	virtual void SetValueFromString(const FString& ValueString) override { MasterVolume.SetValueWithString(ValueString); }
	virtual void ResetToDefault() override { MasterVolume.SetValueWithString(DefaultValue); }
};



UCLASS(Abstract, Blueprintable, EditInlineNew)
class UNREALEXTENDEDFRAMEWORK_API UEFModularAudioSettingMusicVolume : public UEFModularSettingBase
{
	GENERATED_BODY()
public:
	UEFModularAudioSettingMusicVolume() : UEFModularSettingBase()
	{
		DefaultValue = "1.0";
		ConfigCategory = TEXT("Audio");
		DisplayName = NSLOCTEXT("Settings", "MusicVolume", "Music Volume");
		SettingTag = FGameplayTag::RequestGameplayTag(FName("Settings.Audio.MusicVolume"));
		MusicVolume = FModularAudioSetting(EAudioSettingType::MusicVolume, 1.0f, 0.0f, 1.0f);
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TObjectPtr<USoundMix> GlobalSoundMix;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio Settings")
	FModularAudioSetting MusicVolume;

	virtual void Apply() override;
	virtual FString GetValueAsString() const override { return FString::SanitizeFloat(MusicVolume.Value); }
	virtual void SetValueFromString(const FString& ValueString) override { MusicVolume.SetValueWithString(ValueString); }
	virtual void ResetToDefault() override { MusicVolume.SetValueWithString(DefaultValue); }
};


UCLASS(Abstract, Blueprintable, EditInlineNew)
class UNREALEXTENDEDFRAMEWORK_API UEFModularAudioSettingSFXVolume : public UEFModularSettingBase
{
	GENERATED_BODY()
public:
	UEFModularAudioSettingSFXVolume() : UEFModularSettingBase()
	{
		DefaultValue = "1.0";
		ConfigCategory = TEXT("Audio");
		DisplayName = NSLOCTEXT("Settings", "SFXVolume", "SFX Volume");
		SettingTag = FGameplayTag::RequestGameplayTag(FName("Settings.Audio.SFXVolume"));
		SFXVolume = FModularAudioSetting(EAudioSettingType::SFXVolume, 1.0f, 0.0f, 1.0f);
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TObjectPtr<USoundMix> GlobalSoundMix;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio Settings")
	FModularAudioSetting SFXVolume;

	virtual void Apply() override;
	virtual FString GetValueAsString() const override { return FString::SanitizeFloat(SFXVolume.Value); }
	virtual void SetValueFromString(const FString& ValueString) override { SFXVolume.SetValueWithString(ValueString); }
	virtual void ResetToDefault() override { SFXVolume.SetValueWithString(DefaultValue); }
};


UCLASS(Abstract, Blueprintable, EditInlineNew)
class UNREALEXTENDEDFRAMEWORK_API UEFModularAudioSettingVoiceVolume : public UEFModularSettingBase
{
	GENERATED_BODY()
public:
	UEFModularAudioSettingVoiceVolume() : UEFModularSettingBase()
	{
		DefaultValue = "1.0";
		ConfigCategory = TEXT("Audio");
		DisplayName = NSLOCTEXT("Settings", "VoiceVolume", "Voice Volume");
		SettingTag = FGameplayTag::RequestGameplayTag(FName("Settings.Audio.VoiceVolume"));
		VoiceVolume = FModularAudioSetting(EAudioSettingType::VoiceVolume, 1.0f, 0.0f, 1.0f);
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TObjectPtr<USoundMix> GlobalSoundMix;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio Settings")
	FModularAudioSetting VoiceVolume;

	virtual void Apply() override;
	virtual FString GetValueAsString() const override { return FString::SanitizeFloat(VoiceVolume.Value); }
	virtual void SetValueFromString(const FString& ValueString) override { VoiceVolume.SetValueWithString(ValueString); }
	virtual void ResetToDefault() override { VoiceVolume.SetValueWithString(DefaultValue); }
};



UCLASS(Abstract, Blueprintable, EditInlineNew)
class UNREALEXTENDEDFRAMEWORK_API UEFModularAudioSettingAmbientVolume : public UEFModularSettingBase
{
	GENERATED_BODY()
public:
	UEFModularAudioSettingAmbientVolume() : UEFModularSettingBase()
	{
		DefaultValue = "1.0";
		ConfigCategory = TEXT("Audio");
		DisplayName = NSLOCTEXT("Settings", "AmbientVolume", "Ambient Volume");
		SettingTag = FGameplayTag::RequestGameplayTag(FName("Settings.Audio.AmbientVolume"));
		AmbientVolume = FModularAudioSetting(EAudioSettingType::AmbientVolume, 1.0f, 0.0f, 1.0f);
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TObjectPtr<USoundMix> GlobalSoundMix;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio Settings")
	FModularAudioSetting AmbientVolume;

	virtual void Apply() override;
	virtual FString GetValueAsString() const override { return FString::SanitizeFloat(AmbientVolume.Value); }
	virtual void SetValueFromString(const FString& ValueString) override { AmbientVolume.SetValueWithString(ValueString); }
	virtual void ResetToDefault() override { AmbientVolume.SetValueWithString(DefaultValue); }
};



inline void UEFModularAudioSettingMasterVolume::Apply()
{
	if (GlobalSoundMix)
	{
		UGameplayStatics::SetSoundMixClassOverride(GetWorld(), GlobalSoundMix.Get(), MasterVolume.SoundClass.Get(), MasterVolume.Value, 1.0f, 0.0f);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("GlobalSoundMix is not valid!"));
	}
}


inline void UEFModularAudioSettingMusicVolume::Apply()
{
	if (GlobalSoundMix)
	{
		UGameplayStatics::SetSoundMixClassOverride(GetWorld(), GlobalSoundMix.Get(), MusicVolume.SoundClass.Get(), MusicVolume.Value, 1.0f, 0.0f);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("GlobalSoundMix is not valid!"));
	}
}

inline void UEFModularAudioSettingSFXVolume::Apply()
{
	if (GlobalSoundMix)
	{
		UGameplayStatics::SetSoundMixClassOverride(GetWorld(), GlobalSoundMix.Get(), SFXVolume.SoundClass.Get(), SFXVolume.Value, 1.0f, 0.0f);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("GlobalSoundMix is not valid!"));
	}
}

inline void UEFModularAudioSettingVoiceVolume::Apply()
{
	if (GlobalSoundMix)
	{
		UGameplayStatics::SetSoundMixClassOverride(GetWorld(), GlobalSoundMix.Get(), VoiceVolume.SoundClass.Get(), VoiceVolume.Value, 1.0f, 0.0f);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("GlobalSoundMix is not valid!"));
	}
}

inline void UEFModularAudioSettingAmbientVolume::Apply()
{
	if (GlobalSoundMix)
	{
		UGameplayStatics::SetSoundMixClassOverride(GetWorld(), GlobalSoundMix.Get(), AmbientVolume.SoundClass.Get(), AmbientVolume.Value, 1.0f, 0.0f);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("GlobalSoundMix is not valid!"));
	}
}

 
