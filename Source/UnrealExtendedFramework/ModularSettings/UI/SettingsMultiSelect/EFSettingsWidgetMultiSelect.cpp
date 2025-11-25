// Fill out your copyright notice in the Description page of Project Settings.


#include "EFSettingsWidgetMultiSelect.h"
#include "Components/ComboBoxString.h"
#include "UnrealExtendedFramework/ModularSettings/EFModularSettingsSubsystem.h"
#include "UnrealExtendedFramework/ModularSettings/Settings/EFModularSettingsBase.h"

void UEFSettingsWidgetMultiSelect::NativeConstruct()
{
	Super::NativeConstruct();

	if (ComboBox)
	{
		if (const auto Subsystem = GetWorld()->GetGameInstance()->GetSubsystem<UEFModularSettingsSubsystem>())
		{
			ComboBox->ClearOptions();
			for (const FText& Option : Subsystem->GetOptions(SettingsTag))
			{
				ComboBox->AddOption(Option.ToString());
			}
			
			ComboBox->SetSelectedIndex(Subsystem->GetSelectedOptionIndex(SettingsTag));
		}
		
		ComboBox->OnSelectionChanged.AddDynamic(this, &UEFSettingsWidgetMultiSelect::OnSelectionChanged);
	}
}

void UEFSettingsWidgetMultiSelect::OnTrackedSettingsChanged_Implementation(UEFModularSettingsBase* ChangedSetting)
{
	if (ComboBox)
	{
		if (const auto MultiSelectSetting = Cast<UEFModularSettingsMultiSelect>(ChangedSetting))
		{
			// Refresh options if they changed? 
			// For now assuming options don't change at runtime, just selection.
			ComboBox->SetSelectedIndex(MultiSelectSetting->SelectedIndex);
		}
	}
}

void UEFSettingsWidgetMultiSelect::OnSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (const auto Subsystem = GetWorld()->GetGameInstance()->GetSubsystem<UEFModularSettingsSubsystem>())
	{
		Subsystem->SetIndex(SettingsTag, ComboBox->FindOptionIndex(SelectedItem));
	}
}
