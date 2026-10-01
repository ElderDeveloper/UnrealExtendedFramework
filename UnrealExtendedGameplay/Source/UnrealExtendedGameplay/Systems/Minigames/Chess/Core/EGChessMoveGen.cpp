// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGChessMoveGen.h"

namespace EGChessMoveGenPrivate
{
	const int32 KnightSteps[8][2] = { { 1, 2 }, { 2, 1 }, { 2, -1 }, { 1, -2 }, { -1, -2 }, { -2, -1 }, { -2, 1 }, { -1, 2 } };
	const int32 KingSteps[8][2] = { { 1, 0 }, { 1, 1 }, { 0, 1 }, { -1, 1 }, { -1, 0 }, { -1, -1 }, { 0, -1 }, { 1, -1 } };
	const int32 OrthogonalDirs[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
	const int32 DiagonalDirs[4][2] = { { 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 } };

	void AddPawnMove(TArray<FEGChessMove>& Out, int32 From, int32 To, uint8 Flags, bool bPromotes)
	{
		if (bPromotes)
		{
			for (const EEGChessPieceType Promotion : { EEGChessPieceType::Queen, EEGChessPieceType::Rook, EEGChessPieceType::Bishop, EEGChessPieceType::Knight })
			{
				Out.Emplace(From, To, Promotion, static_cast<uint8>(Flags | EGChessMoveFlags::Promotion));
			}
		}
		else
		{
			Out.Emplace(From, To, EEGChessPieceType::None, Flags);
		}
	}

	void GeneratePieceMoves(const FEGChessPosition& Position, int32 From, TArray<FEGChessMove>& Out)
	{
		const uint8 Piece = Position.Board[From];
		if (Piece == 0 || EGChessPiece::ColorOf(Piece) != Position.SideToMove)
		{
			return;
		}

		const EEGChessColor Us = Position.SideToMove;
		const EEGChessColor Them = EGChess::Opposite(Us);
		const int32 File = EGChess::FileOf(From);
		const int32 Rank = EGChess::RankOf(From);

		auto TryStep = [&](int32 F, int32 R)
		{
			if (!EGChess::IsOnBoard(F, R))
			{
				return;
			}
			const int32 To = EGChess::MakeSquare(F, R);
			const uint8 Target = Position.Board[To];
			if (Target == 0)
			{
				Out.Emplace(From, To);
			}
			else if (EGChessPiece::ColorOf(Target) == Them)
			{
				Out.Emplace(From, To, EEGChessPieceType::None, EGChessMoveFlags::Capture);
			}
		};

		auto Slide = [&](const int32 (&Dirs)[4][2])
		{
			for (const auto& Dir : Dirs)
			{
				int32 F = File + Dir[0];
				int32 R = Rank + Dir[1];
				while (EGChess::IsOnBoard(F, R))
				{
					const int32 To = EGChess::MakeSquare(F, R);
					const uint8 Target = Position.Board[To];
					if (Target == 0)
					{
						Out.Emplace(From, To);
					}
					else
					{
						if (EGChessPiece::ColorOf(Target) == Them)
						{
							Out.Emplace(From, To, EEGChessPieceType::None, EGChessMoveFlags::Capture);
						}
						break;
					}
					F += Dir[0];
					R += Dir[1];
				}
			}
		};

		switch (EGChessPiece::TypeOf(Piece))
		{
		case EEGChessPieceType::Pawn:
		{
			const int32 Forward = Us == EEGChessColor::White ? 1 : -1;
			const int32 StartRank = Us == EEGChessColor::White ? 1 : 6;
			const int32 LastRank = Us == EEGChessColor::White ? 7 : 0;

			const int32 OneRank = Rank + Forward;
			if (EGChess::IsOnBoard(File, OneRank) && Position.Board[EGChess::MakeSquare(File, OneRank)] == 0)
			{
				AddPawnMove(Out, From, EGChess::MakeSquare(File, OneRank), 0, OneRank == LastRank);
				const int32 TwoRank = Rank + 2 * Forward;
				if (Rank == StartRank && Position.Board[EGChess::MakeSquare(File, TwoRank)] == 0)
				{
					Out.Emplace(From, EGChess::MakeSquare(File, TwoRank), EEGChessPieceType::None, EGChessMoveFlags::DoublePush);
				}
			}

			for (const int32 DF : { -1, 1 })
			{
				const int32 F = File + DF;
				if (!EGChess::IsOnBoard(F, OneRank))
				{
					continue;
				}
				const int32 To = EGChess::MakeSquare(F, OneRank);
				const uint8 Target = Position.Board[To];
				if (Target != 0 && EGChessPiece::ColorOf(Target) == Them)
				{
					AddPawnMove(Out, From, To, EGChessMoveFlags::Capture, OneRank == LastRank);
				}
				else if (To == Position.EnPassantSquare)
				{
					Out.Emplace(From, To, EEGChessPieceType::None, static_cast<uint8>(EGChessMoveFlags::Capture | EGChessMoveFlags::EnPassant));
				}
			}
			break;
		}
		case EEGChessPieceType::Knight:
			for (const auto& Step : KnightSteps)
			{
				TryStep(File + Step[0], Rank + Step[1]);
			}
			break;
		case EEGChessPieceType::Bishop:
			Slide(DiagonalDirs);
			break;
		case EEGChessPieceType::Rook:
			Slide(OrthogonalDirs);
			break;
		case EEGChessPieceType::Queen:
			Slide(OrthogonalDirs);
			Slide(DiagonalDirs);
			break;
		case EEGChessPieceType::King:
		{
			for (const auto& Step : KingSteps)
			{
				TryStep(File + Step[0], Rank + Step[1]);
			}

			// Castling: rights held, path empty, and the king neither starts, crosses nor lands in check.
			const int32 HomeRank = Us == EEGChessColor::White ? 0 : 7;
			if (From != EGChess::MakeSquare(4, HomeRank) || Position.IsSquareAttacked(From, Them))
			{
				break;
			}
			const uint8 KingSide = Us == EEGChessColor::White ? EGChessCastling::WhiteKingSide : EGChessCastling::BlackKingSide;
			const uint8 QueenSide = Us == EEGChessColor::White ? EGChessCastling::WhiteQueenSide : EGChessCastling::BlackQueenSide;
			const uint8 OurRook = EGChessPiece::Make(EEGChessPieceType::Rook, Us);

			if ((Position.CastlingRights & KingSide)
				&& Position.Board[EGChess::MakeSquare(7, HomeRank)] == OurRook
				&& Position.Board[EGChess::MakeSquare(5, HomeRank)] == 0
				&& Position.Board[EGChess::MakeSquare(6, HomeRank)] == 0
				&& !Position.IsSquareAttacked(EGChess::MakeSquare(5, HomeRank), Them)
				&& !Position.IsSquareAttacked(EGChess::MakeSquare(6, HomeRank), Them))
			{
				Out.Emplace(From, EGChess::MakeSquare(6, HomeRank), EEGChessPieceType::None, EGChessMoveFlags::CastleKingSide);
			}
			if ((Position.CastlingRights & QueenSide)
				&& Position.Board[EGChess::MakeSquare(0, HomeRank)] == OurRook
				&& Position.Board[EGChess::MakeSquare(1, HomeRank)] == 0
				&& Position.Board[EGChess::MakeSquare(2, HomeRank)] == 0
				&& Position.Board[EGChess::MakeSquare(3, HomeRank)] == 0
				&& !Position.IsSquareAttacked(EGChess::MakeSquare(3, HomeRank), Them)
				&& !Position.IsSquareAttacked(EGChess::MakeSquare(2, HomeRank), Them))
			{
				Out.Emplace(From, EGChess::MakeSquare(2, HomeRank), EEGChessPieceType::None, EGChessMoveFlags::CastleQueenSide);
			}
			break;
		}
		default:
			break;
		}
	}

	void FilterLegal(const FEGChessPosition& Position, TArray<FEGChessMove>& InOutMoves)
	{
		FEGChessPosition Scratch = Position;
		const EEGChessColor Us = Position.SideToMove;
		for (int32 Index = InOutMoves.Num() - 1; Index >= 0; --Index)
		{
			FEGChessUndo Undo;
			Scratch.MakeMove(InOutMoves[Index], Undo);
			const bool bLeavesKingInCheck = Scratch.IsInCheck(Us);
			Scratch.UnmakeMove(InOutMoves[Index], Undo);
			if (bLeavesKingInCheck)
			{
				InOutMoves.RemoveAt(Index, EAllowShrinking::No);
			}
		}
	}
}

namespace EGChessMoveGen
{
	void GeneratePseudoLegal(const FEGChessPosition& Position, TArray<FEGChessMove>& OutMoves)
	{
		OutMoves.Reset();
		for (int32 Square = 0; Square < 64; ++Square)
		{
			EGChessMoveGenPrivate::GeneratePieceMoves(Position, Square, OutMoves);
		}
	}

	void GenerateLegal(const FEGChessPosition& Position, TArray<FEGChessMove>& OutMoves)
	{
		GeneratePseudoLegal(Position, OutMoves);
		EGChessMoveGenPrivate::FilterLegal(Position, OutMoves);
	}

	void GenerateLegalFrom(const FEGChessPosition& Position, int32 FromSquare, TArray<FEGChessMove>& OutMoves)
	{
		OutMoves.Reset();
		if (EGChess::IsValidSquare(FromSquare))
		{
			EGChessMoveGenPrivate::GeneratePieceMoves(Position, FromSquare, OutMoves);
			EGChessMoveGenPrivate::FilterLegal(Position, OutMoves);
		}
	}

	bool HasAnyLegalMove(const FEGChessPosition& Position)
	{
		TArray<FEGChessMove> Moves;
		for (int32 Square = 0; Square < 64; ++Square)
		{
			if (!Position.HasColorAt(Square, Position.SideToMove))
			{
				continue;
			}
			GenerateLegalFrom(Position, Square, Moves);
			if (Moves.Num() > 0)
			{
				return true;
			}
		}
		return false;
	}
}
