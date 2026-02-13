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
		// Slider her zaman 0-1 arasýnda çalýþýr
		Slider->SetMinValue(0.0f);
		Slider->SetMaxValue(1.0f);

		// Display step size'ý normalized step size'a çevir
		float NormalizedStep = CalculateNormalizedStepSize();
		Slider->SetStepSize(NormalizedStep);

		if (UEFModularSettingsFloat* FloatSetting = Cast<UEFModularSettingsFloat>(UEFModularSettingsLibrary::GetModularSetting(this, SettingsTag, SettingsSource)))
		{
			// Deðer zaten 0-1 arasý, direkt set et
			Slider->SetValue(FloatSetting->Value);

			if (ValueText)
			{
				// Gösterim için DisplayMin-DisplayMax arasýna map et
				float DisplayValue = NormalizedToDisplay(FloatSetting->Value);
				DisplayValue = RoundToStepSize(DisplayValue);
				ValueText->SetText(FText::AsNumber(DisplayValue));
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
			// Deðer zaten 0-1 arasý, direkt set et
			Slider->SetValue(FloatSetting->Value);

			if (ValueText)
			{
				// Gösterim için DisplayMin-DisplayMax arasýna map et
				float DisplayValue = NormalizedToDisplay(FloatSetting->Value);
				DisplayValue = RoundToStepSize(DisplayValue);
				ValueText->SetText(FText::AsNumber(DisplayValue));
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
	// NewValue 0-1 arasý (slider'dan gelen deðer)

	// Settings'e 0-1 arasý clamped deðer gönder
	float ClampedValue = FMath::Clamp(NewValue, 0.0f, 1.0f);
	UEFModularSettingsLibrary::SetModularFloat(this, SettingsTag, ClampedValue, SettingsSource);

	// Gösterim için DisplayMin-DisplayMax arasýna map et
	float DisplayValue = NormalizedToDisplay(NewValue);
	DisplayValue = RoundToStepSize(DisplayValue);

	if (ValueText)
	{
		ValueText->SetText(FText::AsNumber(DisplayValue));
	}
}

float UEFSettingsWidgetFloatSlider::NormalizedToDisplay(float NormalizedValue) const
{
	// 0-1 arasý deðeri DisplayMin-DisplayMax arasýna map et
	return FMath::Lerp(DisplayMinValue, DisplayMaxValue, NormalizedValue);
}

float UEFSettingsWidgetFloatSlider::CalculateNormalizedStepSize() const
{
	// Display range'i hesapla
	float DisplayRange = DisplayMaxValue - DisplayMinValue;

	// DisplayStepSize'ý normalized step size'a çevir
	if (FMath::IsNearlyZero(DisplayRange))
	{
		return 0.01f; // Varsayýlan
	}

	return DisplayStepSize / DisplayRange;
}

float UEFSettingsWidgetFloatSlider::RoundToStepSize(float Value) const
{
	// DisplayStepSize'a göre yuvarla
	if (FMath::IsNearlyZero(DisplayStepSize))
	{
		return Value;
	}

	return FMath::RoundToFloat(Value / DisplayStepSize) * DisplayStepSize;
}

float UEFSettingsWidgetFloatSlider::GetValue() const
{
	if (Slider)
	{
		return Slider->GetValue();
	}

	return 0.0f;
}

void UEFSettingsWidgetFloatSlider::SetValue(float NewValue)
{
	// 0-1 arasý clamp et
	float ClampedValue = FMath::Clamp(NewValue, 0.0f, 1.0f);

	if (Slider)
	{
		Slider->SetValue(ClampedValue);
	}

	// Settings'e kaydet
	UEFModularSettingsLibrary::SetModularFloat(this, SettingsTag, ClampedValue, SettingsSource);

	// Göstergeyi güncelle
	if (ValueText)
	{
		float DisplayValue = NormalizedToDisplay(ClampedValue);
		DisplayValue = RoundToStepSize(DisplayValue);
		ValueText->SetText(FText::AsNumber(DisplayValue));
	}
}