// EFSubtitleProjectSettings.h — Project-wide subtitle configuration (DeveloperSettings)
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "EFSubtitleData.h"
#include "EFSubtitleProjectSettings.generated.h"

class UEFSubtitleDisplayWidget;
class UEFSubtitleDataAsset;

/**
 * Project-wide subtitle settings, editable in Project Settings > Extended Framework > Subtitle.
 * Replaces the old UEFSubtitleSettings.
 */
UCLASS(Config=Game, defaultconfig, meta=(DisplayName="EF Subtitle Settings"))
class UNREALEXTENDEDFRAMEWORK_API UEFSubtitleProjectSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UEFSubtitleProjectSettings();

	// ── Widget ──

	// Widget class to instantiate for subtitle display. Must be a subclass of UEFSubtitleDisplayWidget.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Widget")
	TSubclassOf<UEFSubtitleDisplayWidget> SubtitleWidgetClass;

	// ── Data Sources ──

	// Default DataTable containing FEFSubtitleEntry rows
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	TSoftObjectPtr<UDataTable> DefaultDataTable;

	// Additional DataAssets to auto-register as subtitle sources
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	TArray<TSoftObjectPtr<UEFSubtitleDataAsset>> DefaultDataAssets;

	// ── Behavior ──

	// Default queue mode for subtitle display
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behavior")
	EEFSubtitleQueueMode DefaultQueueMode = EEFSubtitleQueueMode::Replace;

	// Max simultaneous subtitles when using Stack mode
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behavior",
		meta=(EditCondition="DefaultQueueMode==EEFSubtitleQueueMode::Stack", ClampMin=1, ClampMax=10))
	int32 MaxStackedSubtitles = 3;

	// ── Appearance ──

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	FSlateFontInfo DefaultFont;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	FLinearColor DefaultFontColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	FVector2D ShadowOffset = FVector2D(1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	FLinearColor ShadowColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.5f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	bool bUseBorder = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance",
		meta=(EditCondition="bUseBorder", EditConditionHides))
	FEFSubtitleBorderSettings BorderSettings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	bool bUseBackground = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance",
		meta=(EditCondition="bUseBackground", EditConditionHides))
	FEFSubtitleBackgroundSettings BackgroundSettings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	FEFSubtitleDurationSettings DurationSettings;

	// ── Settings Category ──
	virtual FName GetCategoryName() const override { return TEXT("Extended Framework"); }
};
