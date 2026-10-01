// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGChessNotation.h"

#include "EGChessMoveGen.h"
#include "EGChessRules.h"

namespace EGChessNotation
{
	FString ToSAN(const FEGChessPosition& Position, const FEGChessMove& Move)
	{
		FString San;
		const EEGChessPieceType Type = Position.TypeAt(Move.From);

		if (Move.HasFlag(EGChessMoveFlags::CastleKingSide))
		{
			San = TEXT("O-O");
		}
		else if (Move.HasFlag(EGChessMoveFlags::CastleQueenSide))
		{
			San = TEXT("O-O-O");
		}
		else
		{
			if (Type == EEGChessPieceType::Pawn)
			{
				if (Move.IsCapture())
				{
					San.AppendChar(static_cast<TCHAR>(TEXT('a') + EGChess::FileOf(Move.From)));
				}
			}
			else
			{
				San += EGChess::PieceLetter(Type);

				// Other pieces of the same type that could also reach the target.
				TArray<FEGChessMove> Legal;
				EGChessMoveGen::GenerateLegal(Position, Legal);
				bool bAmbiguous = false;
				bool bSameFile = false;
				bool bSameRank = false;
				for (const FEGChessMove& Other : Legal)
				{
					if (Other.To != Move.To || Other.From == Move.From || Position.TypeAt(Other.From) != Type)
					{
						continue;
					}
					bAmbiguous = true;
					bSameFile |= EGChess::FileOf(Other.From) == EGChess::FileOf(Move.From);
					bSameRank |= EGChess::RankOf(Other.From) == EGChess::RankOf(Move.From);
				}
				if (bAmbiguous)
				{
					if (!bSameFile)
					{
						San.AppendChar(static_cast<TCHAR>(TEXT('a') + EGChess::FileOf(Move.From)));
					}
					else if (!bSameRank)
					{
						San.AppendChar(static_cast<TCHAR>(TEXT('1') + EGChess::RankOf(Move.From)));
					}
					else
					{
						San += EGChess::SquareName(Move.From);
					}
				}
			}

			if (Move.IsCapture())
			{
				San.AppendChar(TEXT('x'));
			}
			San += EGChess::SquareName(Move.To);

			if (Move.Promotion != EEGChessPieceType::None)
			{
				San.AppendChar(TEXT('='));
				San += EGChess::PieceLetter(Move.Promotion);
			}
		}

		FEGChessPosition After = Position;
		After.ApplyMove(Move);
		if (After.IsInCheck(After.SideToMove))
		{
			San.AppendChar(EGChessMoveGen::HasAnyLegalMove(After) ? TEXT('+') : TEXT('#'));
		}
		return San;
	}

	FString ToUCI(const FEGChessMove& Move)
	{
		FString Uci = EGChess::SquareName(Move.From) + EGChess::SquareName(Move.To);
		switch (Move.Promotion)
		{
		case EEGChessPieceType::Queen: Uci.AppendChar(TEXT('q')); break;
		case EEGChessPieceType::Rook: Uci.AppendChar(TEXT('r')); break;
		case EEGChessPieceType::Bishop: Uci.AppendChar(TEXT('b')); break;
		case EEGChessPieceType::Knight: Uci.AppendChar(TEXT('n')); break;
		default: break;
		}
		return Uci;
	}

	bool FromUCI(const FEGChessPosition& Position, const FString& Uci, FEGChessMove& OutMove)
	{
		if (Uci.Len() < 4)
		{
			return false;
		}
		const int32 From = EGChess::ParseSquare(Uci.Mid(0, 2));
		const int32 To = EGChess::ParseSquare(Uci.Mid(2, 2));
		if (From == INDEX_NONE || To == INDEX_NONE)
		{
			return false;
		}
		EEGChessPieceType Promotion = EEGChessPieceType::None;
		if (Uci.Len() >= 5)
		{
			switch (FChar::ToLower(Uci[4]))
			{
			case TEXT('q'): Promotion = EEGChessPieceType::Queen; break;
			case TEXT('r'): Promotion = EEGChessPieceType::Rook; break;
			case TEXT('b'): Promotion = EEGChessPieceType::Bishop; break;
			case TEXT('n'): Promotion = EEGChessPieceType::Knight; break;
			default: return false;
			}
		}
		return EGChessRules::FindLegalMove(Position, From, To, Promotion, OutMove);
	}
}
