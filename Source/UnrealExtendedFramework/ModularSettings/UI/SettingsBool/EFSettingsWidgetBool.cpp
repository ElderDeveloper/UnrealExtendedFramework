// Fill out your copyright notice in the Description page of Project Settings.


#include "EFSettingsWidgetBool.h"
#include "Components/CheckBox.h"
#include "UnrealExtendedFramework/ModularSettings/EFModularSettingsSubsystem.h"
#include "UnrealExtendedFramework/ModularSettings/Settings/EFModularSettingsBase.h"

void UEFSettingsWidgetBool::NativeConstruct()
{
	Super::NativeConstruct();

	if (CheckBox)
	{
		if (const auto Subsystem = GetWorld()->GetGameInstance()->GetSubsystem<UEFModularSettingsSubsystem>())
		{
			CheckBox->SetIsChecked(Subsystem->GetBool(SettingsTag));
		}
		
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
	if (const auto Subsystem = GetWorld()->GetGameInstance()->GetSubsystem<UEFModularSettingsSubsystem>())
	{
		Subsystem->SetBool(SettingsTag, bIsChecked);
	}
}
