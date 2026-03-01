// Fill out your copyright notice in the Description page of Project Settings.


#include "EFSettingsWidgetMultiSelect.h"
#include "Components/ComboBoxString.h"
#include "UnrealExtendedFramework/ModularSettings/EFModularSettingsLibrary.h"
#include "UnrealExtendedFramework/ModularSettings/Settings/EFModularSettingsBase.h"

void UEFSettingsWidgetMultiSelect::NativeConstruct()
{
	Super::NativeConstruct();

	if (ComboBox)
	{
		RefreshOptions();
		ComboBox->OnSelectionChanged.AddDynamic(this, &UEFSettingsWidgetMultiSelect::OnSelectionChanged);
	}
}

void UEFSettingsWidgetMultiSelect::RefreshOptions()
{
	if (!ComboBox) return;

	ComboBox->ClearOptions();
	TArray<FText> Options = UEFModularSettingsLibrary::GetModularOptions(this, SettingsTag, SettingsSource);
	
	for (int32 i = 0; i < Options.Num(); ++i)
	{
		FString OptionStr = Options[i].ToString();
		if (UEFModularSettingsLibrary::IsModularOptionLocked(this, SettingsTag, i, SettingsSource))
		{
			OptionStr += TEXT(" (Locked)");
		}
		ComboBox->AddOption(OptionStr);
	}
	
	ComboBox->SetSelectedIndex(UEFModularSettingsLibrary::GetModularSelectedIndex(this, SettingsTag, SettingsSource));
}

void UEFSettingsWidgetMultiSelect::OnTrackedSettingsChanged_Implementation(UEFModularSettingsBase* ChangedSetting)
{
	if (ComboBox)
	{
		if (const auto MultiSelectSetting = Cast<UEFModularSettingsMultiSelect>(ChangedSetting))
		{
			// Always refresh options to catch lock state changes
			RefreshOptions();
		}
	}
}

void UEFSettingsWidgetMultiSelect::OnSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	UEFModularSettingsLibrary::SetModularSelectedIndex(this, SettingsTag, ComboBox->FindOptionIndex(SelectedItem), SettingsSource, nullptr, ConfirmationType == EEFSettingConfirmationType::Instant);
}
