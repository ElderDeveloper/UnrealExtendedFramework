// Fill out your copyright notice in the Description page of Project Settings.


#include "EFSettingsWidgetMultiSelectStepper.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "UnrealExtendedFramework/ModularSettings/EFModularSettingsLibrary.h"
#include "UnrealExtendedFramework/ModularSettings/Settings/EFModularSettingsBase.h"

void UEFSettingsWidgetMultiSelectStepper::NativeConstruct()
{
	Super::NativeConstruct();

	if (PreviousButton)
	{
		PreviousButton->OnClicked.AddDynamic(this, &UEFSettingsWidgetMultiSelectStepper::OnPreviousClicked);
	}

	if (NextButton)
	{
		NextButton->OnClicked.AddDynamic(this, &UEFSettingsWidgetMultiSelectStepper::OnNextClicked);
	}

	if (const auto Setting = UEFModularSettingsLibrary::GetModularSetting(this, SettingsTag, SettingsSource))
	{
		UpdateText(Setting);
	}
}

void UEFSettingsWidgetMultiSelectStepper::OnTrackedSettingsChanged_Implementation(UEFModularSettingsBase* ChangedSetting)
{
	UpdateText(ChangedSetting);
}

void UEFSettingsWidgetMultiSelectStepper::SettingsPreConstruct_Implementation()
{
	Super::SettingsPreConstruct_Implementation();
	
	if (ValueText)
	{
		FSlateFontInfo ValueTextFontInfo = ValueText->GetFont();
		ValueTextFontInfo.Size = ValueTextFontSize;
		ValueText->SetFont(FSlateFontInfo(ValueTextFontInfo));
		
		ValueText->SetText(FText::FromString("0"));
	}
}

void UEFSettingsWidgetMultiSelectStepper::OnPreviousClicked()
{
	UEFModularSettingsLibrary::AdjustModularIndex(this, SettingsTag, -1, true, SettingsSource);
}

void UEFSettingsWidgetMultiSelectStepper::OnNextClicked()
{
	UEFModularSettingsLibrary::AdjustModularIndex(this, SettingsTag, 1, true, SettingsSource);
}

void UEFSettingsWidgetMultiSelectStepper::UpdateText(const UEFModularSettingsBase* Setting)
{
	if (ValueText)
	{
		if (const auto MultiSelectSetting = Cast<UEFModularSettingsMultiSelect>(Setting))
		{
			FText DisplayText = FText::GetEmpty();
			
			if (MultiSelectSetting->DisplayNames.IsValidIndex(MultiSelectSetting->SelectedIndex))
			{
				DisplayText = MultiSelectSetting->DisplayNames[MultiSelectSetting->SelectedIndex];
			}
			else if (MultiSelectSetting->Values.IsValidIndex(MultiSelectSetting->SelectedIndex))
			{
				DisplayText = FText::FromString(MultiSelectSetting->Values[MultiSelectSetting->SelectedIndex]);
			}
			
			if (MultiSelectSetting->IsIndexLocked(MultiSelectSetting->SelectedIndex))
			{
				DisplayText = FText::Format(NSLOCTEXT("Settings", "LockedFormat", "{0} (Locked)"), DisplayText);
				ValueText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.5f))); // Dimmed
			}
			else
			{
				ValueText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
			}

			ValueText->SetText(DisplayText);
		}
	}
}
