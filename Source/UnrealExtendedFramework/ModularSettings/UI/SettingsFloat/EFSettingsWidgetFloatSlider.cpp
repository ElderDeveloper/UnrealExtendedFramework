// Fill out your copyright notice in the Description page of Project Settings.


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
			Slider->SetMinValue(FloatSetting->Min);
			Slider->SetMaxValue(FloatSetting->Max);
			Slider->SetValue(FloatSetting->Value);
			
			if (ValueText)
			{
				ValueText->SetText(FText::AsNumber(FloatSetting->Value));
			}
		}
		
		Slider->OnValueChanged.AddDynamic(this, &UEFSettingsWidgetFloatSlider::OnValueChanged);
	}
}

void UEFSettingsWidgetFloatSlider::OnTrackedSettingsChanged_Implementation(UEFModularSettingsBase* ChangedSetting)
{
	if (Slider)
	{
		if (const auto FloatSetting = Cast<UEFModularSettingsFloat>(ChangedSetting))
		{
			Slider->SetValue(FloatSetting->Value);
			
			if (ValueText)
			{
				ValueText->SetText(FText::AsNumber(FloatSetting->Value));
			}
		}
	}
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
	UEFModularSettingsLibrary::SetModularFloat(this, SettingsTag, NewValue, SettingsSource);
	
	if (ValueText)
	{
		ValueText->SetText(FText::AsNumber(NewValue));
	}
}
