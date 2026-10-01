// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGChessAI.h"

#include "EGChessAI_RuleBased.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Core/EGChessMoveGen.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Data/EGChessAIProfile.h"

// ---------------------------------------------------------------------------------------------
// Base
// ---------------------------------------------------------------------------------------------

FEGChessMove UEGChessAI::ChooseMove(const FEGChessPosition& Position, const UEGChessAIProfile& Profile, FRandomStream& Random)
{
	TArray<FEGChessMove> Moves;
	EGChessMoveGen::GenerateLegal(Position, Moves);
	return Moves.Num() > 0 ? Moves[Random.RandHelper(Moves.Num())] : FEGChessMove();
}

bool UEGChessAI::RespondToDrawOffer(const FEGChessPosition& Position, const UEGChessAIProfile& Profile, FRandomStream& Random)
{
	return false;
}

float UEGChessAI::GetThinkDelay(const FEGChessPosition& Position, const UEGChessAIProfile& Profile, FRandomStream& Random) const
{
	TArray<FEGChessMove> Moves;
	EGChessMoveGen::GenerateLegal(Position, Moves);
	const float Min = FMath::Max(0.0f, static_cast<float>(Profile.ThinkDelayRange.X));
	const float Max = FMath::Max(Min, static_cast<float>(Profile.ThinkDelayRange.Y));
	return Random.FRandRange(Min, Max) + Moves.Num() * Profile.ThinkDelayPerLegalMove;
}

// ---------------------------------------------------------------------------------------------
// Rule based
// ---------------------------------------------------------------------------------------------

namespace EGChessAIRuleBasedPrivate
{
	bool IsCentre(int32 Square)
	{
		const int32 File = EGChess::FileOf(Square);
		const int32 Rank = EGChess::RankOf(Square);
		return (File == 3 || File == 4) && (Rank == 3 || Rank == 4);
	}

	bool IsExtendedCentre(int32 Square)
	{
		const int32 File = EGChess::FileOf(Square);
		const int32 Rank = EGChess::RankOf(Square);
		return File >= 2 && File <= 5 && Rank >= 2 && Rank <= 5;
	}

	int32 CountPieces(const FEGChessPosition& Position)
	{
		int32 Count = 0;
		for (int32 Square = 0; Square < 64; ++Square)
		{
			Count += Position.Board[Square] != 0 ? 1 : 0;
		}
		return Count;
	}
}

FEGChessMove UEGChessAI_RuleBased::ChooseMove(const FEGChessPosition& Position, const UEGChessAIProfile& Profile, FRandomStream& Random)
{
	TArray<FEGChessMove> Moves;
	EGChessMoveGen::GenerateLegal(Position, Moves);
	if (Moves.Num() == 0)
	{
		return FEGChessMove();
	}

	const bool bCheckHanging = Random.FRand() >= Profile.SkipHangingCheckChance;

	int32 BestIndex = 0;
	float BestScore = -MAX_flt;
	for (int32 Index = 0; Index < Moves.Num(); ++Index)
	{
		const float Score = ScoreMove(Position, Moves[Index], Profile, bCheckHanging, Random);
		if (Score > BestScore)
		{
			BestScore = Score;
			BestIndex = Index;
		}
	}
	return Moves[BestIndex];
}

bool UEGChessAI_RuleBased::RespondToDrawOffer(const FEGChessPosition& Position, const UEGChessAIProfile& Profile, FRandomStream& Random)
{
	const EEGChessColor Us = Position.SideToMove;
	const float Balance = Position.Material(Us) - Position.Material(EGChess::Opposite(Us));
	if (Balance < -Profile.AcceptDrawWhenBehindBy)
	{
		return true;
	}
	return FMath::Abs(Balance) < 1.0f
		&& Position.FullmoveNumber >= Profile.AcceptEqualDrawAfterMove
		&& EGChessAIRuleBasedPrivate::CountPieces(Position) <= Profile.AcceptEqualDrawMaxPieces;
}

bool UEGChessAI_RuleBased::IsMate(const FEGChessPosition& After)
{
	return After.IsInCheck(After.SideToMove) && !EGChessMoveGen::HasAnyLegalMove(After);
}

float UEGChessAI_RuleBased::BestImmediateGain(const FEGChessPosition& After)
{
	TArray<FEGChessMove> Replies;
	EGChessMoveGen::GenerateLegal(After, Replies);
	const EEGChessColor Them = After.SideToMove;
	const EEGChessColor Us = EGChess::Opposite(Them);

	float Best = 0.0f;
	FEGChessPosition Scratch = After;
	for (const FEGChessMove& Reply : Replies)
	{
		if (!Reply.IsCapture())
		{
			continue;
		}
		const int32 CapturedSquare = Reply.HasFlag(EGChessMoveFlags::EnPassant) ? Reply.To + (Them == EEGChessColor::White ? -8 : 8) : Reply.To;
		const float Captured = EGChess::PieceValue(After.TypeAt(CapturedSquare));
		const float Attacker = EGChess::PieceValue(After.TypeAt(Reply.From));

		FEGChessUndo Undo;
		Scratch.MakeMove(Reply, Undo);
		const bool bRecaptured = Scratch.IsSquareAttacked(Reply.To, Us);
		Scratch.UnmakeMove(Reply, Undo);

		Best = FMath::Max(Best, Captured - (bRecaptured ? Attacker : 0.0f));
	}
	return Best;
}

float UEGChessAI_RuleBased::ScoreMove(const FEGChessPosition& Position, const FEGChessMove& Move, const UEGChessAIProfile& Profile, bool bCheckHanging, FRandomStream& Random) const
{
	using namespace EGChessAIRuleBasedPrivate;

	const FEGChessAIWeights& W = Profile.Weights;
	const EEGChessColor Us = Position.SideToMove;
	const EEGChessColor Them = EGChess::Opposite(Us);
	const EEGChessPieceType Mover = Position.TypeAt(Move.From);
	const float MoverValue = EGChess::PieceValue(Mover);

	const int32 CapturedSquare = Move.HasFlag(EGChessMoveFlags::EnPassant) ? Move.To + (Us == EEGChessColor::White ? -8 : 8) : Move.To;
	const float CapturedValue = Move.IsCapture() ? EGChess::PieceValue(Position.TypeAt(CapturedSquare)) : 0.0f;

	const bool bWasAttacked = Position.IsSquareAttacked(Move.From, Them);
	const bool bWasDefended = Position.CountAttackers(Move.From, Us) > 0;

	FEGChessPosition After = Position;
	After.ApplyMove(Move);

	// Mate in one is always taken, unless the profile makes this AI overlook it.
	if (IsMate(After) && Random.FRand() >= Profile.MissMateChance)
	{
		return 100000.0f;
	}

	float Score = 0.0f;

	// Material won, less the mover if the target is defended.
	const bool bTargetDefended = After.IsSquareAttacked(Move.To, Them);
	const bool bTargetSupported = After.CountAttackers(Move.To, Us) > 0;
	Score += CapturedValue * W.Material;

	if (bCheckHanging)
	{
		if (bTargetDefended && (!bTargetSupported || MoverValue > CapturedValue))
		{
			Score -= MoverValue * W.HangingPenalty;
		}

		// Any other piece of ours left attacked and undefended.
		for (int32 Square = 0; Square < 64; ++Square)
		{
			if (Square == Move.To || !After.HasColorAt(Square, Us))
			{
				continue;
			}
			const EEGChessPieceType Type = After.TypeAt(Square);
			if (Type == EEGChessPieceType::King || Type == EEGChessPieceType::Pawn)
			{
				continue;
			}
			if (After.IsSquareAttacked(Square, Them) && After.CountAttackers(Square, Us) == 0)
			{
				Score -= EGChess::PieceValue(Type) * W.HangingPenalty * 0.5f;
			}
		}

		// Rescuing an attacked piece.
		if (bWasAttacked && (!bWasDefended || MoverValue > 3.0f) && !bTargetDefended)
		{
			Score += MoverValue * W.Rescue;
		}
	}

	if (Move.Promotion == EEGChessPieceType::Queen)
	{
		Score += W.Promotion;
	}
	else if (Move.Promotion != EEGChessPieceType::None)
	{
		Score += W.Promotion * 0.2f;
	}

	if (After.IsInCheck(Them))
	{
		Score += W.Check;
	}

	if (Move.IsCastle())
	{
		Score += W.Castling;
	}

	const bool bOpening = Position.FullmoveNumber <= 12;
	const int32 HomeRank = Us == EEGChessColor::White ? 0 : 7;
	if (bOpening)
	{
		if ((Mover == EEGChessPieceType::Knight || Mover == EEGChessPieceType::Bishop) && EGChess::RankOf(Move.From) == HomeRank)
		{
			Score += W.Development;
		}
		if (Mover == EEGChessPieceType::Queen && Position.FullmoveNumber <= 6)
		{
			Score -= W.Development * 0.75f;
		}
		if (Mover == EEGChessPieceType::King && !Move.IsCastle())
		{
			Score -= W.KingSafety * 2.0f;
		}
	}

	if (Mover == EEGChessPieceType::Pawn || Mover == EEGChessPieceType::Knight)
	{
		if (IsCentre(Move.To))
		{
			Score += W.Centre;
		}
		else if (IsExtendedCentre(Move.To))
		{
			Score += W.Centre * 0.5f;
		}
	}

	// Pushing the pawns in front of a castled king.
	if (Mover == EEGChessPieceType::Pawn)
	{
		const int32 King = Position.FindKing(Us);
		if (King != INDEX_NONE && EGChess::RankOf(King) == HomeRank)
		{
			const int32 KingFile = EGChess::FileOf(King);
			const bool bCastledShape = KingFile >= 6 || KingFile <= 2;
			if (bCastledShape && FMath::Abs(EGChess::FileOf(Move.From) - KingFile) <= 1)
			{
				Score -= W.KingSafety;
			}
		}
	}

	// One-move lookahead: refuse walking into mate or handing over material.
	if (Profile.bOneMoveLookahead)
	{
		TArray<FEGChessMove> Replies;
		EGChessMoveGen::GenerateLegal(After, Replies);
		FEGChessPosition Scratch = After;
		for (const FEGChessMove& Reply : Replies)
		{
			FEGChessUndo Undo;
			Scratch.MakeMove(Reply, Undo);
			const bool bMated = IsMate(Scratch);
			Scratch.UnmakeMove(Reply, Undo);
			if (bMated)
			{
				Score -= 50000.0f;
				break;
			}
		}
		Score -= BestImmediateGain(After) * W.Material;
	}

	if (Profile.Randomness > 0.0f)
	{
		Score += Random.FRandRange(-Profile.Randomness, Profile.Randomness);
	}
	return Score;
}
