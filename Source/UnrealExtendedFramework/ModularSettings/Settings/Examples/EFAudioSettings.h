#pragma once

#include "CoreMinimal.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "UnrealExtendedFramework/ModularSettings/Settings/AudioDevice/EFAudioDeviceSubsystem.h"
#include "UnrealExtendedFramework/ModularSettings/Settings/EFModularSettingsBase.h"
#include "EFAudioSettings.generated.h"

UCLASS(Abstract, Blueprintable, EditInlineNew)
class UNREALEXTENDEDFRAMEWORK_API UEFModularAudioDeviceSettingBase : public UEFModularSettingsMultiSelect
{
	GENERATED_BODY()

public:
	UEFModularAudioDeviceSettingBase()
	{
		ConfigCategory = TEXT("Audio");
		DefaultValue = TEXT("Default");
	}

	virtual void OnRegistered() override
	{
		Super::OnRegistered();
		RefreshValues();
	}

	virtual void RefreshValues_Implementation() override
	{
		const FString PreviouslySelectedValue = GetValueAsString();
		Values.Reset();
		DisplayNames.Reset();

		UEFAudioDeviceSubsystem* AudioDeviceSubsystem = UEFAudioDeviceSubsystem::GetAudioDeviceSubsystem(this);
		if (!AudioDeviceSubsystem)
		{
			Values.Add(DefaultValue);
			DisplayNames.Add(GetDefaultDeviceDisplayName());
			SelectedIndex = 0;
			return;
		}

		AudioDeviceSubsystem->RefreshDeviceList();

		const TArray<FEFAudioDeviceInfo> Devices = GetDeviceType() == EEFAudioDeviceType::Input
			? AudioDeviceSubsystem->GetInputDevices()
			: AudioDeviceSubsystem->GetOutputDevices();

		for (const FEFAudioDeviceInfo& DeviceInfo : Devices)
		{
			Values.Add(DeviceInfo.DeviceID);
			DisplayNames.Add(FText::FromString(DeviceInfo.DeviceName));
		}

		if (Values.Num() == 0)
		{
			Values.Add(DefaultValue);
			DisplayNames.Add(GetDefaultDeviceDisplayName());
		}

		const FEFAudioDeviceInfo ActiveDevice = GetDeviceType() == EEFAudioDeviceType::Input
			? AudioDeviceSubsystem->GetActiveInputDevice()
			: AudioDeviceSubsystem->GetActiveOutputDevice();

		FString DesiredValue = PreviouslySelectedValue;
		if (DesiredValue.IsEmpty() && !ActiveDevice.DeviceID.IsEmpty())
		{
			DesiredValue = ActiveDevice.DeviceID;
		}

		if (DesiredValue.IsEmpty())
		{
			DesiredValue = DefaultValue;
		}

		DesiredValue = AudioDeviceSubsystem->ResolvePreferredDeviceID(GetDeviceType(), DesiredValue);

		int32 DesiredIndex = Values.Find(DesiredValue);
		if (DesiredIndex == INDEX_NONE && !ActiveDevice.DeviceID.IsEmpty())
		{
			DesiredIndex = Values.Find(ActiveDevice.DeviceID);
		}

		SelectedIndex = DesiredIndex != INDEX_NONE ? DesiredIndex : 0;
	}

	virtual void SetValueFromString(const FString& Value) override
	{
		UEFAudioDeviceSubsystem* AudioDeviceSubsystem = UEFAudioDeviceSubsystem::GetAudioDeviceSubsystem(this);
		const FString ResolvedValue = AudioDeviceSubsystem
			? AudioDeviceSubsystem->ResolvePreferredDeviceID(GetDeviceType(), Value)
			: Value;

		Super::SetValueFromString(ResolvedValue);
	}

	virtual void Apply_Implementation() override
	{
		UEFAudioDeviceSubsystem* AudioDeviceSubsystem = UEFAudioDeviceSubsystem::GetAudioDeviceSubsystem(this);
		if (!AudioDeviceSubsystem)
		{
			UE_LOG(LogTemp, Warning, TEXT("AudioDeviceSubsystem is not available for %s."), *GetName());
			return;
		}

		RefreshValues();
		if (!Values.IsValidIndex(SelectedIndex))
		{
			UE_LOG(LogTemp, Warning, TEXT("No valid audio device selection is available for %s."), *GetName());
			return;
		}

		const FString DeviceID = Values[SelectedIndex];
		const EEFAudioDeviceSelectionResult Result = GetDeviceType() == EEFAudioDeviceType::Input
			? AudioDeviceSubsystem->SelectInputDevice(DeviceID)
			: AudioDeviceSubsystem->SelectOutputDevice(DeviceID);

		if (Result != EEFAudioDeviceSelectionResult::Success)
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to apply audio device '%s' for %s: %s"), *DeviceID, *GetName(), *AudioDeviceSubsystem->GetLastError());
		}

		RefreshValues();
	}

protected:
	virtual EEFAudioDeviceType GetDeviceType() const
	{
		return EEFAudioDeviceType::Output;
	}

	virtual FText GetDefaultDeviceDisplayName() const
	{
		return NSLOCTEXT("Settings", "DefaultAudioDevice", "System Default Device");
	}
};



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

	virtual void Apply_Implementation() override
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
	
	virtual void Apply_Implementation() override
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
	
	virtual void Apply_Implementation() override
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

	virtual void Apply_Implementation() override
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

	virtual void Apply_Implementation() override
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
};	



UCLASS(Blueprintable, EditInlineNew, DisplayName = "Extended Output Device")
class UNREALEXTENDEDFRAMEWORK_API UEFModularAudioSettingOutputDevice : public UEFModularAudioDeviceSettingBase
{
	GENERATED_BODY()

public:
	UEFModularAudioSettingOutputDevice()
	{
		DisplayName = NSLOCTEXT("Settings", "OutputDevice", "Output Device");
		SettingTag = FGameplayTag::RequestGameplayTag(FName("Settings.Audio.OutputDevice"));
	}

protected:
	virtual EEFAudioDeviceType GetDeviceType() const override
	{
		return EEFAudioDeviceType::Output;
	}

	virtual FText GetDefaultDeviceDisplayName() const override
	{
		return NSLOCTEXT("Settings", "DefaultOutputDevice", "System Default Output");
	}
};



UCLASS(Blueprintable, EditInlineNew, DisplayName = "Extended Input Device")
class UNREALEXTENDEDFRAMEWORK_API UEFModularAudioSettingInputDevice : public UEFModularAudioDeviceSettingBase
{
	GENERATED_BODY()

public:
	UEFModularAudioSettingInputDevice()
	{
		DisplayName = NSLOCTEXT("Settings", "InputDevice", "Input Device");
		SettingTag = FGameplayTag::RequestGameplayTag(FName("Settings.Audio.InputDevice"));
	}

protected:
	virtual EEFAudioDeviceType GetDeviceType() const override
	{
		return EEFAudioDeviceType::Input;
	}

	virtual FText GetDefaultDeviceDisplayName() const override
	{
		return NSLOCTEXT("Settings", "DefaultInputDevice", "System Default Input");
	}
};



