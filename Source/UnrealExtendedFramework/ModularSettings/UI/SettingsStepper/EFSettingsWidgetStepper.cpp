// Fill out your copyright notice in the Description page of Project Settings.


#include "EFSettingsWidgetStepper.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "UnrealExtendedFramework/ModularSettings/EFModularSettingsSubsystem.h"
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

	if (const auto Subsystem = GetWorld()->GetGameInstance()->GetSubsystem<UEFModularSettingsSubsystem>())
	{
		if (const auto Setting = Subsystem->GetSettingByTag(SettingsTag))
		{
			UpdateText(Setting);
		}
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
	if (const auto Subsystem = GetWorld()->GetGameInstance()->GetSubsystem<UEFModularSettingsSubsystem>())
	{
		if (const auto Setting = Subsystem->GetSettingByTag(SettingsTag))
		{
			if (const auto BoolSetting = Cast<UEFModularSettingsBool>(Setting))
			{
				Subsystem->SetBool(SettingsTag, !BoolSetting->Value);
			}
			else if (const auto MultiSelectSetting = Cast<UEFModularSettingsMultiSelect>(Setting))
			{
				Subsystem->AddIndex(SettingsTag, -1);
			}
		}
	}
}

void UEFSettingsWidgetStepper::OnNextClicked()
{
	if (const auto Subsystem = GetWorld()->GetGameInstance()->GetSubsystem<UEFModularSettingsSubsystem>())
	{
		if (const auto Setting = Subsystem->GetSettingByTag(SettingsTag))
		{
			if (const auto BoolSetting = Cast<UEFModularSettingsBool>(Setting))
			{
				Subsystem->SetBool(SettingsTag, !BoolSetting->Value);
			}
			else if (const auto MultiSelectSetting = Cast<UEFModularSettingsMultiSelect>(Setting))
			{
				Subsystem->AddIndex(SettingsTag, 1);
			}
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
		ValueText->SetText(BoolSetting->Value ? FText::FromString(TEXT("Enabled")) : FText::FromString(TEXT("Disabled")));
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
