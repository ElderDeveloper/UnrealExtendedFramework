// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EGChessPosition.h"

/**
 * Game-end rules and move resolution on top of move generation.
 *
 * Threefold repetition and the fifty-move rule are applied automatically when bAutomaticDrawClaims
 * is true (the default). FIDE makes both claims; a game is friendlier applying them.
 */
namespace EGChessRules
{
	/**
	 * Status of Position. RepetitionHistory holds the hashes of every earlier position since the
	 * last irreversible move, oldest first, not including Position itself.
	 */
	UNREALEXTENDEDGAMEPLAY_API EEGChessStatus Evaluate(const FEGChessPosition& Position, TConstArrayView<uint64> RepetitionHistory, bool bAutomaticDrawClaims = true);

	/** Neither side can ever mate: K v K, K+minor v K, K+B v K+B with same-coloured bishops. */
	UNREALEXTENDEDGAMEPLAY_API bool IsInsufficientMaterial(const FEGChessPosition& Position);

	/**
	 * Whether Color still has material that could mate. Used when the opponent's flag falls:
	 * a lone king, or king and one minor piece, cannot, so the timeout is a draw.
	 */
	UNREALEXTENDEDGAMEPLAY_API bool HasMatingMaterial(const FEGChessPosition& Position, EEGChessColor Color);

	/**
	 * Resolve a requested move against the legal moves of Position and fill in its flags.
	 * A pawn move to the last rank without a promotion piece does not resolve.
	 */
	UNREALEXTENDEDGAMEPLAY_API bool FindLegalMove(const FEGChessPosition& Position, int32 From, int32 To, EEGChessPieceType Promotion, FEGChessMove& OutMove);

	/** True when a legal move from From to To exists that needs a promotion choice. */
	UNREALEXTENDEDGAMEPLAY_API bool NeedsPromotionChoice(const FEGChessPosition& Position, int32 From, int32 To);

	/** Counts every legal move sequence to Depth. The standard proof of a move generator. */
	UNREALEXTENDEDGAMEPLAY_API uint64 Perft(const FEGChessPosition& Position, int32 Depth);

	/** Map a rules status to a result for the side that was to move. */
	UNREALEXTENDEDGAMEPLAY_API FEGChessResult ResultFromStatus(EEGChessStatus Status, EEGChessColor SideToMove);
}
