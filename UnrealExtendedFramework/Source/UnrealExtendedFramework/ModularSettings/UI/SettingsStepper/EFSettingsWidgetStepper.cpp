// Fill out your copyright notice in the Description page of Project Settings.
#include "EFSettingsWidgetStepper.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "UnrealExtendedFramework/ModularSettings/EFModularSettingsLibrary.h"
#include "UnrealExtendedFramework/ModularSettings/Settings/EFModularSettingsBase.h"

void UEFSettingsWidgetStepper::NativeConstruct()
{
	Super::NativeConstruct();

	if (PreviousButton)
	{
		PreviousButton->OnClicked.AddDynamic(this, &UEFSettingsWidgetStepper::OnPreviousClicked);
	}

	if (NextButton)
	{
		NextButton->OnClicked.AddDynamic(this, &UEFSettingsWidgetStepper::OnNextClicked);
	}

	if (const auto Setting = UEFModularSettingsLibrary::GetModularSetting(this, SettingsTag, SettingsSource))
	{
		UpdateText(Setting);
	}
}

void UEFSettingsWidgetStepper::OnTrackedSettingsChanged_Implementation(UEFModularSettingsBase* ChangedSetting)
{
	UpdateText(ChangedSetting);
}

void UEFSettingsWidgetStepper::SettingsPreConstruct_Implementation()
{
	Super::SettingsPreConstruct_Implementation();

	if (ValueText)
	{
		FSlateFontInfo ValueTextFontInfo = ValueText->GetFont();
		ValueTextFontInfo.Size = ValueTextFontSize;
		ValueText->SetFont(FSlateFontInfo(ValueTextFontInfo));

		ValueText->SetText(FText::FromString(""));
	}
}

void UEFSettingsWidgetStepper::OnPreviousClicked()
{
	if (const auto Setting = UEFModularSettingsLibrary::GetModularSetting(this, SettingsTag, SettingsSource))
	{
		if (const auto BoolSetting = Cast<UEFModularSettingsBool>(Setting))
		{
			UEFModularSettingsLibrary::SetModularBool(this, SettingsTag, !BoolSetting->Value, SettingsSource, nullptr, ConfirmationType == EEFSettingConfirmationType::Instant);
		}
		else if (const auto MultiSelectSetting = Cast<UEFModularSettingsMultiSelect>(Setting))
		{
			UEFModularSettingsLibrary::AdjustModularIndex(this, SettingsTag, -1, true, SettingsSource, nullptr, ConfirmationType == EEFSettingConfirmationType::Instant);
		}
	}
}

void UEFSettingsWidgetStepper::OnNextClicked()
{
	if (const auto Setting = UEFModularSettingsLibrary::GetModularSetting(this, SettingsTag, SettingsSource))
	{
		if (const auto BoolSetting = Cast<UEFModularSettingsBool>(Setting))
		{
			UEFModularSettingsLibrary::SetModularBool(this, SettingsTag, !BoolSetting->Value, SettingsSource, nullptr, ConfirmationType == EEFSettingConfirmationType::Instant);
		}
		else if (const auto MultiSelectSetting = Cast<UEFModularSettingsMultiSelect>(Setting))
		{
			UEFModularSettingsLibrary::AdjustModularIndex(this, SettingsTag, 1, true, SettingsSource, nullptr, ConfirmationType == EEFSettingConfirmationType::Instant);
		}
	}
}


void UEFSettingsWidgetStepper::UpdateText(const UEFModularSettingsBase* Setting)
{
	if (!ValueText || !Setting)
	{
		return;
	}

	if (const auto BoolSetting = Cast<UEFModularSettingsBool>(Setting))
	{
		ValueText->SetText(BoolSetting->Value ? EnabledText : DisabledText);
	}
	else if (const auto MultiSelectSetting = Cast<UEFModularSettingsMultiSelect>(Setting))
	{
		if (MultiSelectSetting->DisplayNames.IsValidIndex(MultiSelectSetting->SelectedIndex))
		{
			ValueText->SetText(MultiSelectSetting->DisplayNames[MultiSelectSetting->SelectedIndex]);
		}
		else if (MultiSelectSetting->Values.IsValidIndex(MultiSelectSetting->SelectedIndex))
		{
			ValueText->SetText(FText::FromString(MultiSelectSetting->Values[MultiSelectSetting->SelectedIndex]));
		}
	}
}
