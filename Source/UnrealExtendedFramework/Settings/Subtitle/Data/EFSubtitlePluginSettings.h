// Fill out your copyright notice in the Description page of Project Settings.
#pragma once

#include "CoreMinimal.h"
#include "UnrealExtendedFramework/Settings/Subtitle/Data/EFSubtitleData.h"
#include "UnrealExtendedFramework/Settings/Subtitle/Widget/EFSubtitleWidget.h"
#include "EFSubtitlePluginSettings.generated.h"


class UExtendedSubtitleWidget;
struct FGameplayTag;

UCLASS(Config=Game, defaultconfig, meta=(DisplayName="Extended Subtitle Settings"))
class UNREALEXTENDEDFRAMEWORK_API UEFSubtitleSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UEFSubtitleSettings();

	UPROPERTY(EditAnywhere , BlueprintReadOnly, Category = "Subtitle Widget")
	TSubclassOf<UEFSubtitleWidget> SubtitleWidgetClass = UEFSubtitleWidget::StaticClass();

	UPROPERTY(EditAnywhere , BlueprintReadOnly, Category = "Subtitle Widget")
	TSoftObjectPtr<UDataTable> SubtitleDataTable;

	UPROPERTY(EditAnywhere , BlueprintReadOnly, Category = "Subtitle Appearance")
	FSlateFontInfo Font;

	UPROPERTY(EditAnywhere , BlueprintReadOnly, Category = "Subtitle Appearance")
	FLinearColor FontColor;
	
	UPROPERTY(EditAnywhere , BlueprintReadOnly, Category = "Subtitle Appearance")
	FVector2D ShadowOffset;

	UPROPERTY(EditAnywhere , BlueprintReadOnly, Category = "Subtitle Appearance")
	FLinearColor ShadowColor;
	
	UPROPERTY(EditAnywhere ,config, BlueprintReadOnly, Category = "Subtitle Appearance")
	bool bUseBorder = false;

	UPROPERTY(EditAnywhere , BlueprintReadOnly, meta=(EditCondition="bUseBorder", EditConditionHides), Category = "Subtitle Appearance")
	FExtendedSubtitleBorderSettings BorderSettings;
	
	UPROPERTY(EditAnywhere ,config, BlueprintReadOnly, Category = "Subtitle Appearance")
	bool bUseBackground = false;

	UPROPERTY(EditAnywhere ,config, BlueprintReadOnly, meta=(EditCondition="bUseBackground", EditConditionHides), Category = "Subtitle Appearance")
	FExtendedSubtitleBackgroundSettings BackgroundSettings;
	
	UPROPERTY(EditAnywhere ,config, BlueprintReadOnly, Category = "Subtitle Appearance")
	FExtendedSubtitleDurationSettings DurationSettings;

};

