// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UnrealExtendedFramework/ModularSettings/UI/EFSettingsWidgetBase.h"
#include "EFSettingsWidgetMultiSelect.generated.h"

/**
 * 
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFSettingsWidgetMultiSelect : public UEFSettingsWidgetBase
{
	GENERATED_BODY()

public:
	
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Settings")
	class UComboBoxString* ComboBox;

	virtual void NativeConstruct() override;
	virtual void OnTrackedSettingsChanged_Implementation(UEFModularSettingsBase* ChangedSetting) override;

protected:
	void RefreshOptions();

	UFUNCTION()
	void OnSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
};
