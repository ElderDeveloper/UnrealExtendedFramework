// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "../Settings/EFModularSettingsBase.h"
#include "EFSettingsWidgetBase.generated.h"

class UTextBlock;
/**
 * Base class for all setting widgets.
 * Automatically binds to the Setting's OnSettingsChanged event.
 */
UCLASS(Abstract)
class UNREALEXTENDEDFRAMEWORK_API UEFSettingsWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Settings")
	UTextBlock* SettingsLabel;
	
	UFUNCTION(BlueprintNativeEvent, Category = "Settings")
	void OnTrackedSettingsChanged(UEFModularSettingsBase* ChangedSetting);
	
	UFUNCTION(BlueprintNativeEvent,BlueprintCallable, Category = "Settings")
	void SettingsPreConstruct();
	
	virtual void NativeConstruct() override;

protected:
	
	UPROPERTY(BlueprintReadOnly,EditAnywhere, Category = "Settings")
	FGameplayTag SettingsTag;
	
	UPROPERTY(BlueprintReadOnly,EditAnywhere, Category = "Settings", meta = (ExposeOnSpawn = "true"))
	FText SettingsDisplayName = FText::FromString("Settings Label");
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Settings")
	float SettingsLabelFontSize = 13.5f;
	
	UFUNCTION()
	void OnSettingChanged(UEFModularSettingsBase* ChangedSetting);
};
