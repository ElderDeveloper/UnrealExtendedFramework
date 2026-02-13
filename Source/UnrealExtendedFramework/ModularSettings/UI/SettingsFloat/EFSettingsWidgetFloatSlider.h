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

	// Blueprint'ten direkt deðer set etmek için (0-1 arasý normalized deðer)
	UFUNCTION(BlueprintCallable, Category = "Settings")
	void SetValue(float NewValue);

	// Slider'ýn mevcut deðerini al (0-1 arasý normalized deðer)
	UFUNCTION(BlueprintPure, Category = "Settings")
	float GetValue() const;

	// Oyuncuya gösterilecek min/max deðerler (arkaplanda hep 0-1 arasý çalýþýr)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	float DisplayMinValue = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	float DisplayMaxValue = 1.0f;

	// Slider'ýn ilerleme adýmý (Display deðerler üzerinden: 1.0 = 1'er adým, 0.1 = 0.1'lik adýmlar)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (ClampMin = "0.1"))
	float DisplayStepSize = 0.1f;

	virtual void NativeConstruct() override;
	virtual void OnTrackedSettingsChanged_Implementation(UEFModularSettingsBase* ChangedSetting) override;
	virtual void SettingsPreConstruct_Implementation() override;

protected:

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Settings")
	float ValueTextFontSize = 13.5f;

	UFUNCTION()
	void OnValueChanged(float NewValue);

private:
	// 0-1 arasý deðeri görsel deðere (DisplayMin-DisplayMax) dönüþtürür
	float NormalizedToDisplay(float NormalizedValue) const;

	// Display step size'ý normalized step size'a çevirir
	float CalculateNormalizedStepSize() const;

	// Float deðeri DisplayStepSize'a göre yuvarlar
	float RoundToStepSize(float Value) const;
};