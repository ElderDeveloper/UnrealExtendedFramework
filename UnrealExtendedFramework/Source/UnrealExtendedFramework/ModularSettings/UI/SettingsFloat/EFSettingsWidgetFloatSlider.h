#pragma once
#include "CoreMinimal.h"
#include "UnrealExtendedFramework/ModularSettings/UI/EFSettingsWidgetBase.h"
#include "EFSettingsWidgetFloatSlider.generated.h"

/**
 *
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFSettingsWidgetFloatSlider : public UEFSettingsWidgetBase
{
	GENERATED_BODY()
public:

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Settings")
	class USlider* Slider;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, OptionalWidget = true), Category = "Settings")
	class UTextBlock* ValueText;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings")
	bool bRemoveValueFractions = false;
	
	UFUNCTION(BlueprintCallable, Category = "Settings")
	void SetValue(float NewValue);
	
	UFUNCTION(BlueprintCallable, Category = "Settings")
	void UpdateSliderBar();
	
	virtual void NativeConstruct() override;
	virtual void OnTrackedSettingsChanged_Implementation(UEFModularSettingsBase* ChangedSetting) override;
	virtual void SettingsPreConstruct_Implementation() override;

protected:

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Settings")
	float ValueTextFontSize = 13.5f;

	UFUNCTION()
	void OnValueChanged(float NewValue);
	
};