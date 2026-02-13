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
		// Ýlk deðerleri hem preview hem de backup olarak kaydet
		if (const auto BoolSetting = Cast<UEFModularSettingsBool>(Setting))
		{
			CurrentPreviewBool = BoolSetting->Value;
			OldBoolValue = BoolSetting->Value; // Backup
		}
		else if (const auto MultiSelectSetting = Cast<UEFModularSettingsMultiSelect>(Setting))
		{
			CurrentPreviewIndex = MultiSelectSetting->SelectedIndex;
			OldIndex = MultiSelectSetting->SelectedIndex; // Backup
		}

		UpdateText(Setting);
	}
}

void UEFSettingsWidgetStepper::OnTrackedSettingsChanged_Implementation(UEFModularSettingsBase* ChangedSetting)
{
	// Preview modundayken gerçek deðer güncellemelerini görmezden gel
	if (!bIsInPreviewMode)
	{
		UpdateText(ChangedSetting);
	}
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
			if (bApplyOnSwitch)
			{
				bIsInPreviewMode = false;
				// Apply etmeden önce backup al
				OldBoolValue = BoolSetting->Value;
				UEFModularSettingsLibrary::SetModularBool(this, SettingsTag, !BoolSetting->Value, SettingsSource);
			}
			else
			{
				bIsInPreviewMode = true;
				CurrentPreviewBool = !CurrentPreviewBool;
				ValueText->SetText(CurrentPreviewBool ? EnabledText : DisabledText);
			}
		}
		else if (const auto MultiSelectSetting = Cast<UEFModularSettingsMultiSelect>(Setting))
		{
			if (bApplyOnSwitch)
			{
				bIsInPreviewMode = false;
				// Apply etmeden önce backup al
				OldIndex = MultiSelectSetting->SelectedIndex;
				UEFModularSettingsLibrary::AdjustModularIndex(this, SettingsTag, -1, true, SettingsSource);
			}
			else
			{
				bIsInPreviewMode = true;
				int32 NumOptions = MultiSelectSetting->Values.Num();
				if (NumOptions > 0)
				{
					CurrentPreviewIndex--;
					if (CurrentPreviewIndex < 0) CurrentPreviewIndex = NumOptions - 1;

					if (MultiSelectSetting->DisplayNames.IsValidIndex(CurrentPreviewIndex))
					{
						ValueText->SetText(MultiSelectSetting->DisplayNames[CurrentPreviewIndex]);
					}
					else if (MultiSelectSetting->Values.IsValidIndex(CurrentPreviewIndex))
					{
						ValueText->SetText(FText::FromString(MultiSelectSetting->Values[CurrentPreviewIndex]));
					}
				}
			}
		}
	}
}

void UEFSettingsWidgetStepper::OnNextClicked()
{
	if (const auto Setting = UEFModularSettingsLibrary::GetModularSetting(this, SettingsTag, SettingsSource))
	{
		if (const auto BoolSetting = Cast<UEFModularSettingsBool>(Setting))
		{
			if (bApplyOnSwitch)
			{
				bIsInPreviewMode = false;
				// Apply etmeden önce backup al
				OldBoolValue = BoolSetting->Value;
				UEFModularSettingsLibrary::SetModularBool(this, SettingsTag, !BoolSetting->Value, SettingsSource);
			}
			else
			{
				bIsInPreviewMode = true;
				CurrentPreviewBool = !CurrentPreviewBool;
				ValueText->SetText(CurrentPreviewBool ? EnabledText : DisabledText);
			}
		}
		else if (const auto MultiSelectSetting = Cast<UEFModularSettingsMultiSelect>(Setting))
		{
			if (bApplyOnSwitch)
			{
				bIsInPreviewMode = false;
				// Apply etmeden önce backup al
				OldIndex = MultiSelectSetting->SelectedIndex;
				UEFModularSettingsLibrary::AdjustModularIndex(this, SettingsTag, 1, true, SettingsSource);
			}
			else
			{
				bIsInPreviewMode = true;
				int32 NumOptions = MultiSelectSetting->Values.Num();
				if (NumOptions > 0)
				{
					CurrentPreviewIndex++;
					if (CurrentPreviewIndex >= NumOptions) CurrentPreviewIndex = 0;

					if (MultiSelectSetting->DisplayNames.IsValidIndex(CurrentPreviewIndex))
					{
						ValueText->SetText(MultiSelectSetting->DisplayNames[CurrentPreviewIndex]);
					}
					else if (MultiSelectSetting->Values.IsValidIndex(CurrentPreviewIndex))
					{
						ValueText->SetText(FText::FromString(MultiSelectSetting->Values[CurrentPreviewIndex]));
					}
				}
			}
		}
	}
}

void UEFSettingsWidgetStepper::ApplyPreviewValues()
{
	if (!bIsInPreviewMode)
	{
		// Zaten preview modunda deðiliz, bir þey yapma
		return;
	}

	if (const auto Setting = UEFModularSettingsLibrary::GetModularSetting(this, SettingsTag, SettingsSource))
	{
		if (const auto BoolSetting = Cast<UEFModularSettingsBool>(Setting))
		{
			// ÞU ANKÝ deðeri backup'la (apply etmeden önce)
			OldBoolValue = BoolSetting->Value;

			// Yeni deðeri apply et
			UEFModularSettingsLibrary::SetModularBool(this, SettingsTag, CurrentPreviewBool, SettingsSource);
		}
		else if (const auto MultiSelectSetting = Cast<UEFModularSettingsMultiSelect>(Setting))
		{
			// ÞU ANKÝ index'i backup'la (apply etmeden önce)
			OldIndex = MultiSelectSetting->SelectedIndex;

			// Yeni index'i apply et
			UEFModularSettingsLibrary::SetModularSelectedIndex(this, SettingsTag, CurrentPreviewIndex, SettingsSource);
		}
	}

	// Preview modundan çýk
	bIsInPreviewMode = false;
}

void UEFSettingsWidgetStepper::RevertPreviewValues()
{
	if (const auto Setting = UEFModularSettingsLibrary::GetModularSetting(this, SettingsTag, SettingsSource))
	{
		if (const auto BoolSetting = Cast<UEFModularSettingsBool>(Setting))
		{
			// Eski deðere geri dön
			UEFModularSettingsLibrary::SetModularBool(this, SettingsTag, OldBoolValue, SettingsSource);
			CurrentPreviewBool = OldBoolValue;
		}
		else if (const auto MultiSelectSetting = Cast<UEFModularSettingsMultiSelect>(Setting))
		{
			// Eski index'e geri dön
			UEFModularSettingsLibrary::SetModularSelectedIndex(this, SettingsTag, OldIndex, SettingsSource);
			CurrentPreviewIndex = OldIndex;
		}
	}

	// Preview modundan çýk
	bIsInPreviewMode = false;
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