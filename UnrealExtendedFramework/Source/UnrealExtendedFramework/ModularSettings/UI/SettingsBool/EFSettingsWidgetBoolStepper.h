// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UnrealExtendedFramework/ModularSettings/UI/EFSettingsWidgetBase.h"
#include "EFSettingsWidgetBoolStepper.generated.h"

/**
 * 
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFSettingsWidgetBoolStepper : public UEFSettingsWidgetBase
{
	GENERATED_BODY()

public:
	
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Settings")
	class UButton* PreviousButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Settings")
	class UButton* NextButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Settings")
	class UTextBlock* ValueText;
	
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FText EnabledText = FText::FromString("Enabled");
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FText DisabledText = FText::FromString("Disabled");

	virtual void NativeConstruct() override;
	virtual void OnTrackedSettingsChanged_Implementation(UEFModularSettingsBase* ChangedSetting) override;
	virtual void SettingsPreConstruct_Implementation() override;

protected:
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Settings")
	float ValueTextFontSize = 13.5f;
	
	UFUNCTION()
	void OnButtonClicked();

	void UpdateText(bool bValue);
};
