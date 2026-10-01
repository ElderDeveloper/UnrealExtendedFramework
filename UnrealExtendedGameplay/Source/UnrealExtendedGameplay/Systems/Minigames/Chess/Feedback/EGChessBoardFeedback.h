// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "Containers/StaticArray.h"
#include "CoreMinimal.h"
#include "EGChessFeedbackTypes.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Core/EGChessPosition.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Table/EGChessMatchTypes.h"

using FEGChessSquareMarks = TStaticArray<uint16, 64>;

/** Everything that decides the marks on the board for one viewer. */
struct FEGChessFeedbackInput
{
	const FEGChessPosition* Position = nullptr;
	EEGChessPhase Phase = EEGChessPhase::Idle;
	FEGChessResult Result;

	/** The viewer is seated at this table (local marks only exist for them). */
	bool bLocalSeated = false;
	EEGChessColor LocalColor = EEGChessColor::White;

	int32 CursorSquare = INDEX_NONE;
	int32 SelectedSquare = INDEX_NONE;
	int32 IllegalSquare = INDEX_NONE;

	/** Legal moves of the selected piece. */
	TArray<FEGChessMove> SelectedMoves;

	bool bHasLastMove = false;
	FEGChessMove LastMove;

	FEGChessFeedbackOptions Options;
};

/**
 * Which marks each square carries. Plain C++ and deterministic, so it is unit tested; drawing
 * the marks is the renderer's job.
 *
 * Special moves need nothing special from the caller: castling marks its rook as the partner,
 * en passant is a capture whose taken pawn also gets the capture mark, and pins and check fall
 * out of only ever marking legal moves.
 */
namespace EGChessBoardFeedback
{
	UNREALEXTENDEDGAMEPLAY_API void Build(const FEGChessFeedbackInput& Input, FEGChessSquareMarks& OutMarks);
}
