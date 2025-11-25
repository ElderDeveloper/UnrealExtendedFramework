// Fill out your copyright notice in the Description page of Project Settings.


#include "EFSettingsWidgetFloat.h"
#include "Components/SpinBox.h"
#include "UnrealExtendedFramework/ModularSettings/EFModularSettingsSubsystem.h"
#include "UnrealExtendedFramework/ModularSettings/Settings/EFModularSettingsBase.h"

void UEFSettingsWidgetFloat::NativeConstruct()
{
	Super::NativeConstruct();

	if (SpinBox)
	{
		if (const auto Subsystem = GetWorld()->GetGameInstance()->GetSubsystem<UEFModularSettingsSubsystem>())
		{
			if (const auto FloatSetting = Subsystem->GetSetting<UEFModularSettingsFloat>(SettingsTag))
			{
				SpinBox->SetMinValue(FloatSetting->Min);
				SpinBox->SetMaxValue(FloatSetting->Max);
				SpinBox->SetMinSliderValue(FloatSetting->Min);
				SpinBox->SetMaxSliderValue(FloatSetting->Max);
				SpinBox->SetValue(FloatSetting->Value);
			}
		}
		
		SpinBox->OnValueChanged.AddDynamic(this, &UEFSettingsWidgetFloat::OnValueChanged);
	}
}

void UEFSettingsWidgetFloat::OnTrackedSettingsChanged_Implementation(UEFModularSettingsBase* ChangedSetting)
{
	if (SpinBox)
	{
		if (const auto FloatSetting = Cast<UEFModularSettingsFloat>(ChangedSetting))
		{
			SpinBox->SetValue(FloatSetting->Value);
		}
	}
}

void UEFSettingsWidgetFloat::OnValueChanged(float NewValue)
{
	if (const auto Subsystem = GetWorld()->GetGameInstance()->GetSubsystem<UEFModularSettingsSubsystem>())
	{
		Subsystem->SetFloat(SettingsTag, NewValue);
	}
}
