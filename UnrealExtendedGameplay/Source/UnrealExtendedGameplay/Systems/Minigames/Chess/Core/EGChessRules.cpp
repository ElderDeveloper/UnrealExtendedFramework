// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGChessRules.h"

#include "EGChessMoveGen.h"

namespace EGChessRules
{
	EEGChessStatus Evaluate(const FEGChessPosition& Position, TConstArrayView<uint64> RepetitionHistory, bool bAutomaticDrawClaims)
	{
		if (!EGChessMoveGen::HasAnyLegalMove(Position))
		{
			return Position.IsInCheck(Position.SideToMove) ? EEGChessStatus::Checkmate : EEGChessStatus::Stalemate;
		}

		if (IsInsufficientMaterial(Position))
		{
			return EEGChessStatus::InsufficientMaterial;
		}

		if (bAutomaticDrawClaims)
		{
			if (Position.HalfmoveClock >= 100)
			{
				return EEGChessStatus::FiftyMoveRule;
			}

			const uint64 Hash = Position.ComputeHash();
			int32 Seen = 1;
			for (const uint64 Earlier : RepetitionHistory)
			{
				if (Earlier == Hash && ++Seen >= 3)
				{
					return EEGChessStatus::ThreefoldRepetition;
				}
			}
		}

		return EEGChessStatus::Ongoing;
	}

	bool IsInsufficientMaterial(const FEGChessPosition& Position)
	{
		int32 Minors[2] = { 0, 0 };
		int32 BishopSquareColor[2] = { -1, -1 };
		bool bBishopsOnMixedColors = false;

		for (int32 Square = 0; Square < 64; ++Square)
		{
			const uint8 Piece = Position.Board[Square];
			if (Piece == 0)
			{
				continue;
			}
			const EEGChessPieceType Type = EGChessPiece::TypeOf(Piece);
			const int32 Side = EGChess::ColorIndex(EGChessPiece::ColorOf(Piece));
			switch (Type)
			{
			case EEGChessPieceType::King:
				break;
			case EEGChessPieceType::Knight:
				++Minors[Side];
				break;
			case EEGChessPieceType::Bishop:
			{
				++Minors[Side];
				const int32 SquareColor = (EGChess::FileOf(Square) + EGChess::RankOf(Square)) & 1;
				if (BishopSquareColor[Side] != -1 && BishopSquareColor[Side] != SquareColor)
				{
					bBishopsOnMixedColors = true;
				}
				BishopSquareColor[Side] = SquareColor;
				break;
			}
			default:
				return false; // any pawn, rook or queen
			}
		}

		if (Minors[0] > 1 || Minors[1] > 1 || bBishopsOnMixedColors)
		{
			return false;
		}
		if (Minors[0] + Minors[1] <= 1)
		{
			return true; // K v K, K+minor v K
		}

		// One minor each: only K+B v K+B with bishops on the same colour is dead.
		const bool bBothBishops = BishopSquareColor[0] != -1 && BishopSquareColor[1] != -1;
		return bBothBishops && BishopSquareColor[0] == BishopSquareColor[1];
	}

	bool HasMatingMaterial(const FEGChessPosition& Position, EEGChessColor Color)
	{
		int32 Minors = 0;
		for (int32 Square = 0; Square < 64; ++Square)
		{
			const uint8 Piece = Position.Board[Square];
			if (Piece == 0 || EGChessPiece::ColorOf(Piece) != Color)
			{
				continue;
			}
			switch (EGChessPiece::TypeOf(Piece))
			{
			case EEGChessPieceType::Pawn:
			case EEGChessPieceType::Rook:
			case EEGChessPieceType::Queen:
				return true;
			case EEGChessPieceType::Knight:
			case EEGChessPieceType::Bishop:
				++Minors;
				break;
			default:
				break;
			}
		}
		return Minors >= 2;
	}

	bool FindLegalMove(const FEGChessPosition& Position, int32 From, int32 To, EEGChessPieceType Promotion, FEGChessMove& OutMove)
	{
		TArray<FEGChessMove> Moves;
		EGChessMoveGen::GenerateLegalFrom(Position, From, Moves);
		for (const FEGChessMove& Move : Moves)
		{
			if (Move.To == To && Move.Promotion == Promotion)
			{
				OutMove = Move;
				return true;
			}
		}
		return false;
	}

	bool NeedsPromotionChoice(const FEGChessPosition& Position, int32 From, int32 To)
	{
		TArray<FEGChessMove> Moves;
		EGChessMoveGen::GenerateLegalFrom(Position, From, Moves);
		return Moves.ContainsByPredicate([To](const FEGChessMove& Move)
		{
			return Move.To == To && Move.Promotion != EEGChessPieceType::None;
		});
	}

	uint64 Perft(const FEGChessPosition& Position, int32 Depth)
	{
		if (Depth <= 0)
		{
			return 1;
		}
		TArray<FEGChessMove> Moves;
		EGChessMoveGen::GenerateLegal(Position, Moves);
		if (Depth == 1)
		{
			return Moves.Num();
		}
		uint64 Nodes = 0;
		FEGChessPosition Scratch = Position;
		for (const FEGChessMove& Move : Moves)
		{
			FEGChessUndo Undo;
			Scratch.MakeMove(Move, Undo);
			Nodes += Perft(Scratch, Depth - 1);
			Scratch.UnmakeMove(Move, Undo);
		}
		return Nodes;
	}

	FEGChessResult ResultFromStatus(EEGChessStatus Status, EEGChessColor SideToMove)
	{
		FEGChessResult Result;
		switch (Status)
		{
		case EEGChessStatus::Checkmate:
			Result.Result = SideToMove == EEGChessColor::White ? EEGChessGameResult::BlackWins : EEGChessGameResult::WhiteWins;
			Result.Reason = EEGChessEndReason::Checkmate;
			break;
		case EEGChessStatus::Stalemate:
			Result.Result = EEGChessGameResult::Draw;
			Result.Reason = EEGChessEndReason::Stalemate;
			break;
		case EEGChessStatus::ThreefoldRepetition:
			Result.Result = EEGChessGameResult::Draw;
			Result.Reason = EEGChessEndReason::ThreefoldRepetition;
			break;
		case EEGChessStatus::FiftyMoveRule:
			Result.Result = EEGChessGameResult::Draw;
			Result.Reason = EEGChessEndReason::FiftyMoveRule;
			break;
		case EEGChessStatus::InsufficientMaterial:
			Result.Result = EEGChessGameResult::Draw;
			Result.Reason = EEGChessEndReason::InsufficientMaterial;
			break;
		default:
			break;
		}
		return Result;
	}
}
