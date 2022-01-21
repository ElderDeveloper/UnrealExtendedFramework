// Fill out your copyright notice in the Description page of Project Settings.


#include "ESText.h"

#include "UnrealExtendedSettings/Subsystems/ESPlayerSubsystem.h"

UESText::UESText()
{
}

void UESText::PostLoad()
{
	Super::PostLoad();

	if (GetWorld())
	{
		if (const auto Subsystem = GetWorld()->GetGameInstance()->GetSubsystem<UESPlayerSubsystem>())
			Subsystem->FloatSettingsUpdate.AddDynamic(this,&UESText::OnSettingsUpdated);
	}
}

void UESText::OnSettingsUpdated(TEnumAsByte<EExtendedSettingsFloat> SettingsType, float Value)
{
	if (SettingsType == SettingsFloat)
	{
		SetText(FText::FromString(FString::SanitizeFloat(Value)));
	}
}
