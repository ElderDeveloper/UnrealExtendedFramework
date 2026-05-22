// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UnrealExtendedFramework/ModularSettings/Settings/AudioDevice/EFAudioDeviceSubsystem.h"
#include "UnrealExtendedFramework/ModularSettings/UI/SettingsMultiSelect/EFSettingsWidgetMultiSelect.h"
#include "EFSettingsWidgetAudioDevice.generated.h"

class UProgressBar;
class UTextBlock;

UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFSettingsWidgetAudioDevice : public UEFSettingsWidgetMultiSelect
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void OnTrackedSettingsChanged_Implementation(UEFModularSettingsBase* ChangedSetting) override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Settings")
	UProgressBar* AudioLevelProgressBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Settings")
	UTextBlock* RawAudioLevelText;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	EEFAudioDeviceType DeviceType = EEFAudioDeviceType::Output;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio", meta = (ClampMin = "0.01"))
	float MeterUpdateInterval = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio", meta = (ClampMin = "0.0"))
	float MeterSmoothingSpeed = 8.0f;

	UFUNCTION()
	void UpdateAudioMeter();

	UFUNCTION()
	void HandleAudioDevicesChanged(const TArray<FEFAudioDeviceInfo>& AudioDevices);

	UFUNCTION()
	void HandleActiveAudioDeviceChanged(EEFAudioDeviceType ChangedDeviceType, const FEFAudioDeviceInfo& DeviceInfo);

	void RefreshDeviceTypeFromSettingsTag();
	UEFAudioDeviceSubsystem* GetAudioDeviceSubsystem() const;
	FString GetSelectedDeviceID() const;
	float QueryTargetLevel() const;
	float QueryRawTargetLevel() const;
	void UpdateMeterVisuals(float RawTargetLevel);

	FTimerHandle AudioMeterTimerHandle;
	float DisplayedAudioLevel = 0.0f;
};