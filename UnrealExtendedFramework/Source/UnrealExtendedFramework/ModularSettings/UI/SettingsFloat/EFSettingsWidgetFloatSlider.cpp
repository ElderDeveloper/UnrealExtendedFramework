#include "EFSettingsWidgetFloatSlider.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "UnrealExtendedFramework/ModularSettings/EFModularSettingsLibrary.h"
#include "UnrealExtendedFramework/ModularSettings/Settings/EFModularSettingsBase.h"


void UEFSettingsWidgetFloatSlider::NativeConstruct()
{
	Super::NativeConstruct();

	if (Slider)
	{
		if (UEFModularSettingsFloat* FloatSetting = Cast<UEFModularSettingsFloat>(UEFModularSettingsLibrary::GetModularSetting(this, SettingsTag, SettingsSource)))
		{
			Slider->SetMinValue(FloatSetting->DisplayMin);
			Slider->SetMaxValue(FloatSetting->DisplayMax);
		}
		Slider->OnValueChanged.AddDynamic(this, &UEFSettingsWidgetFloatSlider::OnValueChanged);
	}
	
	UpdateSliderBar();
}


void UEFSettingsWidgetFloatSlider::OnTrackedSettingsChanged_Implementation(UEFModularSettingsBase* ChangedSetting)
{
	UpdateSliderBar();
}


void UEFSettingsWidgetFloatSlider::SettingsPreConstruct_Implementation()
{
	Super::SettingsPreConstruct_Implementation();

	if (ValueText)
	{
		FSlateFontInfo ValueTextFontInfo = ValueText->GetFont();
		ValueTextFontInfo.Size = ValueTextFontSize;
		ValueText->SetFont(FSlateFontInfo(ValueTextFontInfo));
	}
}


void UEFSettingsWidgetFloatSlider::OnValueChanged(float NewValue)
{
	SetValue(NewValue);
	UpdateSliderBar();
}


void UEFSettingsWidgetFloatSlider::SetValue(float NewValue)
{
	if (UEFModularSettingsFloat* FloatSetting = Cast<UEFModularSettingsFloat>(UEFModularSettingsLibrary::GetModularSetting(this, SettingsTag, SettingsSource)))
	{
		float TargetValue = FMath::GetMappedRangeValueClamped(FVector2D(FloatSetting->DisplayMin, FloatSetting->DisplayMax), FVector2D(FloatSetting->Min, FloatSetting->Max), NewValue);
		UEFModularSettingsLibrary::SetModularFloat(this, SettingsTag, TargetValue, SettingsSource, nullptr, ConfirmationType == EEFSettingConfirmationType::Instant);
	}
}


void UEFSettingsWidgetFloatSlider::UpdateSliderBar()
{
	if (Slider)
	{
		if (UEFModularSettingsFloat* FloatSetting = Cast<UEFModularSettingsFloat>(UEFModularSettingsLibrary::GetModularSetting(this, SettingsTag, SettingsSource)))
		{
			Slider->SetValue(FloatSetting->GetDisplayValue());
			Slider->SetMinValue(FloatSetting->DisplayMin);
			Slider->SetMaxValue(FloatSetting->DisplayMax);

			if (ValueText)
			{
				const float DisplayValue = FloatSetting->GetDisplayValue();

				if (bRemoveValueFractions)
				{
					ValueText->SetText(FText::AsNumber(FMath::RoundToInt(DisplayValue)));
				}
				else
				{
					FNumberFormattingOptions OneFractionFormat;
					OneFractionFormat.SetMinimumFractionalDigits(1);
					OneFractionFormat.SetMaximumFractionalDigits(1);

					ValueText->SetText(FText::AsNumber(DisplayValue, &OneFractionFormat));
				}
			}
		}
	}
}
