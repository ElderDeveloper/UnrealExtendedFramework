// Fill out your copyright notice in the Description page of Project Settings.


#include "EFSettingsWidgetBool.h"
#include "Components/CheckBox.h"
#include "UnrealExtendedFramework/ModularSettings/EFModularSettingsLibrary.h"
#include "UnrealExtendedFramework/ModularSettings/EFModularSettingsSubsystem.h"
#include "UnrealExtendedFramework/ModularSettings/Settings/EFModularSettingsBase.h"

void UEFSettingsWidgetBool::NativeConstruct()
{
	Super::NativeConstruct();

	if (CheckBox)
	{
		CheckBox->SetIsChecked(UEFModularSettingsLibrary::GetModularBool(this, SettingsTag, SettingsSource));
		CheckBox->OnCheckStateChanged.AddDynamic(this, &UEFSettingsWidgetBool::OnCheckStateChanged);
	}
}

void UEFSettingsWidgetBool::OnTrackedSettingsChanged_Implementation(UEFModularSettingsBase* ChangedSetting)
{
	if (CheckBox)
	{
		if (const auto BoolSetting = Cast<UEFModularSettingsBool>(ChangedSetting))
		{
			CheckBox->SetIsChecked(BoolSetting->Value);
		}
	}
}

void UEFSettingsWidgetBool::OnCheckStateChanged(bool bIsChecked)
{
	UEFModularSettingsLibrary::SetModularBool(this, SettingsTag, bIsChecked, SettingsSource);
}
