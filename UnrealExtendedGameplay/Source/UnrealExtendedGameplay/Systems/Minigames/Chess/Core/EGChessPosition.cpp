// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGChessPosition.h"

namespace EGChessPositionPrivate
{
	const int32 KnightOffsets[8][2] = { { 1, 2 }, { 2, 1 }, { 2, -1 }, { 1, -2 }, { -1, -2 }, { -2, -1 }, { -2, 1 }, { -1, 2 } };
	const int32 KingOffsets[8][2] = { { 1, 0 }, { 1, 1 }, { 0, 1 }, { -1, 1 }, { -1, 0 }, { -1, -1 }, { 0, -1 }, { 1, -1 } };
	const int32 RookDirs[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
	const int32 BishopDirs[4][2] = { { 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 } };

	/** Fixed-seed splitmix64, so every machine and every run gets the same keys. */
	struct FZobristKeys
	{
		uint64 Pieces[16][64];
		uint64 BlackToMove;
		uint64 Castling[16];
		uint64 EnPassantFile[8];

		FZobristKeys()
		{
			uint64 State = 0x9E3779B97F4A7C15ull;
			auto Next = [&State]()
			{
				uint64 Z = (State += 0x9E3779B97F4A7C15ull);
				Z = (Z ^ (Z >> 30)) * 0xBF58476D1CE4E5B9ull;
				Z = (Z ^ (Z >> 27)) * 0x94D049BB133111EBull;
				return Z ^ (Z >> 31);
			};
			for (int32 P = 0; P < 16; ++P)
			{
				for (int32 S = 0; S < 64; ++S)
				{
					Pieces[P][S] = Next();
				}
			}
			BlackToMove = Next();
			for (int32 C = 0; C < 16; ++C)
			{
				Castling[C] = Next();
			}
			for (int32 F = 0; F < 8; ++F)
			{
				EnPassantFile[F] = Next();
			}
		}
	};

	const FZobristKeys& Keys()
	{
		static const FZobristKeys Instance;
		return Instance;
	}

	/** Castling rights lost when a piece leaves or lands on a square. */
	uint8 CastlingMaskFor(int32 Square)
	{
		switch (Square)
		{
		case 0: return EGChessCastling::WhiteQueenSide;
		case 7: return EGChessCastling::WhiteKingSide;
		case 4: return EGChessCastling::WhiteKingSide | EGChessCastling::WhiteQueenSide;
		case 56: return EGChessCastling::BlackQueenSide;
		case 63: return EGChessCastling::BlackKingSide;
		case 60: return EGChessCastling::BlackKingSide | EGChessCastling::BlackQueenSide;
		default: return 0;
		}
	}

	uint8 PieceFromChar(TCHAR C)
	{
		const EEGChessColor Color = FChar::IsUpper(C) ? EEGChessColor::White : EEGChessColor::Black;
		switch (FChar::ToLower(C))
		{
		case TEXT('p'): return EGChessPiece::Make(EEGChessPieceType::Pawn, Color);
		case TEXT('n'): return EGChessPiece::Make(EEGChessPieceType::Knight, Color);
		case TEXT('b'): return EGChessPiece::Make(EEGChessPieceType::Bishop, Color);
		case TEXT('r'): return EGChessPiece::Make(EEGChessPieceType::Rook, Color);
		case TEXT('q'): return EGChessPiece::Make(EEGChessPieceType::Queen, Color);
		case TEXT('k'): return EGChessPiece::Make(EEGChessPieceType::King, Color);
		default: return 0;
		}
	}

	TCHAR CharFromPiece(uint8 Piece)
	{
		TCHAR C = TEXT('?');
		switch (EGChessPiece::TypeOf(Piece))
		{
		case EEGChessPieceType::Pawn: C = TEXT('p'); break;
		case EEGChessPieceType::Knight: C = TEXT('n'); break;
		case EEGChessPieceType::Bishop: C = TEXT('b'); break;
		case EEGChessPieceType::Rook: C = TEXT('r'); break;
		case EEGChessPieceType::Queen: C = TEXT('q'); break;
		case EEGChessPieceType::King: C = TEXT('k'); break;
		default: break;
		}
		return EGChessPiece::ColorOf(Piece) == EEGChessColor::White ? FChar::ToUpper(C) : C;
	}
}


const TCHAR* FEGChessPosition::StartFEN()
{
	return TEXT("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

FEGChessPosition FEGChessPosition::Start()
{
	FEGChessPosition Position;
	Position.FromFEN(StartFEN());
	return Position;
}

void FEGChessPosition::Clear()
{
	FMemory::Memzero(Board, sizeof(Board));
	SideToMove = EEGChessColor::White;
	CastlingRights = 0;
	EnPassantSquare = -1;
	HalfmoveClock = 0;
	FullmoveNumber = 1;
}

bool FEGChessPosition::FromFEN(const FString& Fen, FString* OutError)
{
	auto Fail = [OutError](const TCHAR* Why)
	{
		if (OutError)
		{
			*OutError = Why;
		}
		return false;
	};

	TArray<FString> Fields;
	Fen.TrimStartAndEnd().ParseIntoArrayWS(Fields);
	if (Fields.Num() < 4)
	{
		return Fail(TEXT("FEN needs at least four fields."));
	}

	FEGChessPosition Parsed;
	int32 Rank = 7;
	int32 File = 0;
	for (const TCHAR C : Fields[0])
	{
		if (C == TEXT('/'))
		{
			if (File != 8)
			{
				return Fail(TEXT("A rank does not have eight files."));
			}
			--Rank;
			File = 0;
			continue;
		}
		if (FChar::IsDigit(C))
		{
			File += C - TEXT('0');
			if (File > 8)
			{
				return Fail(TEXT("A rank overflows."));
			}
			continue;
		}
		const uint8 Piece = EGChessPositionPrivate::PieceFromChar(C);
		if (Piece == 0 || !EGChess::IsOnBoard(File, Rank))
		{
			return Fail(TEXT("Unknown piece or square in the board field."));
		}
		Parsed.Board[EGChess::MakeSquare(File, Rank)] = Piece;
		++File;
	}
	if (Rank != 0 || File != 8)
	{
		return Fail(TEXT("The board field does not describe eight ranks."));
	}

	if (Fields[1] == TEXT("w"))
	{
		Parsed.SideToMove = EEGChessColor::White;
	}
	else if (Fields[1] == TEXT("b"))
	{
		Parsed.SideToMove = EEGChessColor::Black;
	}
	else
	{
		return Fail(TEXT("Side to move must be w or b."));
	}

	Parsed.CastlingRights = 0;
	if (Fields[2] != TEXT("-"))
	{
		for (const TCHAR C : Fields[2])
		{
			switch (C)
			{
			case TEXT('K'): Parsed.CastlingRights |= EGChessCastling::WhiteKingSide; break;
			case TEXT('Q'): Parsed.CastlingRights |= EGChessCastling::WhiteQueenSide; break;
			case TEXT('k'): Parsed.CastlingRights |= EGChessCastling::BlackKingSide; break;
			case TEXT('q'): Parsed.CastlingRights |= EGChessCastling::BlackQueenSide; break;
			default: return Fail(TEXT("Unknown castling right."));
			}
		}
	}

	Parsed.EnPassantSquare = -1;
	if (Fields[3] != TEXT("-"))
	{
		const int32 Square = EGChess::ParseSquare(Fields[3]);
		if (Square == INDEX_NONE)
		{
			return Fail(TEXT("Bad en passant square."));
		}
		Parsed.EnPassantSquare = static_cast<int8>(Square);
	}

	Parsed.HalfmoveClock = Fields.Num() > 4 ? FCString::Atoi(*Fields[4]) : 0;
	Parsed.FullmoveNumber = Fields.Num() > 5 ? FMath::Max(1, FCString::Atoi(*Fields[5])) : 1;

	if (Parsed.FindKing(EEGChessColor::White) == INDEX_NONE || Parsed.FindKing(EEGChessColor::Black) == INDEX_NONE)
	{
		return Fail(TEXT("Both sides need a king."));
	}

	*this = Parsed;
	return true;
}

FString FEGChessPosition::ToFEN() const
{
	FString Out;
	for (int32 Rank = 7; Rank >= 0; --Rank)
	{
		int32 Empty = 0;
		for (int32 File = 0; File < 8; ++File)
		{
			const uint8 Piece = Board[EGChess::MakeSquare(File, Rank)];
			if (Piece == 0)
			{
				++Empty;
				continue;
			}
			if (Empty > 0)
			{
				Out.AppendInt(Empty);
				Empty = 0;
			}
			Out.AppendChar(EGChessPositionPrivate::CharFromPiece(Piece));
		}
		if (Empty > 0)
		{
			Out.AppendInt(Empty);
		}
		if (Rank > 0)
		{
			Out.AppendChar(TEXT('/'));
		}
	}

	Out += SideToMove == EEGChessColor::White ? TEXT(" w ") : TEXT(" b ");

	if (CastlingRights == 0)
	{
		Out.AppendChar(TEXT('-'));
	}
	else
	{
		if (CastlingRights & EGChessCastling::WhiteKingSide) { Out.AppendChar(TEXT('K')); }
		if (CastlingRights & EGChessCastling::WhiteQueenSide) { Out.AppendChar(TEXT('Q')); }
		if (CastlingRights & EGChessCastling::BlackKingSide) { Out.AppendChar(TEXT('k')); }
		if (CastlingRights & EGChessCastling::BlackQueenSide) { Out.AppendChar(TEXT('q')); }
	}

	Out.AppendChar(TEXT(' '));
	Out += EnPassantSquare >= 0 ? EGChess::SquareName(EnPassantSquare) : FString(TEXT("-"));
	Out += FString::Printf(TEXT(" %d %d"), HalfmoveClock, FullmoveNumber);
	return Out;
}

void FEGChessPosition::MakeMove(const FEGChessMove& Move, FEGChessUndo& OutUndo)
{
	OutUndo.CastlingRights = CastlingRights;
	OutUndo.EnPassantSquare = EnPassantSquare;
	OutUndo.HalfmoveClock = HalfmoveClock;
	OutUndo.FullmoveNumber = FullmoveNumber;
	OutUndo.CapturedPiece = 0;
	OutUndo.CapturedSquare = -1;

	const uint8 Moving = Board[Move.From];
	const EEGChessPieceType MovingType = EGChessPiece::TypeOf(Moving);
	const EEGChessColor Us = SideToMove;

	// Capture: en passant takes the pawn beside the target, everything else takes the target.
	if (Move.HasFlag(EGChessMoveFlags::EnPassant))
	{
		const int32 CapturedSquare = Move.To + (Us == EEGChessColor::White ? -8 : 8);
		OutUndo.CapturedPiece = Board[CapturedSquare];
		OutUndo.CapturedSquare = static_cast<int8>(CapturedSquare);
		Board[CapturedSquare] = 0;
	}
	else if (Board[Move.To] != 0)
	{
		OutUndo.CapturedPiece = Board[Move.To];
		OutUndo.CapturedSquare = static_cast<int8>(Move.To);
	}

	Board[Move.To] = Moving;
	Board[Move.From] = 0;

	if (Move.Promotion != EEGChessPieceType::None && MovingType == EEGChessPieceType::Pawn)
	{
		Board[Move.To] = EGChessPiece::Make(Move.Promotion, Us);
	}

	// The rook's half of castling.
	if (Move.HasFlag(EGChessMoveFlags::CastleKingSide))
	{
		const int32 RookFrom = Move.To + 1;
		const int32 RookTo = Move.To - 1;
		Board[RookTo] = Board[RookFrom];
		Board[RookFrom] = 0;
	}
	else if (Move.HasFlag(EGChessMoveFlags::CastleQueenSide))
	{
		const int32 RookFrom = Move.To - 2;
		const int32 RookTo = Move.To + 1;
		Board[RookTo] = Board[RookFrom];
		Board[RookFrom] = 0;
	}

	CastlingRights &= ~(EGChessPositionPrivate::CastlingMaskFor(Move.From) | EGChessPositionPrivate::CastlingMaskFor(Move.To));

	EnPassantSquare = -1;
	if (MovingType == EEGChessPieceType::Pawn && FMath::Abs(static_cast<int32>(Move.To) - static_cast<int32>(Move.From)) == 16)
	{
		EnPassantSquare = static_cast<int8>((Move.From + Move.To) / 2);
	}

	HalfmoveClock = (MovingType == EEGChessPieceType::Pawn || OutUndo.CapturedPiece != 0) ? 0 : HalfmoveClock + 1;
	if (Us == EEGChessColor::Black)
	{
		++FullmoveNumber;
	}
	SideToMove = EGChess::Opposite(Us);
}

void FEGChessPosition::UnmakeMove(const FEGChessMove& Move, const FEGChessUndo& Undo)
{
	SideToMove = EGChess::Opposite(SideToMove);
	const EEGChessColor Us = SideToMove;

	uint8 Moved = Board[Move.To];
	if (Move.Promotion != EEGChessPieceType::None && Move.HasFlag(EGChessMoveFlags::Promotion))
	{
		Moved = EGChessPiece::Make(EEGChessPieceType::Pawn, Us);
	}
	Board[Move.From] = Moved;
	Board[Move.To] = 0;

	if (Move.HasFlag(EGChessMoveFlags::CastleKingSide))
	{
		const int32 RookFrom = Move.To + 1;
		const int32 RookTo = Move.To - 1;
		Board[RookFrom] = Board[RookTo];
		Board[RookTo] = 0;
	}
	else if (Move.HasFlag(EGChessMoveFlags::CastleQueenSide))
	{
		const int32 RookFrom = Move.To - 2;
		const int32 RookTo = Move.To + 1;
		Board[RookFrom] = Board[RookTo];
		Board[RookTo] = 0;
	}

	if (Undo.CapturedSquare >= 0)
	{
		Board[Undo.CapturedSquare] = Undo.CapturedPiece;
	}

	CastlingRights = Undo.CastlingRights;
	EnPassantSquare = Undo.EnPassantSquare;
	HalfmoveClock = Undo.HalfmoveClock;
	FullmoveNumber = Undo.FullmoveNumber;
}

int32 FEGChessPosition::FindKing(EEGChessColor Color) const
{
	const uint8 King = EGChessPiece::Make(EEGChessPieceType::King, Color);
	for (int32 Square = 0; Square < 64; ++Square)
	{
		if (Board[Square] == King)
		{
			return Square;
		}
	}
	return INDEX_NONE;
}

int32 FEGChessPosition::CountAttackers(int32 Square, EEGChessColor ByColor) const
{
	if (!EGChess::IsValidSquare(Square))
	{
		return 0;
	}

	int32 Count = 0;
	const int32 File = EGChess::FileOf(Square);
	const int32 Rank = EGChess::RankOf(Square);

	// Pawns attack diagonally forward, so look one rank "behind" the square from their side.
	const int32 PawnRank = Rank + (ByColor == EEGChessColor::White ? -1 : 1);
	const uint8 Pawn = EGChessPiece::Make(EEGChessPieceType::Pawn, ByColor);
	for (const int32 DF : { -1, 1 })
	{
		if (EGChess::IsOnBoard(File + DF, PawnRank) && Board[EGChess::MakeSquare(File + DF, PawnRank)] == Pawn)
		{
			++Count;
		}
	}

	const uint8 Knight = EGChessPiece::Make(EEGChessPieceType::Knight, ByColor);
	for (const auto& Offset : EGChessPositionPrivate::KnightOffsets)
	{
		const int32 F = File + Offset[0];
		const int32 R = Rank + Offset[1];
		if (EGChess::IsOnBoard(F, R) && Board[EGChess::MakeSquare(F, R)] == Knight)
		{
			++Count;
		}
	}

	const uint8 King = EGChessPiece::Make(EEGChessPieceType::King, ByColor);
	for (const auto& Offset : EGChessPositionPrivate::KingOffsets)
	{
		const int32 F = File + Offset[0];
		const int32 R = Rank + Offset[1];
		if (EGChess::IsOnBoard(F, R) && Board[EGChess::MakeSquare(F, R)] == King)
		{
			++Count;
		}
	}

	const uint8 Queen = EGChessPiece::Make(EEGChessPieceType::Queen, ByColor);
	const uint8 Rook = EGChessPiece::Make(EEGChessPieceType::Rook, ByColor);
	const uint8 Bishop = EGChessPiece::Make(EEGChessPieceType::Bishop, ByColor);
	auto Slide = [&](const int32 (&Dirs)[4][2], uint8 Slider)
	{
		for (const auto& Dir : Dirs)
		{
			int32 F = File + Dir[0];
			int32 R = Rank + Dir[1];
			while (EGChess::IsOnBoard(F, R))
			{
				const uint8 Piece = Board[EGChess::MakeSquare(F, R)];
				if (Piece != 0)
				{
					if (Piece == Slider || Piece == Queen)
					{
						++Count;
					}
					break;
				}
				F += Dir[0];
				R += Dir[1];
			}
		}
	};
	Slide(EGChessPositionPrivate::RookDirs, Rook);
	Slide(EGChessPositionPrivate::BishopDirs, Bishop);
	return Count;
}

bool FEGChessPosition::IsSquareAttacked(int32 Square, EEGChessColor ByColor) const
{
	return CountAttackers(Square, ByColor) > 0;
}

bool FEGChessPosition::IsInCheck(EEGChessColor Color) const
{
	const int32 King = FindKing(Color);
	return King != INDEX_NONE && IsSquareAttacked(King, EGChess::Opposite(Color));
}

uint64 FEGChessPosition::ComputeHash() const
{
	const EGChessPositionPrivate::FZobristKeys& K = EGChessPositionPrivate::Keys();
	uint64 Hash = 0;
	for (int32 Square = 0; Square < 64; ++Square)
	{
		if (Board[Square] != 0)
		{
			Hash ^= K.Pieces[Board[Square] & 15][Square];
		}
	}
	if (SideToMove == EEGChessColor::Black)
	{
		Hash ^= K.BlackToMove;
	}
	Hash ^= K.Castling[CastlingRights & 15];

	// FIDE: an en passant square only makes positions differ when the capture is actually possible.
	if (EnPassantSquare >= 0)
	{
		const int32 File = EGChess::FileOf(EnPassantSquare);
		const int32 PawnRank = EGChess::RankOf(EnPassantSquare) + (SideToMove == EEGChessColor::White ? -1 : 1);
		const uint8 OurPawn = EGChessPiece::Make(EEGChessPieceType::Pawn, SideToMove);
		bool bCapturable = false;
		for (const int32 DF : { -1, 1 })
		{
			if (EGChess::IsOnBoard(File + DF, PawnRank) && Board[EGChess::MakeSquare(File + DF, PawnRank)] == OurPawn)
			{
				bCapturable = true;
			}
		}
		if (bCapturable)
		{
			Hash ^= K.EnPassantFile[File];
		}
	}
	return Hash;
}

float FEGChessPosition::Material(EEGChessColor Color) const
{
	float Sum = 0.0f;
	for (int32 Square = 0; Square < 64; ++Square)
	{
		if (Board[Square] != 0 && EGChessPiece::ColorOf(Board[Square]) == Color)
		{
			Sum += EGChess::PieceValue(EGChessPiece::TypeOf(Board[Square]));
		}
	}
	return Sum;
}
