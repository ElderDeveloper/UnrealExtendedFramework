// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/TextBlock.h"
#include "UnrealExtendedSettings/Data/ESData.h"
#include "ESText.generated.h"

/**
 * 
 */
UCLASS()
class UNREALEXTENDEDSETTINGS_API UESText : public UTextBlock
{
	GENERATED_BODY()

	UESText();
public:

	UPROPERTY(EditAnywhere)
	TEnumAsByte<EExtendedSettingsFloat> SettingsFloat;
	
	virtual void PostLoad() override;

	UFUNCTION()
	void OnSettingsUpdated(TEnumAsByte<EExtendedSettingsFloat> SettingsType , float Value);
};
