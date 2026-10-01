// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGChessWidgetBase.h"

#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Data/EGChessTimeControl.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Data/EGChessUIConfig.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Player/EGChessPlayerComponent.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Table/EGChessTableActor.h"

#define LOCTEXT_NAMESPACE "EGChess"

// ---------------------------------------------------------------------------------------------
// Base
// ---------------------------------------------------------------------------------------------

void UEGChessWidgetBase::InitChessWidget(UEGChessPlayerComponent* InOwner)
{
	OwnerComponent = InOwner;
	OnChessWidgetInitialized();
	OnChessStateChanged();
}

void UEGChessWidgetBase::HandleStateChanged()
{
	OnChessStateChanged();
}

AEGChessTableActor* UEGChessWidgetBase::GetTable() const
{
	const UEGChessPlayerComponent* Owner = OwnerComponent.Get();
	return Owner ? Owner->GetTable() : nullptr;
}

EEGChessColor UEGChessWidgetBase::GetLocalColor() const
{
	const UEGChessPlayerComponent* Owner = OwnerComponent.Get();
	return Owner ? Owner->GetLocalColor() : EEGChessColor::White;
}

EEGChessPhase UEGChessWidgetBase::GetPhase() const
{
	const AEGChessTableActor* Table = GetTable();
	return Table ? Table->GetPhase() : EEGChessPhase::Idle;
}

UTexture* UEGChessWidgetBase::GetPieceIcon(EEGChessPieceType Type, EEGChessColor Color) const
{
	AEGChessTableActor* Table = GetTable();
	return Table ? Table->GetPieceIcon(Type, Color) : nullptr;
}

FText UEGChessWidgetBase::GetPieceLetter(EEGChessPieceType Type)
{
	return Type == EEGChessPieceType::Pawn ? FText::FromString(TEXT("P")) : FText::FromString(EGChess::PieceLetter(Type));
}

FText UEGChessWidgetBase::FormatClock(float Seconds)
{
	Seconds = FMath::Max(0.0f, Seconds);
	if (Seconds < 10.0f)
	{
		return FText::FromString(FString::Printf(TEXT("%.1f"), FMath::FloorToFloat(Seconds * 10.0f) / 10.0f));
	}
	const int32 Whole = FMath::FloorToInt(Seconds);
	return FText::FromString(FString::Printf(TEXT("%d:%02d"), Whole / 60, Whole % 60));
}

void UEGChessWidgetBase::CloseChessWidget()
{
	if (UEGChessPlayerComponent* Owner = OwnerComponent.Get())
	{
		Owner->NotifyWidgetClosed(this);
	}
}

// ---------------------------------------------------------------------------------------------
// HUD
// ---------------------------------------------------------------------------------------------

FText UEGChessHUDWidgetBase::GetClockText(EEGChessColor Color) const
{
	return IsTimed() ? FormatClock(GetRemainingTime(Color)) : FText::GetEmpty();
}

float UEGChessHUDWidgetBase::GetRemainingTime(EEGChessColor Color) const
{
	const AEGChessTableActor* Table = GetTable();
	return Table ? Table->GetRemainingTime(Color) : 0.0f;
}

bool UEGChessHUDWidgetBase::IsClockRunning(EEGChessColor Color) const
{
	const AEGChessTableActor* Table = GetTable();
	return Table && Table->GetClockRef().bRunning && Table->GetClockRef().RunningColor == Color;
}

bool UEGChessHUDWidgetBase::IsLowTime(EEGChessColor Color) const
{
	const AEGChessTableActor* Table = GetTable();
	return Table && Table->IsTimed() && Table->GetRemainingTime(Color) <= Table->GetClockRef().LowTimeThreshold;
}

bool UEGChessHUDWidgetBase::IsTimed() const
{
	const AEGChessTableActor* Table = GetTable();
	return Table && Table->IsTimed();
}

EEGChessColor UEGChessHUDWidgetBase::GetSideToMove() const
{
	const AEGChessTableActor* Table = GetTable();
	return Table ? Table->GetSideToMove() : EEGChessColor::White;
}

bool UEGChessHUDWidgetBase::IsInCheck() const
{
	const AEGChessTableActor* Table = GetTable();
	return Table && Table->GetPhase() == EEGChessPhase::Playing && Table->GetPosition().IsInCheck(Table->GetSideToMove());
}

FText UEGChessHUDWidgetBase::GetPlayerName(EEGChessColor Color) const
{
	const AEGChessTableActor* Table = GetTable();
	return Table ? Table->GetSeatDisplayName(Table->GetSeatForColor(Color)) : FText::GetEmpty();
}

TArray<EEGChessPieceType> UEGChessHUDWidgetBase::GetCapturedPieces(EEGChessColor Color) const
{
	const AEGChessTableActor* Table = GetTable();
	return Table ? Table->GetCapturedPieces(Color) : TArray<EEGChessPieceType>();
}

int32 UEGChessHUDWidgetBase::GetMaterialAdvantage(EEGChessColor Color) const
{
	const AEGChessTableActor* Table = GetTable();
	if (!Table)
	{
		return 0;
	}
	const float Balance = Table->GetMaterialBalance() * (Color == EEGChessColor::White ? 1.0f : -1.0f);
	return FMath::Max(0, FMath::RoundToInt(Balance));
}

TArray<FString> UEGChessHUDWidgetBase::GetMoveList() const
{
	const AEGChessTableActor* Table = GetTable();
	return Table ? Table->GetMoveListSAN() : TArray<FString>();
}

FText UEGChessHUDWidgetBase::GetStatusText() const
{
	const AEGChessTableActor* Table = GetTable();
	const UEGChessUIConfig* UI = Table ? Table->GetUIConfig() : nullptr;
	switch (GetPhase())
	{
	case EEGChessPhase::WaitingForOpponent:
		return UI ? UI->WaitingText : LOCTEXT("WaitingFallback", "Waiting for an opponent");
	case EEGChessPhase::Starting:
		return FText::AsNumber(GetCountdownSeconds());
	default:
		return FText::GetEmpty();
	}
}

int32 UEGChessHUDWidgetBase::GetCountdownSeconds() const
{
	const AEGChessTableActor* Table = GetTable();
	if (!Table || Table->GetPhase() != EEGChessPhase::Starting)
	{
		return 0;
	}
	const int32 Seconds = FMath::CeilToInt(static_cast<float>(Table->GetPhaseEndServerTime() - Table->GetServerTime()));
	return FMath::Clamp(Seconds, 0, FMath::CeilToInt(Table->GetMatchRulesRef().StartCountdownSeconds));
}

FText UEGChessHUDWidgetBase::GetTimeControlText() const
{
	const AEGChessTableActor* Table = GetTable();
	if (!Table)
	{
		return FText::GetEmpty();
	}
	if (const UEGChessTimeControl* TimeControl = Table->GetMatchRulesRef().TimeControl)
	{
		return TimeControl->DisplayName;
	}
	const UEGChessUIConfig* UI = Table->GetUIConfig();
	return UI ? UI->UntimedText : LOCTEXT("UntimedFallback", "Untimed");
}

bool UEGChessHUDWidgetBase::CanPlayVersusAI() const
{
	const UEGChessPlayerComponent* Owner = GetChessPlayer();
	return Owner && Owner->CanPlayVersusAI();
}

// ---------------------------------------------------------------------------------------------
// Menu
// ---------------------------------------------------------------------------------------------

void UEGChessMenuWidgetBase::Resign() { if (UEGChessPlayerComponent* Owner = GetChessPlayer()) { Owner->RequestResign(); } }
void UEGChessMenuWidgetBase::OfferDraw() { if (UEGChessPlayerComponent* Owner = GetChessPlayer()) { Owner->OfferDraw(); Owner->CloseMenu(); } }
void UEGChessMenuWidgetBase::Takeback() { if (UEGChessPlayerComponent* Owner = GetChessPlayer()) { Owner->RequestTakeback(); Owner->CloseMenu(); } }
void UEGChessMenuWidgetBase::ToggleView() { if (UEGChessPlayerComponent* Owner = GetChessPlayer()) { Owner->ToggleView(); } }
void UEGChessMenuWidgetBase::Leave() { if (UEGChessPlayerComponent* Owner = GetChessPlayer()) { Owner->RequestLeave(); } }
void UEGChessMenuWidgetBase::PlayVersusAI() { if (UEGChessPlayerComponent* Owner = GetChessPlayer()) { Owner->PlayVersusAI(); Owner->CloseMenu(); } }

bool UEGChessMenuWidgetBase::CanResign() const { const UEGChessPlayerComponent* Owner = GetChessPlayer(); return Owner && Owner->CanResign(); }
bool UEGChessMenuWidgetBase::CanOfferDraw() const { const UEGChessPlayerComponent* Owner = GetChessPlayer(); return Owner && Owner->CanOfferDraw(); }
bool UEGChessMenuWidgetBase::CanTakeback() const { const UEGChessPlayerComponent* Owner = GetChessPlayer(); return Owner && Owner->CanTakeback(); }
bool UEGChessMenuWidgetBase::CanToggleView() const { const UEGChessPlayerComponent* Owner = GetChessPlayer(); return Owner && Owner->CanToggleView(); }
bool UEGChessMenuWidgetBase::CanPlayVersusAI() const { const UEGChessPlayerComponent* Owner = GetChessPlayer(); return Owner && Owner->CanPlayVersusAI(); }

// ---------------------------------------------------------------------------------------------
// Prompt
// ---------------------------------------------------------------------------------------------

void UEGChessPromptWidgetBase::SetupPrompt(EEGChessPromptKind InKind, const FText& Title)
{
	Kind = InKind;
	OnPromptSetup(InKind, Title);
}

void UEGChessPromptWidgetBase::Accept()
{
	if (UEGChessPlayerComponent* Owner = GetChessPlayer())
	{
		Owner->AnswerPrompt(Kind, true);
	}
}

void UEGChessPromptWidgetBase::Decline()
{
	if (UEGChessPlayerComponent* Owner = GetChessPlayer())
	{
		Owner->AnswerPrompt(Kind, false);
	}
}

// ---------------------------------------------------------------------------------------------
// Promotion
// ---------------------------------------------------------------------------------------------

void UEGChessPromotionWidgetBase::Choose(EEGChessPieceType Piece)
{
	if (UEGChessPlayerComponent* Owner = GetChessPlayer())
	{
		Owner->ChoosePromotion(Piece);
	}
}

void UEGChessPromotionWidgetBase::Cancel()
{
	if (UEGChessPlayerComponent* Owner = GetChessPlayer())
	{
		Owner->CancelPromotion();
	}
}

TArray<EEGChessPieceType> UEGChessPromotionWidgetBase::GetChoices()
{
	return { EEGChessPieceType::Queen, EEGChessPieceType::Rook, EEGChessPieceType::Bishop, EEGChessPieceType::Knight };
}

// ---------------------------------------------------------------------------------------------
// Game over
// ---------------------------------------------------------------------------------------------

void UEGChessGameOverWidgetBase::Rematch()
{
	if (UEGChessPlayerComponent* Owner = GetChessPlayer())
	{
		Owner->RequestRematch();
	}
}

void UEGChessGameOverWidgetBase::Leave()
{
	if (UEGChessPlayerComponent* Owner = GetChessPlayer())
	{
		Owner->RequestLeave();
	}
}

bool UEGChessGameOverWidgetBase::HasRequestedRematch() const
{
	const UEGChessPlayerComponent* Owner = GetChessPlayer();
	const AEGChessTableActor* Table = GetTable();
	return Owner && Table && Table->HasRematchRequest(Owner->GetSeat());
}

bool UEGChessGameOverWidgetBase::HasOpponentRequestedRematch() const
{
	const UEGChessPlayerComponent* Owner = GetChessPlayer();
	const AEGChessTableActor* Table = GetTable();
	return Owner && Table && Table->HasRematchRequest(EGChess::OtherSeat(Owner->GetSeat()));
}

#undef LOCTEXT_NAMESPACE
