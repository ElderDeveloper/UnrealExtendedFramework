// Fill out your copyright notice in the Description page of Project Settings.


#include "EFSettingsWidgetBoolStepper.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "UnrealExtendedFramework/ModularSettings/EFModularSettingsSubsystem.h"
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

	if (const auto Subsystem = GetWorld()->GetGameInstance()->GetSubsystem<UEFModularSettingsSubsystem>())
	{
		UpdateText(Subsystem->GetBool(SettingsTag));
	}
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
	if (const auto Subsystem = GetWorld()->GetGameInstance()->GetSubsystem<UEFModularSettingsSubsystem>())
	{
		// Toggle value
		bool bCurrentValue = Subsystem->GetBool(SettingsTag);
		Subsystem->SetBool(SettingsTag, !bCurrentValue);
	}
}

void UEFSettingsWidgetBoolStepper::UpdateText(bool bValue)
{
	if (ValueText)
	{
		ValueText->SetText(bValue ? FText::FromString(TEXT("Enabled")) : FText::FromString(TEXT("Disabled")));
	}
}
