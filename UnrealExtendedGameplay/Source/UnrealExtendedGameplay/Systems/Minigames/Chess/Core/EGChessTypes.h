// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

#include "EGChessTypes.generated.h"

/**
 * Chess
 *
 * A complete, generic chess game: one placed AEGChessTableActor, content from data assets, and
 * a rules core that is plain C++. Documents/ExtendedGameplayChessPlan.md (in the first project
 * that used it) is the design; the notes in these headers explain the parts a reader would get
 * wrong without it.
 *
 * Squares are 0..63 with a1 = 0, b1 = 1 ... h8 = 63, i.e. Square = Rank * 8 + File. Every API
 * that names a square uses that index; INDEX_NONE (or -1) means "no square".
 */

/** Interaction id the table's Sit definition uses. */
UNREALEXTENDEDGAMEPLAY_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_EGChess_Interaction_Sit);

UENUM(BlueprintType)
enum class EEGChessColor : uint8
{
	White,
	Black
};

UENUM(BlueprintType)
enum class EEGChessPieceType : uint8
{
	None,
	Pawn,
	Knight,
	Bishop,
	Rook,
	Queen,
	King
};

/** The two chairs. Colours are not tied to seats: they swap on rematches. */
UENUM(BlueprintType)
enum class EEGChessSeat : uint8
{
	A,
	B
};

UENUM(BlueprintType)
enum class EEGChessGameResult : uint8
{
	None,
	WhiteWins,
	BlackWins,
	Draw
};

UENUM(BlueprintType)
enum class EEGChessEndReason : uint8
{
	None,
	Checkmate,
	Resignation,
	Timeout,
	Stalemate,
	ThreefoldRepetition,
	FiftyMoveRule,
	InsufficientMaterial,
	Agreement,
	Forfeit
};

/** What the rules say about a position, before clocks and players are considered. */
enum class EEGChessStatus : uint8
{
	Ongoing,
	Checkmate,
	Stalemate,
	ThreefoldRepetition,
	FiftyMoveRule,
	InsufficientMaterial
};

namespace EGChessMoveFlags
{
	enum Type : uint8
	{
		None = 0,
		Capture = 1 << 0,
		DoublePush = 1 << 1,
		EnPassant = 1 << 2,
		CastleKingSide = 1 << 3,
		CastleQueenSide = 1 << 4,
		Promotion = 1 << 5
	};
}

/**
 * One move. Small on purpose: it is what clients send and what the move history replicates.
 * Equality ignores the flags, which are derived; a client only has to name from, to and the
 * promotion piece, and the server fills in the rest from the position.
 */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGChessMove
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Chess")
	uint8 From = 255;

	UPROPERTY(BlueprintReadWrite, Category = "Chess")
	uint8 To = 255;

	UPROPERTY(BlueprintReadWrite, Category = "Chess")
	EEGChessPieceType Promotion = EEGChessPieceType::None;

	/** EGChessMoveFlags bits. */
	UPROPERTY(BlueprintReadOnly, Category = "Chess")
	uint8 Flags = 0;

	FEGChessMove() = default;
	FEGChessMove(int32 InFrom, int32 InTo, EEGChessPieceType InPromotion = EEGChessPieceType::None, uint8 InFlags = 0)
		: From(static_cast<uint8>(InFrom)), To(static_cast<uint8>(InTo)), Promotion(InPromotion), Flags(InFlags)
	{
	}

	bool IsValid() const { return From < 64 && To < 64 && From != To; }
	bool HasFlag(uint8 Flag) const { return (Flags & Flag) != 0; }
	bool IsCapture() const { return HasFlag(EGChessMoveFlags::Capture); }
	bool IsCastle() const { return HasFlag(EGChessMoveFlags::CastleKingSide | EGChessMoveFlags::CastleQueenSide); }

	bool operator==(const FEGChessMove& Other) const
	{
		return From == Other.From && To == Other.To && Promotion == Other.Promotion;
	}
	bool operator!=(const FEGChessMove& Other) const { return !(*this == Other); }
};

/** How a game ended. Result None means it has not. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGChessResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Chess")
	EEGChessGameResult Result = EEGChessGameResult::None;

	UPROPERTY(BlueprintReadOnly, Category = "Chess")
	EEGChessEndReason Reason = EEGChessEndReason::None;

	bool IsOver() const { return Result != EEGChessGameResult::None; }
	bool IsDraw() const { return Result == EEGChessGameResult::Draw; }

	/** The winner's colour. Only meaningful when the game is over and not drawn. */
	EEGChessColor GetWinner() const { return Result == EEGChessGameResult::BlackWins ? EEGChessColor::Black : EEGChessColor::White; }
};

namespace EGChess
{
	inline int32 FileOf(int32 Square) { return Square & 7; }
	inline int32 RankOf(int32 Square) { return Square >> 3; }
	inline int32 MakeSquare(int32 File, int32 Rank) { return Rank * 8 + File; }
	inline bool IsOnBoard(int32 File, int32 Rank) { return File >= 0 && File < 8 && Rank >= 0 && Rank < 8; }
	inline bool IsValidSquare(int32 Square) { return Square >= 0 && Square < 64; }
	inline EEGChessColor Opposite(EEGChessColor Color) { return Color == EEGChessColor::White ? EEGChessColor::Black : EEGChessColor::White; }
	inline EEGChessSeat OtherSeat(EEGChessSeat Seat) { return Seat == EEGChessSeat::A ? EEGChessSeat::B : EEGChessSeat::A; }
	inline int32 SeatIndex(EEGChessSeat Seat) { return Seat == EEGChessSeat::A ? 0 : 1; }
	inline int32 ColorIndex(EEGChessColor Color) { return Color == EEGChessColor::White ? 0 : 1; }

	/** "e4" for 28. Empty for an invalid square. */
	UNREALEXTENDEDGAMEPLAY_API FString SquareName(int32 Square);

	/** 28 for "e4". INDEX_NONE when it does not parse. */
	UNREALEXTENDEDGAMEPLAY_API int32 ParseSquare(const FString& Name);

	/** Material value used by the AI and the HUD's material balance. King counts 0. */
	UNREALEXTENDEDGAMEPLAY_API float PieceValue(EEGChessPieceType Type);

	/** Upper-case SAN letter: K Q R B N, empty for a pawn. */
	UNREALEXTENDEDGAMEPLAY_API FString PieceLetter(EEGChessPieceType Type);
}
