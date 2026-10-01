// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Table/EGChessMatchTypes.h"

#include "EGChessWidgetBase.generated.h"

class AEGChessTableActor;
class UEGChessPlayerComponent;
class UTexture;

/**
 * Widget bases for the chess UI. Plain UUserWidgets with BlueprintNativeEvents, like the skill
 * check's, so the plugin does not depend on CommonUI: a game subclasses them in its own style and
 * names the subclasses in its UEGChessUIConfig. They read everything from the table and act only
 * through the player component, so a widget never needs to know about networking.
 *
 * Input glyphs are the game's job; GetInputConfig-style lookups belong in the game's subclass.
 */
UCLASS(Abstract)
class UNREALEXTENDEDGAMEPLAY_API UEGChessWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitChessWidget(UEGChessPlayerComponent* InOwner);
	void HandleStateChanged();

	bool IsModal() const { return bModal; }

	UFUNCTION(BlueprintPure, Category = "Chess")
	UEGChessPlayerComponent* GetChessPlayer() const { return OwnerComponent.Get(); }

	UFUNCTION(BlueprintPure, Category = "Chess")
	AEGChessTableActor* GetTable() const;

	UFUNCTION(BlueprintPure, Category = "Chess")
	EEGChessColor GetLocalColor() const;

	UFUNCTION(BlueprintPure, Category = "Chess")
	EEGChessPhase GetPhase() const;

	/** Icon for a piece, or null (show a letter then). */
	UFUNCTION(BlueprintCallable, Category = "Chess")
	UTexture* GetPieceIcon(EEGChessPieceType Type, EEGChessColor Color) const;

	/** "K", "Q", ... for a piece without an icon. */
	UFUNCTION(BlueprintPure, Category = "Chess")
	static FText GetPieceLetter(EEGChessPieceType Type);

	/** m:ss, switching to s.t under ten seconds. */
	UFUNCTION(BlueprintPure, Category = "Chess")
	static FText FormatClock(float Seconds);

	/** Ask the owner to close this widget. */
	UFUNCTION(BlueprintCallable, Category = "Chess")
	void CloseChessWidget();

protected:
	UFUNCTION(BlueprintNativeEvent, Category = "Chess")
	void OnChessWidgetInitialized();
	virtual void OnChessWidgetInitialized_Implementation() {}

	/** Anything on the table changed: redraw. Clocks tick continuously; read them each frame. */
	UFUNCTION(BlueprintNativeEvent, Category = "Chess")
	void OnChessStateChanged();
	virtual void OnChessStateChanged_Implementation() {}

	/** Modal widgets block board input while open. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chess")
	bool bModal = false;

private:
	TWeakObjectPtr<UEGChessPlayerComponent> OwnerComponent;
};

/** Whose turn, both clocks, check warning, captured pieces, the move list, notices. */
UCLASS(Abstract)
class UNREALEXTENDEDGAMEPLAY_API UEGChessHUDWidgetBase : public UEGChessWidgetBase
{
	GENERATED_BODY()

public:
	void ShowNotice(EEGChessNotice Notice, const FText& Text) { OnNotice(Notice, Text); }

	UFUNCTION(BlueprintPure, Category = "Chess")
	FText GetClockText(EEGChessColor Color) const;

	UFUNCTION(BlueprintPure, Category = "Chess")
	float GetRemainingTime(EEGChessColor Color) const;

	UFUNCTION(BlueprintPure, Category = "Chess")
	bool IsClockRunning(EEGChessColor Color) const;

	UFUNCTION(BlueprintPure, Category = "Chess")
	bool IsLowTime(EEGChessColor Color) const;

	UFUNCTION(BlueprintPure, Category = "Chess")
	bool IsTimed() const;

	UFUNCTION(BlueprintPure, Category = "Chess")
	EEGChessColor GetSideToMove() const;

	UFUNCTION(BlueprintPure, Category = "Chess")
	bool IsInCheck() const;

	UFUNCTION(BlueprintPure, Category = "Chess")
	FText GetPlayerName(EEGChessColor Color) const;

	/** Pieces Color has lost. */
	UFUNCTION(BlueprintPure, Category = "Chess")
	TArray<EEGChessPieceType> GetCapturedPieces(EEGChessColor Color) const;

	/** Material advantage of Color, 0 when level or behind. */
	UFUNCTION(BlueprintPure, Category = "Chess")
	int32 GetMaterialAdvantage(EEGChessColor Color) const;

	UFUNCTION(BlueprintPure, Category = "Chess")
	TArray<FString> GetMoveList() const;

	/** "Waiting for an opponent", the countdown, or empty while playing. */
	UFUNCTION(BlueprintPure, Category = "Chess")
	FText GetStatusText() const;

	/** Seconds left in the start countdown. */
	UFUNCTION(BlueprintPure, Category = "Chess")
	int32 GetCountdownSeconds() const;

	/** The table's time control, e.g. "Blitz 3+2", or "Untimed". */
	UFUNCTION(BlueprintPure, Category = "Chess")
	FText GetTimeControlText() const;

	UFUNCTION(BlueprintPure, Category = "Chess")
	bool CanPlayVersusAI() const;

protected:
	UFUNCTION(BlueprintNativeEvent, Category = "Chess")
	void OnNotice(EEGChessNotice Notice, const FText& Text);
	virtual void OnNotice_Implementation(EEGChessNotice Notice, const FText& Text) {}
};

/** Resign, offer draw, takeback, switch view, leave; "play against the AI" while waiting. */
UCLASS(Abstract)
class UNREALEXTENDEDGAMEPLAY_API UEGChessMenuWidgetBase : public UEGChessWidgetBase
{
	GENERATED_BODY()

public:
	UEGChessMenuWidgetBase() { bModal = true; }

	UFUNCTION(BlueprintCallable, Category = "Chess")
	void Resign();

	UFUNCTION(BlueprintCallable, Category = "Chess")
	void OfferDraw();

	UFUNCTION(BlueprintCallable, Category = "Chess")
	void Takeback();

	UFUNCTION(BlueprintCallable, Category = "Chess")
	void ToggleView();

	UFUNCTION(BlueprintCallable, Category = "Chess")
	void Leave();

	UFUNCTION(BlueprintCallable, Category = "Chess")
	void PlayVersusAI();

	UFUNCTION(BlueprintPure, Category = "Chess")
	bool CanResign() const;

	UFUNCTION(BlueprintPure, Category = "Chess")
	bool CanOfferDraw() const;

	UFUNCTION(BlueprintPure, Category = "Chess")
	bool CanTakeback() const;

	UFUNCTION(BlueprintPure, Category = "Chess")
	bool CanToggleView() const;

	UFUNCTION(BlueprintPure, Category = "Chess")
	bool CanPlayVersusAI() const;
};

/** A confirmation or an incoming request: resign?, resign and leave?, draw offered. */
UCLASS(Abstract)
class UNREALEXTENDEDGAMEPLAY_API UEGChessPromptWidgetBase : public UEGChessWidgetBase
{
	GENERATED_BODY()

public:
	UEGChessPromptWidgetBase() { bModal = true; }

	void SetupPrompt(EEGChessPromptKind InKind, const FText& Title);

	UFUNCTION(BlueprintPure, Category = "Chess")
	EEGChessPromptKind GetPromptKind() const { return Kind; }

	UFUNCTION(BlueprintCallable, Category = "Chess")
	void Accept();

	UFUNCTION(BlueprintCallable, Category = "Chess")
	void Decline();

protected:
	UFUNCTION(BlueprintNativeEvent, Category = "Chess")
	void OnPromptSetup(EEGChessPromptKind PromptKind, const FText& Title);
	virtual void OnPromptSetup_Implementation(EEGChessPromptKind PromptKind, const FText& Title) {}

private:
	EEGChessPromptKind Kind = EEGChessPromptKind::ConfirmResign;
};

/** The four promotion choices. Queen is the default; the clock keeps running while open. */
UCLASS(Abstract)
class UNREALEXTENDEDGAMEPLAY_API UEGChessPromotionWidgetBase : public UEGChessWidgetBase
{
	GENERATED_BODY()

public:
	UEGChessPromotionWidgetBase() { bModal = true; }

	void SetupPromotion(EEGChessColor Color) { OnPromotionSetup(Color); }

	UFUNCTION(BlueprintCallable, Category = "Chess")
	void Choose(EEGChessPieceType Piece);

	UFUNCTION(BlueprintCallable, Category = "Chess")
	void Cancel();

	/** Queen, rook, bishop, knight, in that order. */
	UFUNCTION(BlueprintPure, Category = "Chess")
	static TArray<EEGChessPieceType> GetChoices();

protected:
	UFUNCTION(BlueprintNativeEvent, Category = "Chess")
	void OnPromotionSetup(EEGChessColor Color);
	virtual void OnPromotionSetup_Implementation(EEGChessColor Color) {}
};

/** Result and reason; Rematch (showing whether the opponent asked) and Leave. */
UCLASS(Abstract)
class UNREALEXTENDEDGAMEPLAY_API UEGChessGameOverWidgetBase : public UEGChessWidgetBase
{
	GENERATED_BODY()

public:
	void SetupGameOver(const FEGChessResult& Result, const FText& Headline, const FText& Reason) { OnGameOverSetup(Result, Headline, Reason); }

	UFUNCTION(BlueprintCallable, Category = "Chess")
	void Rematch();

	UFUNCTION(BlueprintCallable, Category = "Chess")
	void Leave();

	UFUNCTION(BlueprintPure, Category = "Chess")
	bool HasRequestedRematch() const;

	UFUNCTION(BlueprintPure, Category = "Chess")
	bool HasOpponentRequestedRematch() const;

protected:
	UFUNCTION(BlueprintNativeEvent, Category = "Chess")
	void OnGameOverSetup(const FEGChessResult& Result, const FText& Headline, const FText& Reason);
	virtual void OnGameOverSetup_Implementation(const FEGChessResult& Result, const FText& Headline, const FText& Reason) {}
};
