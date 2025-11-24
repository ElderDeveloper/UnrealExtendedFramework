// Fill out your copyright notice in the Description page of Project Settings.

#include "EFSettingWidgetBase.h"
#include "../EFModularSettingsSubsystem.h"

void UEFSettingWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();

	if (Setting)
	{
		InitializeSetting(Setting);
	}
}

void UEFSettingWidgetBase::InitializeSetting(UEFModularSettingsBase* InSetting)
{
	if (!InSetting) return;

	Setting = InSetting;

	// Bind to subsystem change event to catch external changes
	if (Setting->ModularSettingsSubsystem)
	{
		Setting->ModularSettingsSubsystem->OnSettingsChanged.AddUniqueDynamic(this, &UEFSettingWidgetBase::OnSettingChanged);
	}

	UpdateView();
}

void UEFSettingWidgetBase::UpdateView_Implementation()
{
	// Override in Blueprint to update UI elements (Checkbox, Slider, etc.)
}

void UEFSettingWidgetBase::OnSettingChanged(UEFModularSettingsBase* ChangedSetting)
{
	if (ChangedSetting == Setting)
	{
		UpdateView();
	}
}
