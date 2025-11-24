// Fill out your copyright notice in the Description page of Project Settings.

#include "EFSettingWidgetFactory.h"
#include "../EFModularSettingsSubsystem.h"
#include "Kismet/GameplayStatics.h"

UEFSettingWidgetBase* UEFSettingWidgetFactory::CreateSettingWidget(UObject* WorldContextObject, UEFModularSettingsBase* Setting, TSubclassOf<UEFSettingWidgetBase> WidgetClass)
{
	if (!WorldContextObject || !Setting || !WidgetClass) return nullptr;

	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		UEFSettingWidgetBase* Widget = CreateWidget<UEFSettingWidgetBase>(World, WidgetClass);
		if (Widget)
		{
			Widget->InitializeSetting(Setting);
			return Widget;
		}
	}
	return nullptr;
}

void UEFSettingWidgetFactory::GenerateSettingsWidgets(UObject* WorldContextObject, UPanelWidget* Container, FName Category, TMap<TSubclassOf<UEFModularSettingsBase>, TSubclassOf<UEFSettingWidgetBase>> WidgetClasses)
{
	if (!WorldContextObject || !Container) return;

	UGameInstance* GI = UGameplayStatics::GetGameInstance(WorldContextObject);
	if (!GI) return;

	UEFModularSettingsSubsystem* SettingsSubsystem = GI->GetSubsystem<UEFModularSettingsSubsystem>();
	if (!SettingsSubsystem) return;

	TArray<UEFModularSettingsBase*> Settings = SettingsSubsystem->GetSettingsByCategory(Category);
	
	for (UEFModularSettingsBase* Setting : Settings)
	{
		if (!Setting) continue;

		// Find best matching widget class
		TSubclassOf<UEFSettingWidgetBase> BestWidgetClass = nullptr;
		
		// Check exact match first, then parent classes
		for (const auto& Pair : WidgetClasses)
		{
			if (Setting->IsA(Pair.Key))
			{
				BestWidgetClass = Pair.Value;
				// We break on first match. Ensure map is populated with specific types.
				break; 
			}
		}

		if (BestWidgetClass)
		{
			UEFSettingWidgetBase* NewWidget = CreateSettingWidget(WorldContextObject, Setting, BestWidgetClass);
			if (NewWidget)
			{
				Container->AddChild(NewWidget);
			}
		}
	}
}
