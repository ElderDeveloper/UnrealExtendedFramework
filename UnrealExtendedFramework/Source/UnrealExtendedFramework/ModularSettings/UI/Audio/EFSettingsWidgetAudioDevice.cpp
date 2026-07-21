// Fill out your copyright notice in the Description page of Project Settings.

#include "EFSettingsWidgetAudioDevice.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "UnrealExtendedFramework/ModularSettings/EFModularSettingsLibrary.h"
#include "UnrealExtendedFramework/ModularSettings/Settings/EFModularSettingsBase.h"

void UEFSettingsWidgetAudioDevice::NativeConstruct()
{
	RefreshDeviceTypeFromSettingsTag();
	Super::NativeConstruct();

	if (UEFAudioDeviceSubsystem* AudioDeviceSubsystem = GetAudioDeviceSubsystem())
	{
		AudioDeviceSubsystem->OnAudioDevicesChanged.AddUniqueDynamic(this, &UEFSettingsWidgetAudioDevice::HandleAudioDevicesChanged);
		AudioDeviceSubsystem->OnActiveAudioDeviceChanged.AddUniqueDynamic(this, &UEFSettingsWidgetAudioDevice::HandleActiveAudioDeviceChanged);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(AudioMeterTimerHandle, this, &UEFSettingsWidgetAudioDevice::UpdateAudioMeter, MeterUpdateInterval, true);
	}

	UpdateAudioMeter();
}

void UEFSettingsWidgetAudioDevice::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AudioMeterTimerHandle);
	}

	if (UEFAudioDeviceSubsystem* AudioDeviceSubsystem = GetAudioDeviceSubsystem())
	{
		AudioDeviceSubsystem->OnAudioDevicesChanged.RemoveDynamic(this, &UEFSettingsWidgetAudioDevice::HandleAudioDevicesChanged);
		AudioDeviceSubsystem->OnActiveAudioDeviceChanged.RemoveDynamic(this, &UEFSettingsWidgetAudioDevice::HandleActiveAudioDeviceChanged);
	}

	Super::NativeDestruct();
}

void UEFSettingsWidgetAudioDevice::OnTrackedSettingsChanged_Implementation(UEFModularSettingsBase* ChangedSetting)
{
	Super::OnTrackedSettingsChanged_Implementation(ChangedSetting);
	UpdateAudioMeter();
}

void UEFSettingsWidgetAudioDevice::UpdateAudioMeter()
{
	UpdateMeterVisuals(QueryRawTargetLevel());
}

void UEFSettingsWidgetAudioDevice::HandleAudioDevicesChanged(const TArray<FEFAudioDeviceInfo>& AudioDevices)
{
	RefreshOptions();
	UpdateAudioMeter();
}

void UEFSettingsWidgetAudioDevice::HandleActiveAudioDeviceChanged(EEFAudioDeviceType ChangedDeviceType, const FEFAudioDeviceInfo& DeviceInfo)
{
	if (ChangedDeviceType == DeviceType)
	{
		RefreshOptions();
		UpdateAudioMeter();
	}
}

void UEFSettingsWidgetAudioDevice::RefreshDeviceTypeFromSettingsTag()
{
	const FString SettingsTagName = SettingsTag.ToString();
	if (SettingsTagName.Equals(TEXT("Settings.Audio.InputDevice"), ESearchCase::CaseSensitive))
	{
		DeviceType = EEFAudioDeviceType::Input;
	}
	else if (SettingsTagName.Equals(TEXT("Settings.Audio.OutputDevice"), ESearchCase::CaseSensitive))
	{
		DeviceType = EEFAudioDeviceType::Output;
	}
}

UEFAudioDeviceSubsystem* UEFSettingsWidgetAudioDevice::GetAudioDeviceSubsystem() const
{
	return UEFAudioDeviceSubsystem::GetAudioDeviceSubsystem(this);
}

FString UEFSettingsWidgetAudioDevice::GetSelectedDeviceID() const
{
	const UEFModularSettingsMultiSelect* MultiSelectSetting = Cast<UEFModularSettingsMultiSelect>(
		UEFModularSettingsLibrary::GetModularSetting(this, SettingsTag, SettingsSource));
	if (MultiSelectSetting && MultiSelectSetting->Values.IsValidIndex(MultiSelectSetting->SelectedIndex))
	{
		return MultiSelectSetting->Values[MultiSelectSetting->SelectedIndex];
	}

	return TEXT("Default");
}

float UEFSettingsWidgetAudioDevice::QueryTargetLevel() const
{
	if (UEFAudioDeviceSubsystem* AudioDeviceSubsystem = GetAudioDeviceSubsystem())
	{
		return AudioDeviceSubsystem->GetDeviceLevel(DeviceType, GetSelectedDeviceID());
	}

	return 0.0f;
}

float UEFSettingsWidgetAudioDevice::QueryRawTargetLevel() const
{
	if (UEFAudioDeviceSubsystem* AudioDeviceSubsystem = GetAudioDeviceSubsystem())
	{
		return AudioDeviceSubsystem->GetDeviceLevelRaw(DeviceType, GetSelectedDeviceID());
	}

	return 0.0f;
}

void UEFSettingsWidgetAudioDevice::UpdateMeterVisuals(float RawTargetLevel)
{
	DisplayedAudioLevel = FMath::FInterpTo(DisplayedAudioLevel, FMath::Clamp(RawTargetLevel, 0.0f, 1.0f), MeterUpdateInterval, MeterSmoothingSpeed);

	if (AudioLevelProgressBar)
	{
		AudioLevelProgressBar->SetPercent(DisplayedAudioLevel);
	}

	if (RawAudioLevelText)
	{
		RawAudioLevelText->SetText(FText::FromString(FString::Printf(TEXT("%.4f"), RawTargetLevel)));
	}
}