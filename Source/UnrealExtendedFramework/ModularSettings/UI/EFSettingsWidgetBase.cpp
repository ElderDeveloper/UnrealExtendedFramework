// Fill out your copyright notice in the Description page of Project Settings.

#include "EFSettingsWidgetBase.h"
#include "../EFModularSettingsSubsystem.h"
#include "Components/TextBlock.h"

void UEFSettingsWidgetBase::OnTrackedSettingsChanged_Implementation(UEFModularSettingsBase* ChangedSetting)
{
	
}

void UEFSettingsWidgetBase::SettingsPreConstruct_Implementation()
{
	if (SettingsLabel)
	{
		FSlateFontInfo SettingsLabelFontInfo = SettingsLabel->GetFont();
		SettingsLabelFontInfo.Size = SettingsLabelFontSize;
		SettingsLabel->SetFont(FSlateFontInfo(SettingsLabelFontInfo));
		
		SettingsLabel->SetText(SettingsDisplayName);
		
	}
}

void UEFSettingsWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();
	
	SettingsPreConstruct();

	if (const auto EFModularSettingsSubsystem = GetWorld()->GetGameInstance()->GetSubsystem<UEFModularSettingsSubsystem>())
	{
		EFModularSettingsSubsystem->OnSettingsChanged.AddDynamic(this, &UEFSettingsWidgetBase::OnSettingChanged);
	}
}


void UEFSettingsWidgetBase::OnSettingChanged(UEFModularSettingsBase* ChangedSetting)
{
	if (ChangedSetting->SettingTag == SettingsTag)
	{
		OnTrackedSettingsChanged(ChangedSetting);
	}
}
