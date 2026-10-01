// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EGChessPosition.h"

/**
 * Move generation.
 *
 * Pseudo-legal moves obey how pieces move and ignore whether the mover's king is left in check.
 * Legal moves are the pseudo-legal ones that survive make-and-test: make the move, reject it if
 * the mover's king is attacked, unmake it. Slower than pin detection, and much harder to get
 * wrong — en passant's horizontal pin, for one, needs no special case.
 *
 * Every generated move carries its flags, so MakeMove/UnmakeMove never have to re-derive them.
 */
namespace EGChessMoveGen
{
	UNREALEXTENDEDGAMEPLAY_API void GeneratePseudoLegal(const FEGChessPosition& Position, TArray<FEGChessMove>& OutMoves);
	UNREALEXTENDEDGAMEPLAY_API void GenerateLegal(const FEGChessPosition& Position, TArray<FEGChessMove>& OutMoves);

	/** Legal moves of the piece on one square only. */
	UNREALEXTENDEDGAMEPLAY_API void GenerateLegalFrom(const FEGChessPosition& Position, int32 FromSquare, TArray<FEGChessMove>& OutMoves);

	UNREALEXTENDEDGAMEPLAY_API bool HasAnyLegalMove(const FEGChessPosition& Position);
}
