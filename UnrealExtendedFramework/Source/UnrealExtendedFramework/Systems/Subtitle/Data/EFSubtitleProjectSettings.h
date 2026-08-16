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

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Widget")
	TSubclassOf<UEFSubtitleDisplayWidget> SubtitleWidgetClass;

	/**
	 * When disabled, a game-owned HUD must supply the display widget through the
	 * local subtitle subsystem. This prevents a subsystem from silently adding UI
	 * to the viewport behind the game's widget owner.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Widget")
	bool bAutoCreateDisplayWidget = true;

	// -- Data Sources --

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Data")
	TSoftObjectPtr<UDataTable> DefaultDataTable;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Data")
	TArray<TSoftObjectPtr<UEFSubtitleDataAsset>> DefaultDataAssets;

	// -- Behavior --

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Behavior")
	EEFSubtitleQueueMode DefaultQueueMode = EEFSubtitleQueueMode::Replace;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Behavior",
		meta=(EditCondition="DefaultQueueMode==EEFSubtitleQueueMode::Stack", ClampMin=1, ClampMax=10))
	int32 MaxStackedSubtitles = 3;

	/** Swappable queue policy. Defaults to UEFSubtitleQueuePolicy_Default. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Behavior")
	TSubclassOf<UEFSubtitleQueuePolicy> QueuePolicyClass;

	/** Swappable audio player. Defaults to UEFSubtitleAudioPlayer_Default. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Audio")
	TSubclassOf<UEFSubtitleAudioPlayer> AudioPlayerClass;

	// -- Appearance --

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	FSlateFontInfo DefaultFont;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	FLinearColor DefaultFontColor = FLinearColor::White;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	FVector2D ShadowOffset = FVector2D(1.0f, 1.0f);

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	FLinearColor ShadowColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.5f);

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	bool bUseBorder = false;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Appearance",
		meta=(EditCondition="bUseBorder", EditConditionHides))
	FEFSubtitleBorderSettings BorderSettings;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	bool bUseBackground = true;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Appearance",
		meta=(EditCondition="bUseBackground", EditConditionHides))
	FEFSubtitleBackgroundSettings BackgroundSettings;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	FEFSubtitleDurationSettings DurationSettings;

	/** Optional style profiles selectable via ModularSettings. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	TArray<TSoftObjectPtr<UEFSubtitleStyleProfile>> AvailableStyleProfiles;

	virtual FName GetCategoryName() const override { return TEXT("Extended Framework"); }
};
