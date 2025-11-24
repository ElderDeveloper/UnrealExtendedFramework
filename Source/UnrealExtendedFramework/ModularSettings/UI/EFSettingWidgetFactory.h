// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EFSettingWidgetBase.h"
#include "Components/PanelWidget.h"
#include "EFSettingWidgetFactory.generated.h"

/**
 * Factory library to generate UI for settings automatically.
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFSettingWidgetFactory : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Creates a widget for the given setting.
	 */
	UFUNCTION(BlueprintCallable, Category = "Settings|UI", meta = (WorldContext = "WorldContextObject"))
	static UEFSettingWidgetBase* CreateSettingWidget(UObject* WorldContextObject, UEFModularSettingsBase* Setting, TSubclassOf<UEFSettingWidgetBase> WidgetClass);

	/**
	 * Generates widgets for all settings in a category and adds them to the container.
	 * WidgetClasses map: Key = Setting Class (e.g. UEFModularSettingsBool), Value = Widget Class (e.g. WBP_SettingBool)
	 */
	UFUNCTION(BlueprintCallable, Category = "Settings|UI", meta = (WorldContext = "WorldContextObject"))
	static void GenerateSettingsWidgets(UObject* WorldContextObject, UPanelWidget* Container, FName Category, TMap<TSubclassOf<UEFModularSettingsBase>, TSubclassOf<UEFSettingWidgetBase>> WidgetClasses);
};
