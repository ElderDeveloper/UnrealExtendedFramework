// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGChessBoardFeedback.h"

namespace EGChessBoardFeedback
{
	void Build(const FEGChessFeedbackInput& Input, FEGChessSquareMarks& OutMarks)
	{
		for (uint16& Flags : OutMarks)
		{
			Flags = 0;
		}

		if (Input.Position == nullptr || Input.Options.bHardcore)
		{
			return;
		}

		const FEGChessPosition& Position = *Input.Position;
		auto Mark = [&OutMarks](int32 Square, EEGChessMarkType Type)
		{
			if (EGChess::IsValidSquare(Square))
			{
				OutMarks[Square] |= EGChessMarks::Bit(Type);
			}
		};

		const bool bPlaying = Input.Phase == EEGChessPhase::Playing;
		const bool bOver = Input.Phase == EEGChessPhase::GameOver || Input.Phase == EEGChessPhase::WaitingForOpponent;

		// Shared marks.
		if (Input.Options.bShowLastMove && Input.bHasLastMove && (bPlaying || bOver))
		{
			Mark(Input.LastMove.From, EEGChessMarkType::LastMoveFrom);
			Mark(Input.LastMove.To, EEGChessMarkType::LastMoveTo);
		}

		if (Input.Options.bShowCheck)
		{
			if (bPlaying && Position.IsInCheck(Position.SideToMove))
			{
				Mark(Position.FindKing(Position.SideToMove), EEGChessMarkType::Check);
			}
			else if (bOver && Input.Result.Reason == EEGChessEndReason::Checkmate)
			{
				Mark(Position.FindKing(EGChess::Opposite(Input.Result.GetWinner())), EEGChessMarkType::Check);
			}
		}

		// Local marks: only for a seated viewer, only while playing.
		if (!Input.bLocalSeated || !bPlaying)
		{
			return;
		}

		if (EGChess::IsValidSquare(Input.SelectedSquare) || Input.Options.bShowCursorWithoutSelection)
		{
			Mark(Input.CursorSquare, EEGChessMarkType::Cursor);
		}
		Mark(Input.IllegalSquare, EEGChessMarkType::Illegal);

		if (!EGChess::IsValidSquare(Input.SelectedSquare))
		{
			return;
		}
		Mark(Input.SelectedSquare, EEGChessMarkType::Selected);

		if (!Input.Options.bShowLegalMoves)
		{
			return;
		}

		for (const FEGChessMove& Move : Input.SelectedMoves)
		{
			if (Move.IsCapture())
			{
				Mark(Move.To, EEGChessMarkType::Capture);
				if (Move.HasFlag(EGChessMoveFlags::EnPassant))
				{
					const EEGChessColor Mover = EGChessPiece::ColorOf(Position.Get(Move.From));
					Mark(Move.To + (Mover == EEGChessColor::White ? -8 : 8), EEGChessMarkType::Capture);
				}
			}
			else
			{
				Mark(Move.To, EEGChessMarkType::Move);
			}

			if (Move.HasFlag(EGChessMoveFlags::CastleKingSide))
			{
				Mark(Move.To + 1, EEGChessMarkType::CastlePartner);
			}
			else if (Move.HasFlag(EGChessMoveFlags::CastleQueenSide))
			{
				Mark(Move.To - 2, EEGChessMarkType::CastlePartner);
			}

			if (Move.To == Input.CursorSquare)
			{
				Mark(Move.To, EEGChessMarkType::HoverTarget);
			}
		}
	}
}
