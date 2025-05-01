// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EFSettingsWidget_Multiselect.generated.h"

class UTextBlock;
class UButton;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMultiselectOptionChanged, int32, SelectedIndex, FText, SelectedText);

UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFSettingsWidget_Multiselect : public UUserWidget
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintAssignable, Category = "Settings")
	FOnMultiselectOptionChanged OnMultiselectOptionChanged;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FText OptionText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FText SelectedOptionText;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	TArray<FText> Options;

	

	UPROPERTY(EditAnywhere, BlueprintReadWrite,meta=(BindWidget), Category = "Settings|Buttons")
	UButton* RootButton;

	UPROPERTY(EditAnywhere, BlueprintReadWrite,meta=(BindWidget), Category = "Settings|Components")
	UTextBlock* SettingsText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite,meta=(BindWidget), Category = "Settings|Components")
	UTextBlock* SelectedOption;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintReadWrite,meta=(BindWidget), Category = "Settings|Buttons")
	UButton* ButtonNextOption;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintReadWrite,meta=(BindWidget), Category = "Settings|Buttons")
	UButton* ButtonPreviousOption;
	
	UFUNCTION(BlueprintCallable)
	void UpdateWidgetVisuals();

	UFUNCTION(BlueprintCallable)
	void SetSelectedOption(FText Text);

	UFUNCTION(BlueprintCallable)
	void SetSelectedOptionIndex(int32 Index);
	
protected:
	
	int32 SelectedOptionIndex;

	UFUNCTION()
	void OnRootButtonClicked();
	
	UFUNCTION()
	void OnRootButtonHovered();
	
	UFUNCTION()
	void OnRootButtonUnhovered();
	
	UFUNCTION()
	void OnNextOptionClicked();
	
	UFUNCTION()
	void OnPreviousOptionClicked();

	
	virtual void NativeConstruct() override;
	
};
