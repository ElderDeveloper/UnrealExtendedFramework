// Fill out your copyright notice in the Description page of Project Settings.


#include "EFSettingsWidgetMultiSelectStepper.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "UnrealExtendedFramework/ModularSettings/EFModularSettingsSubsystem.h"
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

	if (const auto Subsystem = GetWorld()->GetGameInstance()->GetSubsystem<UEFModularSettingsSubsystem>())
	{
		if (const auto Setting = Subsystem->GetSettingByTag(SettingsTag))
		{
			UpdateText(Setting);
		}
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
	if (const auto Subsystem = GetWorld()->GetGameInstance()->GetSubsystem<UEFModularSettingsSubsystem>())
	{
		int32 CurrentIndex = Subsystem->GetSelectedOptionIndex(SettingsTag);
		TArray<FText> Options = Subsystem->GetOptions(SettingsTag);
		
		if (Options.Num() > 0)
		{
			int32 NewIndex = CurrentIndex - 1;
			if (NewIndex < 0)
			{
				NewIndex = Options.Num() - 1;
			}
			Subsystem->SetIndex(SettingsTag, NewIndex);
		}
	}
}

void UEFSettingsWidgetMultiSelectStepper::OnNextClicked()
{
	if (const auto Subsystem = GetWorld()->GetGameInstance()->GetSubsystem<UEFModularSettingsSubsystem>())
	{
		int32 CurrentIndex = Subsystem->GetSelectedOptionIndex(SettingsTag);
		TArray<FText> Options = Subsystem->GetOptions(SettingsTag);
		
		if (Options.Num() > 0)
		{
			int32 NewIndex = (CurrentIndex + 1) % Options.Num();
			Subsystem->SetIndex(SettingsTag, NewIndex);
		}
	}
}

void UEFSettingsWidgetMultiSelectStepper::UpdateText(const UEFModularSettingsBase* Setting)
{
	if (ValueText)
	{
		if (const auto MultiSelectSetting = Cast<UEFModularSettingsMultiSelect>(Setting))
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
}
