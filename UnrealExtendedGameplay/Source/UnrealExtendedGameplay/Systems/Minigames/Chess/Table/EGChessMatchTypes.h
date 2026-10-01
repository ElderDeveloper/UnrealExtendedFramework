// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/OnlineReplStructs.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Core/EGChessTypes.h"

#include "EGChessMatchTypes.generated.h"

class APawn;
class APlayerState;
class UEGChessAIProfile;
class UEGChessTimeControl;

UENUM(BlueprintType)
enum class EEGChessPhase : uint8
{
	Idle,
	WaitingForOpponent,
	Starting,
	Playing,
	GameOver
};

UENUM(BlueprintType)
enum class EEGChessOpponentPolicy : uint8
{
	/** Wait for a second player. */
	PlayersOnly,
	/** The other seat is always the AI; a game starts as soon as someone sits. */
	AIOnly,
	/** Wait for a player, but the seated player can start against the AI from the menu. */
	PlayersOrAI,
	/** Both seats are the AI and games repeat. Showcases, decoration, soak tests. */
	AIvsAI
};

UENUM(BlueprintType)
enum class EEGChessMoveAnimation : uint8
{
	/** The move clip at normal speed. */
	Full,
	/** The move clip at the animation set's FastPlayRate. */
	Fast,
	/** No clip; pieces move on their own timing. */
	Off
};

UENUM(BlueprintType)
enum class EEGChessView : uint8
{
	/** The seat's authored camera, framing the whole board and clock. */
	Elevated,
	/** The seated character's head. */
	Eye
};

UENUM(BlueprintType)
enum class EEGChessEntrySide : uint8
{
	Left,
	Right,
	Back
};

/** What a seat's character is doing, as the server decided it. */
UENUM(BlueprintType)
enum class EEGChessSeatAction : uint8
{
	None,
	Entering,
	Seated,
	Exiting
};

UENUM(BlueprintType)
enum class EEGChessReaction : uint8
{
	OfferDraw,
	Resign,
	Check,
	Win,
	Lose,
	Draw,
	Thinking
};

/** Short, non-blocking messages for the seated player's HUD. */
UENUM(BlueprintType)
enum class EEGChessNotice : uint8
{
	NotYourTurn,
	IllegalMove,
	DrawOffered,
	DrawDeclined,
	OpponentJoined,
	OpponentLeft,
	OpponentDisconnected,
	OpponentReconnected,
	TakebackDone,
	LowTime,
	SeatTaken,
	RematchRequested
};

/** Presentation events that sounds and effects hang off. */
UENUM(BlueprintType)
enum class EEGChessEvent : uint8
{
	SitDown,
	StandUp,
	OpponentJoined,
	OpponentLeft,
	PiecesReset,
	Countdown,
	GameStart,
	PiecePickUp,
	PiecePlace,
	Capture,
	Castle,
	Promote,
	Check,
	ClockPress,
	ClockLowTimeLoop,
	YourTurn,
	LowTimeWarning,
	CursorMove,
	Select,
	Deselect,
	Illegal,
	DrawOffered,
	DrawDeclined,
	RematchRequested,
	Takeback,
	GameEnded,
	KingTopple,
	ResultWin,
	ResultLose,
	ResultDraw,
	Notice
};

UENUM(BlueprintType)
enum class EEGChessPromptKind : uint8
{
	ConfirmResign,
	ConfirmLeave,
	DrawOffered
};

/** Everything a game at this table is set up with. There is no setup screen. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGChessMatchRules
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chess")
	EEGChessOpponentPolicy OpponentPolicy = EEGChessOpponentPolicy::PlayersOrAI;

	/** Empty for untimed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chess")
	TObjectPtr<UEGChessTimeControl> TimeControl = nullptr;

	/** Used whenever the AI plays. Empty falls back to a Normal rule-based AI. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chess")
	TObjectPtr<UEGChessAIProfile> AIProfile = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chess")
	EEGChessSeat FirstWhiteSeat = EEGChessSeat::A;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chess")
	bool bSwapColoursOnRematch = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chess")
	EEGChessMoveAnimation MoveAnimation = EEGChessMoveAnimation::Full;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chess", meta = (ClampMin = "0"))
	float StartCountdownSeconds = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chess", meta = (ClampMin = "0"))
	float DisconnectForfeitSeconds = 30.0f;

	/** A player cannot offer again until they have made this many moves. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chess", meta = (ClampMin = "0"))
	int32 DrawOfferCooldownMoves = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chess")
	EEGChessView DefaultView = EEGChessView::Elevated;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chess")
	bool bAllowViewToggle = true;

	/** Threefold repetition and the fifty-move rule end the game by themselves. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chess")
	bool bAutomaticDrawClaims = true;

	/** Seconds between games under AIvsAI. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chess", meta = (ClampMin = "0"))
	float AIvsAIRestartDelay = 5.0f;
};

/** One committed move with what presentation and the move list need. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGChessMoveRecord
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Chess")
	FEGChessMove Move;

	UPROPERTY(BlueprintReadOnly, Category = "Chess")
	FString SAN;

	UPROPERTY(BlueprintReadOnly, Category = "Chess")
	EEGChessPieceType MovedType = EEGChessPieceType::None;

	UPROPERTY(BlueprintReadOnly, Category = "Chess")
	EEGChessPieceType CapturedType = EEGChessPieceType::None;

	UPROPERTY(BlueprintReadOnly, Category = "Chess")
	EEGChessColor MoverColor = EEGChessColor::White;

	UPROPERTY(BlueprintReadOnly, Category = "Chess")
	bool bGivesCheck = false;

	/** Clocks after this move, so a takeback can restore them. */
	UPROPERTY(BlueprintReadOnly, Category = "Chess")
	float WhiteTimeAfter = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Chess")
	float BlackTimeAfter = 0.0f;
};

/**
 * Server-time clock state. Clients compute the running side's display from
 * TurnStartServerTime, so nothing replicates per tick.
 */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGChessClockState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Chess")
	bool bTimed = false;

	UPROPERTY(BlueprintReadOnly, Category = "Chess")
	bool bRunning = false;

	UPROPERTY(BlueprintReadOnly, Category = "Chess")
	EEGChessColor RunningColor = EEGChessColor::White;

	UPROPERTY(BlueprintReadOnly, Category = "Chess")
	float WhiteRemaining = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Chess")
	float BlackRemaining = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Chess")
	float Increment = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Chess")
	float LowTimeThreshold = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Chess")
	double TurnStartServerTime = 0.0;

	float GetStored(EEGChessColor Color) const { return Color == EEGChessColor::White ? WhiteRemaining : BlackRemaining; }
	void SetStored(EEGChessColor Color, float Value) { (Color == EEGChessColor::White ? WhiteRemaining : BlackRemaining) = Value; }

	/** Remaining time for Color at server time Now. */
	float GetRemaining(EEGChessColor Color, double Now) const
	{
		const float Stored = GetStored(Color);
		if (!bTimed || !bRunning || RunningColor != Color)
		{
			return Stored;
		}
		return FMath::Max(0.0f, Stored - static_cast<float>(Now - TurnStartServerTime));
	}
};

/** Who holds a seat. Replicated on the seat component. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGChessSeatOccupant
{
	GENERATED_BODY()

	/** Null for an empty seat and for the AI. */
	UPROPERTY(BlueprintReadOnly, Category = "Chess")
	TObjectPtr<APlayerState> PlayerState = nullptr;

	/** The seated body: the player's pawn, the AI's body if a game placed one, else null. */
	UPROPERTY(BlueprintReadOnly, Category = "Chess")
	TObjectPtr<APawn> Pawn = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Chess")
	bool bAI = false;

	/** Player disconnected; the seat is held until ForfeitServerTime. */
	UPROPERTY(BlueprintReadOnly, Category = "Chess")
	bool bDisconnected = false;

	UPROPERTY(BlueprintReadOnly, Category = "Chess")
	double ForfeitServerTime = 0.0;

	/** Matches a reconnecting player to this seat. */
	UPROPERTY()
	FUniqueNetIdRepl UniqueId;

	bool IsOccupied() const { return bAI || PlayerState != nullptr || bDisconnected; }
	bool IsPlayer() const { return !bAI && (PlayerState != nullptr || bDisconnected); }
};

/**
 * The seat's current animation, as the server decided it. Every machine plays from this, and a
 * late joiner starts at Now - StartServerTime.
 */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGChessSeatAnimState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Chess")
	EEGChessSeatAction Action = EEGChessSeatAction::None;

	UPROPERTY(BlueprintReadOnly, Category = "Chess")
	EEGChessEntrySide Side = EEGChessEntrySide::Left;

	/** Bumped on every change so an identical action restarts. */
	UPROPERTY()
	uint8 Sequence = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Chess")
	double StartServerTime = 0.0;

	/** Where the pawn stood when the action began. */
	UPROPERTY(BlueprintReadOnly, Category = "Chess")
	FTransform StartTransform = FTransform::Identity;

	/** Snap into the seat without an entry clip (reconnects, AI bodies). */
	UPROPERTY()
	bool bSnap = false;
};

/** Pending draw offer and the per-side cooldown. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGChessDrawOfferState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Chess")
	bool bPending = false;

	UPROPERTY(BlueprintReadOnly, Category = "Chess")
	EEGChessColor OfferedBy = EEGChessColor::White;

	/** Moves each colour still has to make before offering again. */
	UPROPERTY(BlueprintReadOnly, Category = "Chess")
	int32 WhiteCooldown = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Chess")
	int32 BlackCooldown = 0;

	int32 GetCooldown(EEGChessColor Color) const { return Color == EEGChessColor::White ? WhiteCooldown : BlackCooldown; }
	void SetCooldown(EEGChessColor Color, int32 Value) { (Color == EEGChessColor::White ? WhiteCooldown : BlackCooldown) = Value; }
};

/** Everything a sound or effect might need to decide how to present an event. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGChessEventContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Chess")
	EEGChessEvent Event = EEGChessEvent::Notice;

	UPROPERTY(BlueprintReadOnly, Category = "Chess")
	EEGChessPieceType PieceType = EEGChessPieceType::None;

	UPROPERTY(BlueprintReadOnly, Category = "Chess")
	EEGChessColor PieceColor = EEGChessColor::White;

	UPROPERTY(BlueprintReadOnly, Category = "Chess")
	int32 FromSquare = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Chess")
	int32 ToSquare = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Chess")
	FVector Location = FVector::ZeroVector;

	/** Play at Location (3D) or as a 2D sound. */
	UPROPERTY(BlueprintReadOnly, Category = "Chess")
	bool bSpatial = true;

	UPROPERTY(BlueprintReadOnly, Category = "Chess")
	EEGChessSeat Seat = EEGChessSeat::A;

	UPROPERTY(BlueprintReadOnly, Category = "Chess")
	bool bLocalPlayerActed = false;

	/** False when a piece moved by itself (no character in that seat, or move animation off). */
	UPROPERTY(BlueprintReadOnly, Category = "Chess")
	bool bMovedByCharacter = false;

	UPROPERTY(BlueprintReadOnly, Category = "Chess")
	float TimeLeft = 0.0f;

	/** Starts (true) or stops (false) a looping event such as ClockLowTimeLoop. */
	UPROPERTY(BlueprintReadOnly, Category = "Chess")
	bool bStart = true;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEGChessPhaseChanged, EEGChessPhase, Phase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEGChessSeatChanged, EEGChessSeat, Seat);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEGChessColoursAssigned, EEGChessSeat, WhiteSeat);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEGChessMoveCommitted, const FEGChessMoveRecord&, Record);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FEGChessTakeback);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEGChessDrawOffered, EEGChessColor, OfferedBy);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEGChessRematchRequested, EEGChessSeat, Seat);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEGChessClockLow, EEGChessColor, Color);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEGChessGameEnded, const FEGChessResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FEGChessStateChanged);
