// Fill out your copyright notice in the Description page of Project Settings.


#include "EFSettingsWidgetBoolStepper.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "UnrealExtendedFramework/ModularSettings/EFModularSettingsLibrary.h"
#include "UnrealExtendedFramework/ModularSettings/Settings/EFModularSettingsBase.h"

void UEFSettingsWidgetBoolStepper::NativeConstruct()
{
	Super::NativeConstruct();

	if (PreviousButton)
	{
		PreviousButton->OnClicked.AddDynamic(this, &UEFSettingsWidgetBoolStepper::OnButtonClicked);
	}

	if (NextButton)
	{
		NextButton->OnClicked.AddDynamic(this, &UEFSettingsWidgetBoolStepper::OnButtonClicked);
	}

	UpdateText(UEFModularSettingsLibrary::GetModularBool(this, SettingsTag, SettingsSource));
}

void UEFSettingsWidgetBoolStepper::OnTrackedSettingsChanged_Implementation(UEFModularSettingsBase* ChangedSetting)
{
	if (const auto BoolSetting = Cast<UEFModularSettingsBool>(ChangedSetting))
	{
		UpdateText(BoolSetting->Value);
	}
}

void UEFSettingsWidgetBoolStepper::SettingsPreConstruct_Implementation()
{
	Super::SettingsPreConstruct_Implementation();
	
	if (ValueText)
	{
		FSlateFontInfo ValueTextFontInfo = ValueText->GetFont();
		ValueTextFontInfo.Size = ValueTextFontSize;
		ValueText->SetFont(FSlateFontInfo(ValueTextFontInfo));

		ValueText->SetText(FText::FromString("Disabled"));
	}
}

void UEFSettingsWidgetBoolStepper::OnButtonClicked()
{
	// Toggle value
	bool bCurrentValue = UEFModularSettingsLibrary::GetModularBool(this, SettingsTag, SettingsSource);
	UEFModularSettingsLibrary::SetModularBool(this, SettingsTag, !bCurrentValue, SettingsSource);
}

void UEFSettingsWidgetBoolStepper::UpdateText(bool bValue)
{
	if (ValueText)
	{
		ValueText->SetText(bValue ? FText::FromString(TEXT("Enabled")) : FText::FromString(TEXT("Disabled")));
	}
}
