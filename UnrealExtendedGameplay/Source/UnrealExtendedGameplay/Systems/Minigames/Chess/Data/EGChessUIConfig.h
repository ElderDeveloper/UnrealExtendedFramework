// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Table/EGChessMatchTypes.h"

#include "EGChessUIConfig.generated.h"

class UEGChessGameOverWidgetBase;
class UEGChessHUDWidgetBase;
class UEGChessMenuWidgetBase;
class UEGChessPromotionWidgetBase;
class UEGChessPromptWidgetBase;

/** Widget classes and every piece of text the plugin shows. Any widget left empty is skipped. */
UCLASS(BlueprintType)
class UNREALEXTENDEDGAMEPLAY_API UEGChessUIConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UEGChessUIConfig();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Widgets")
	TSoftClassPtr<UEGChessHUDWidgetBase> HUDClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Widgets")
	TSoftClassPtr<UEGChessMenuWidgetBase> MenuClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Widgets")
	TSoftClassPtr<UEGChessPromptWidgetBase> PromptClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Widgets")
	TSoftClassPtr<UEGChessPromotionWidgetBase> PromotionClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Widgets")
	TSoftClassPtr<UEGChessGameOverWidgetBase> GameOverClass;

	/** Viewport Z-order used by the default AddChessWidget. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Widgets")
	int32 BaseZOrder = 50;

	/** Move list uses piece icons (figurine notation) instead of English SAN letters. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Widgets")
	bool bFigurineNotation = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FText SitPromptText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FText WaitingText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FText PlayVersusAIText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FText UntimedText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	TMap<EEGChessNotice, FText> NoticeTexts;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	TMap<EEGChessEndReason, FText> ReasonTexts;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	TMap<EEGChessPromptKind, FText> PromptTitles;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FText WinText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FText LoseText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FText DrawText;

	/** For viewers who did not play the game. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FText WhiteWinsText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FText BlackWinsText;

	FText GetNoticeText(EEGChessNotice Notice) const;
	FText GetReasonText(EEGChessEndReason Reason) const;
	FText GetPromptTitle(EEGChessPromptKind Kind) const;
};
