// Fill out your copyright notice in the Description page of Project Settings.
#pragma once
#include "CoreMinimal.h"
#include "UnrealExtendedFramework/ModularSettings/UI/EFSettingsWidgetBase.h"
#include "EFSettingsWidgetStepper.generated.h"

/**
 *
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFSettingsWidgetStepper : public UEFSettingsWidgetBase
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

	// Yeni bool deðiþkeni - Blueprint'ten editlenebilir, default true
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	bool bApplyOnSwitch = true;

	UPROPERTY(BlueprintReadWrite, Category = "Settings")
	int32 CurrentPreviewIndex = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Settings")
	bool CurrentPreviewBool = false;

	UPROPERTY()
	bool bIsInPreviewMode = false;

	// Backup deðerleri - apply öncesi son deðerler
	UPROPERTY()
	int32 OldIndex;

	UPROPERTY()
	bool OldBoolValue;

	UFUNCTION(BlueprintCallable, Category = "Settings")
	int32 GetCurrentPreviewIndex() const { return CurrentPreviewIndex; }

	UFUNCTION(BlueprintCallable, Category = "Settings")
	bool GetCurrentPreviewBool() const { return CurrentPreviewBool; }

	UFUNCTION(BlueprintCallable, Category = "Settings")
	bool IsInPreviewMode() const { return bIsInPreviewMode; }

	// Preview deðerlerini apply et
	UFUNCTION(BlueprintCallable, Category = "Settings")
	void ApplyPreviewValues();

	UFUNCTION(BlueprintCallable, Category = "Settings")
	void RevertPreviewValues();

	virtual void NativeConstruct() override;
	virtual void OnTrackedSettingsChanged_Implementation(UEFModularSettingsBase* ChangedSetting) override;
	virtual void SettingsPreConstruct_Implementation() override;

protected:
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Settings")
	float ValueTextFontSize = 13.5f;

	UFUNCTION()
	void OnPreviousClicked();

	UFUNCTION()
	void OnNextClicked();

	void UpdateText(const UEFModularSettingsBase* Setting);
};