// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "UObject/Object.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Core/EGChessPosition.h"

#include "EGChessAI.generated.h"

class UEGChessAIProfile;

/**
 * UEGChessAI
 *
 * Chooses moves and answers draw offers. Server only: the table creates one per game against
 * the AI (the class comes from the AI profile) and asks it when it is the AI's turn.
 *
 * The base class plays a random legal move and accepts nothing. Replace it by subclassing and
 * naming the subclass in a UEGChessAIProfile.
 */
UCLASS(Abstract, Blueprintable, BlueprintType)
class UNREALEXTENDEDGAMEPLAY_API UEGChessAI : public UObject
{
	GENERATED_BODY()

public:
	/** A legal move for the side to move. Only called when one exists. */
	virtual FEGChessMove ChooseMove(const FEGChessPosition& Position, const UEGChessAIProfile& Profile, FRandomStream& Random);

	/** Whether to accept a draw offered in Position. */
	virtual bool RespondToDrawOffer(const FEGChessPosition& Position, const UEGChessAIProfile& Profile, FRandomStream& Random);

	/** Seconds to "think" before answering. */
	virtual float GetThinkDelay(const FEGChessPosition& Position, const UEGChessAIProfile& Profile, FRandomStream& Random) const;
};
