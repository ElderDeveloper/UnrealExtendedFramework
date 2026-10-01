// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EGChessPosition.h"

/** SAN and UCI. FEN lives on FEGChessPosition itself. */
namespace EGChessNotation
{
	/**
	 * Standard algebraic notation for a legal move in Position (the position before the move):
	 * "Nf3", "exd5", "O-O", "e8=Q+", "Qxf7#". Disambiguates by file, then rank, then both.
	 */
	UNREALEXTENDEDGAMEPLAY_API FString ToSAN(const FEGChessPosition& Position, const FEGChessMove& Move);

	/** "g1f3", "e7e8q". */
	UNREALEXTENDEDGAMEPLAY_API FString ToUCI(const FEGChessMove& Move);

	/** Parses UCI and resolves it against the legal moves of Position. */
	UNREALEXTENDEDGAMEPLAY_API bool FromUCI(const FEGChessPosition& Position, const FString& Uci, FEGChessMove& OutMove);
}
