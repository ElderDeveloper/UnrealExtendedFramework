// EFSubtitleProjectSettings.h — Project-wide subtitle configuration (DeveloperSettings)
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "EFSubtitleData.h"
#include "EFSubtitleProjectSettings.generated.h"

class UEFSubtitleDisplayWidget;
class UEFSubtitleDataAsset;
class UEFSubtitleQueuePolicy;
class UEFSubtitleAudioPlayer;
class UEFSubtitleStyleProfile;

/**
 * Project-wide subtitle settings, editable in Project Settings > Extended Framework > Subtitle.
 */
UCLASS(Config=Game, defaultconfig, meta=(DisplayName="Extended Subtitle Settings"))
class UNREALEXTENDEDFRAMEWORK_API UEFSubtitleProjectSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UEFSubtitleProjectSettings();

	// -- Widget --

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Widget")
	TSubclassOf<UEFSubtitleDisplayWidget> SubtitleWidgetClass;

	// -- Data Sources --

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	TSoftObjectPtr<UDataTable> DefaultDataTable;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	TArray<TSoftObjectPtr<UEFSubtitleDataAsset>> DefaultDataAssets;

	// -- Behavior --

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behavior")
	EEFSubtitleQueueMode DefaultQueueMode = EEFSubtitleQueueMode::Replace;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behavior",
		meta=(EditCondition="DefaultQueueMode==EEFSubtitleQueueMode::Stack", ClampMin=1, ClampMax=10))
	int32 MaxStackedSubtitles = 3;

	/** Swappable queue policy. Defaults to UEFSubtitleQueuePolicy_Default. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behavior")
	TSubclassOf<UEFSubtitleQueuePolicy> QueuePolicyClass;

	/** Swappable audio player. Defaults to UEFSubtitleAudioPlayer_Default. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	TSubclassOf<UEFSubtitleAudioPlayer> AudioPlayerClass;

	// -- Appearance --

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

	/** Optional style profiles selectable via ModularSettings. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	TArray<TSoftObjectPtr<UEFSubtitleStyleProfile>> AvailableStyleProfiles;

	virtual FName GetCategoryName() const override { return TEXT("Extended Framework"); }
};
