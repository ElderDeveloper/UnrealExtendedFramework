// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EGChessTypes.h"

/** One occupant of a square, packed: 0 empty, else PieceType | (Black ? 8 : 0). */
namespace EGChessPiece
{
	inline uint8 Make(EEGChessPieceType Type, EEGChessColor Color)
	{
		return Type == EEGChessPieceType::None ? 0 : static_cast<uint8>(static_cast<uint8>(Type) | (Color == EEGChessColor::Black ? 8 : 0));
	}
	inline EEGChessPieceType TypeOf(uint8 Piece) { return static_cast<EEGChessPieceType>(Piece & 7); }
	inline EEGChessColor ColorOf(uint8 Piece) { return (Piece & 8) != 0 ? EEGChessColor::Black : EEGChessColor::White; }
	inline bool IsEmpty(uint8 Piece) { return Piece == 0; }
}

namespace EGChessCastling
{
	enum Type : uint8
	{
		WhiteKingSide = 1,
		WhiteQueenSide = 2,
		BlackKingSide = 4,
		BlackQueenSide = 8,
		All = 15
	};
}

/** Everything MakeMove destroys, so UnmakeMove can put it back. */
struct FEGChessUndo
{
	uint8 CapturedPiece = 0;
	int8 CapturedSquare = -1;
	uint8 CastlingRights = 0;
	int8 EnPassantSquare = -1;
	int32 HalfmoveClock = 0;
	int32 FullmoveNumber = 1;
};

/**
 * FEGChessPosition
 *
 * The board and the four facts FEN carries besides it. Plain C++ with no UObject in sight, so
 * the AI can copy it by value and the tests can build one in a line.
 *
 * MakeMove trusts its input: it applies whatever move it is handed, flags included. Legality is
 * FEGChessRules' job; a move that did not come from move generation should be resolved through
 * FEGChessRules::FindLegalMove first, which also fills the flags.
 *
 * The Zobrist hash is computed on demand rather than maintained through make/unmake: it is only
 * needed once per real move (for repetition), while the AI makes and unmakes thousands of moves
 * that never need one.
 */
struct UNREALEXTENDEDGAMEPLAY_API FEGChessPosition
{
	uint8 Board[64];
	EEGChessColor SideToMove = EEGChessColor::White;
	uint8 CastlingRights = 0;
	int8 EnPassantSquare = -1;
	int32 HalfmoveClock = 0;
	int32 FullmoveNumber = 1;

	FEGChessPosition() { Clear(); }

	static FEGChessPosition Start();
	static const TCHAR* StartFEN();

	void Clear();

	bool FromFEN(const FString& Fen, FString* OutError = nullptr);
	FString ToFEN() const;

	uint8 Get(int32 Square) const { return EGChess::IsValidSquare(Square) ? Board[Square] : 0; }
	EEGChessPieceType TypeAt(int32 Square) const { return EGChessPiece::TypeOf(Get(Square)); }
	bool IsEmpty(int32 Square) const { return Get(Square) == 0; }
	bool HasColorAt(int32 Square, EEGChessColor Color) const
	{
		const uint8 Piece = Get(Square);
		return Piece != 0 && EGChessPiece::ColorOf(Piece) == Color;
	}
	void Set(int32 Square, uint8 Piece) { if (EGChess::IsValidSquare(Square)) { Board[Square] = Piece; } }

	void MakeMove(const FEGChessMove& Move, FEGChessUndo& OutUndo);
	void UnmakeMove(const FEGChessMove& Move, const FEGChessUndo& Undo);

	/** Convenience for callers that do not need to undo. */
	void ApplyMove(const FEGChessMove& Move)
	{
		FEGChessUndo Undo;
		MakeMove(Move, Undo);
	}

	int32 FindKing(EEGChessColor Color) const;
	bool IsSquareAttacked(int32 Square, EEGChessColor ByColor) const;
	bool IsInCheck(EEGChessColor Color) const;
	int32 CountAttackers(int32 Square, EEGChessColor ByColor) const;

	uint64 ComputeHash() const;

	/** Sum of piece values for one side (the king counts 0). */
	float Material(EEGChessColor Color) const;
};
