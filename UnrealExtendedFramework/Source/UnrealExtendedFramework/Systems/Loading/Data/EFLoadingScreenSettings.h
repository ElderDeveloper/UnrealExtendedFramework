// EFLoadingScreenSettings.h — Project-wide loading screen configuration (DeveloperSettings)
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "EFLoadingScreenSettings.generated.h"

class UFont;
class UTexture2D;

/**
 * Project-wide loading screen settings, editable in
 * Project Settings > Extended Framework > Extended Loading Screen.
 *
 * Consumed by UEFLoadingScreenSubsystem and SEFLoadingScreenWidget. Everything the loading
 * screen draws or waits on is configured here, so no game code has to hold these values.
 */
UCLASS(Config = Game, defaultconfig, meta = (DisplayName = "Extended Loading Screen"))
class UNREALEXTENDEDFRAMEWORK_API UEFLoadingScreenSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UEFLoadingScreenSettings();

	virtual FName GetCategoryName() const override { return TEXT("Extended Framework"); }

	/** Multiple background images — one is chosen at random each time the loading screen appears. Falls back to BackgroundImage if empty. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Slate")
	TArray<TSoftObjectPtr<UTexture2D>> BackgroundImages;

	/** Single fallback image used when BackgroundImages is empty. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Slate")
	TSoftObjectPtr<UTexture2D> BackgroundImage;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Slate")
	FLinearColor BackgroundColor = FLinearColor::Black;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Slate")
	FLinearColor BackgroundImageTint = FLinearColor::White;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Slate")
	bool bShowCircularThrobber = true;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Slate", meta = (ClampMin = "1.0"))
	float CircularThrobberRadius = 36.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Slate", meta = (ClampMin = "1"))
	int32 CircularThrobberPieceCount = 12;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Slate", meta = (ClampMin = "0.05"))
	float CircularThrobberPeriod = 1.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Slate")
	FVector2D CircularThrobberOffset = FVector2D(0.0f, -64.0f);

	/** Optional label above the tip, e.g. "LOADING...". Can be used instead of the throbber. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Slate")
	bool bShowLoadingText = false;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Slate")
	FText LoadingText;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Slate", meta = (ClampMin = "1"))
	int32 LoadingTextFontSize = 28;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Slate")
	FLinearColor LoadingTextColor = FLinearColor::White;

	/** Vertical gap between the loading text and the tip below it. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Slate", meta = (ClampMin = "0.0"))
	float LoadingTextSpacing = 8.0f;

	/** Multiple tip lines — one is chosen at random each time the loading screen appears, like BackgroundImages. Blank entries are skipped. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Tips")
	TArray<FText> LoadingTips;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Tips")
	bool bShowLoadingTip = true;

	/** Optional font asset shared by the loading text and the tip; the default Slate font is used when unset. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Tips")
	TSoftObjectPtr<UFont> LoadingTipFont;

	/** Typeface inside the font asset, e.g. Regular / Bold / Italic. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Tips")
	FName LoadingTipFontTypeface = TEXT("Regular");

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Tips", meta = (ClampMin = "1"))
	int32 LoadingTipFontSize = 20;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Tips")
	FLinearColor LoadingTipColor = FLinearColor(0.85f, 0.80f, 0.68f, 1.0f);

	/**
	 * Offset of the whole loading text + tip stack from the bottom-center anchor,
	 * same convention as Circular Throbber Offset (negative Y moves up).
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Tips")
	FVector2D LoadingTipOffset = FVector2D(0.0f, -32.0f);

	/** Width in pixels before the tip wraps onto another line; 0 disables wrapping. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Tips", meta = (ClampMin = "0.0"))
	float LoadingTipWrapWidth = 1200.0f;

	/** Drop shadow keeps the loading text and tip readable over bright background images; zero offset disables it. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Tips")
	FVector2D LoadingTipShadowOffset = FVector2D(1.0f, 1.0f);

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Tips")
	FLinearColor LoadingTipShadowColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.75f);

	/**
	 * MoviePlayer-only minimum display time while the blocking load movie is active.
	 * Does not keep the screen up after the map finishes loading — use Hide Delay After World Ready for that.
	 * -1 means no MoviePlayer minimum.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Loading", meta = (ClampMin = "-1.0", DisplayName = "MoviePlayer Minimum Display Time"))
	float MinimumLoadingScreenDisplayTime = -1.0f;

	/**
	 * Raise the loading screen automatically on PreLoadMap, so every map travel is covered without
	 * each call site having to ask for it. Pairs with the automatic hide on world readiness.
	 * Never raised on a dedicated server. Turn this off to drive the screen only by hand, which is
	 * what a flow wanting MapTravelDelay before the blocking load starts should do —
	 * ShowLoadingScreen() is idempotent, so doing both is safe.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Loading")
	bool bShowAutomaticallyOnMapTravel = true;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Loading")
	bool bAutoCompleteWhenLoadingCompletes = true;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Loading")
	bool bWaitForManualStop = false;

	/**
	 * Allows MoviePlayer to tick the engine while a blocking loading screen is visible.
	 * Disabled by default because UE 5.5/5.6 can double-tick FRayTracingGeometryManager on this path.
	 * When disabled, MoviePlayer is stopped after map load and the viewport overlay handles world-readiness polling.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Loading")
	bool bAllowEngineTickDuringMoviePlayerLoadingScreen = false;

	/**
	 * Keep the screen up until every streaming level that should be loaded/visible is, and World
	 * Partition streaming has completed. Holds and readiness queries are honoured regardless.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "World Readiness")
	bool bWaitForWorldPartitionBeforeHiding = true;

	/**
	 * Extra seconds to keep the loading screen visible after the loaded world is considered ready
	 * (actors initialized, streaming levels loaded/visible, World Partition complete, no holds).
	 * Applies to map travel and to hold-driven sessions alike.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "World Readiness", meta = (ClampMin = "0.0", DisplayName = "Hide Delay After World Ready"))
	float HideDelayAfterWorldReady = 0.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "World Readiness", meta = (ClampMin = "0.01"))
	float WorldReadinessPollInterval = 0.1f;

	/**
	 * Longest the readiness wait may run (real seconds) before the screen hides anyway. On timeout
	 * every outstanding hold is discarded and logged. 0 disables the timeout entirely — the screen
	 * then stays up until readiness genuinely passes or something calls HideLoadingScreen.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "World Readiness", meta = (ClampMin = "0.0"))
	float WorldReadinessMaxWaitTime = 60.0f;

	/**
	 * Seconds between showing the loading screen and issuing the actual travel, so the screen is
	 * genuinely on screen before the blocking load begins. 0 travels immediately.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Travel", meta = (ClampMin = "0.0"))
	float MapTravelDelay = 0.25f;
};
