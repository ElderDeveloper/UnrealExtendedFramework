// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UnrealExtendedFramework/ModularSettings/UI/EFSettingsWidgetBase.h"
#include "EFSettingsWidgetBool.generated.h"

/**
 * 
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFSettingsWidgetBool : public UEFSettingsWidgetBase
{
	GENERATED_BODY()

public:
	
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Settings")
	class UCheckBox* CheckBox;

	virtual void NativeConstruct() override;
	virtual void OnTrackedSettingsChanged_Implementation(UEFModularSettingsBase* ChangedSetting) override;

protected:
	UFUNCTION()
	void OnCheckStateChanged(bool bIsChecked);
};
