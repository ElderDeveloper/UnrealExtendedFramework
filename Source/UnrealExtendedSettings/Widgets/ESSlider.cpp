// Fill out your copyright notice in the Description page of Project Settings.


#include "ESSlider.h"

#include "UnrealExtendedSettings/Subsystems/ESPlayerSubsystem.h"


void UESSlider::PostLoad()
{
	Super::PostLoad();
	OnValueChanged.AddDynamic(this,&UESSlider::OnSettingsSliderUpdate);
}

void UESSlider::OnSettingsSliderUpdate(float NewValue)
{
	if (GetWorld())
	{
		if (const auto Subsystem = GetWorld()->GetGameInstance()->GetSubsystem<UESPlayerSubsystem>())
			Subsystem->UpdateSettingsFloatValue(SettingsSliderType , NewValue);
	}
}
