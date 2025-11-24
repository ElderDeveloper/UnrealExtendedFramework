// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "../Settings/EFModularSettingsBase.h"
#include "EFSettingWidgetBase.generated.h"

/**
 * Base class for all setting widgets.
 * Automatically binds to the Setting's OnSettingsChanged event.
 */
UCLASS(Abstract)
class UNREALEXTENDEDFRAMEWORK_API UEFSettingWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Settings", meta = (ExposeOnSpawn = "true"))
	TObjectPtr<UEFModularSettingsBase> Setting;

	virtual void NativeConstruct() override;

	UFUNCTION(BlueprintCallable, Category = "Settings")
	void InitializeSetting(UEFModularSettingsBase* InSetting);

	// Called when the setting value changes (externally or internally)
	UFUNCTION(BlueprintNativeEvent, Category = "Settings")
	void UpdateView(); 
	virtual void UpdateView_Implementation();

protected:
	UFUNCTION()
	void OnSettingChanged(UEFModularSettingsBase* ChangedSetting);
};
