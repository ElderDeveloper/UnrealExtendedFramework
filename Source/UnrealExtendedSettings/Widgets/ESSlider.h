// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/Slider.h"
#include "UnrealExtendedSettings/Data/ESData.h"
#include "ESSlider.generated.h"

/**
 * 
 */
UCLASS()
class UNREALEXTENDEDSETTINGS_API UESSlider : public USlider
{
	GENERATED_BODY()

public:
	
	UPROPERTY(EditAnywhere)
	TEnumAsByte<EExtendedSettingsFloat> SettingsSliderType;
	
	virtual void PostLoad() override;
	UFUNCTION()
	void OnSettingsSliderUpdate(float NewValue);
};
