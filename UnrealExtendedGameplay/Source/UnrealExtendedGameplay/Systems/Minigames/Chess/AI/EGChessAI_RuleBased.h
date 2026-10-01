// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EGChessAI.h"

#include "EGChessAI_RuleBased.generated.h"

struct FEGChessAIWeights;

/**
 * UEGChessAI_RuleBased
 *
 * Scores every legal move with weighted heuristics (material, hanging pieces, development,
 * centre, king safety, checks, promotion) plus a random jitter, and plays the best. The profile
 * sets the weights and the mistakes: Easy skips safety checks and sometimes misses mate in one;
 * Hard adds a one-move lookahead that refuses moves losing material or walking into mate.
 *
 * No search tree. It is meant to be beatable and to feel like a person, not to be strong.
 */
UCLASS(Blueprintable)
class UNREALEXTENDEDGAMEPLAY_API UEGChessAI_RuleBased : public UEGChessAI
{
	GENERATED_BODY()

public:
	virtual FEGChessMove ChooseMove(const FEGChessPosition& Position, const UEGChessAIProfile& Profile, FRandomStream& Random) override;
	virtual bool RespondToDrawOffer(const FEGChessPosition& Position, const UEGChessAIProfile& Profile, FRandomStream& Random) override;

protected:
	float ScoreMove(const FEGChessPosition& Position, const FEGChessMove& Move, const UEGChessAIProfile& Profile, bool bCheckHanging, FRandomStream& Random) const;

	/** Best material the side to move in After can win with one capture. */
	static float BestImmediateGain(const FEGChessPosition& After);

	static bool IsMate(const FEGChessPosition& After);
};
