// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"

#include "EGChessHandTypes.generated.h"

/**
 * What a seated player wants their hand to do between moves, decided on their own machine from
 * their input (UEGChessPlayerComponent): during their turn it hovers over the board under the
 * pointer, holds the piece they clicked, and sets it down on the square they clicked next while
 * the move goes to the server. The owning client sends it to the server and the seat replicates it
 * (UEGChessSeatComponent::GetHandIntent), so every machine turns the same intent into the same hand
 * (UEGChessPieceVisuals). Squares are the presented board's.
 */
USTRUCT()
struct UNREALEXTENDEDGAMEPLAY_API FEGChessHandIntent
{
	GENERATED_BODY()

	/** The hand is over the board. Honoured only on the seat's own turn. */
	UPROPERTY()
	bool bRaised = false;

	/** Where the pointer meets the board: BoardSurface space, on the surface (Z is unused). */
	UPROPERTY()
	FVector_NetQuantize10 Pointer = FVector::ZeroVector;

	/** The square of the piece the hand holds, or goes to pick up. -1: none. */
	UPROPERTY()
	int8 HeldSquare = -1;

	/** The square the held piece is being set down on, waiting for the server. -1: none. */
	UPROPERTY()
	int8 PlaceSquare = -1;

	/** Everything but the pointer, which streams on its own. */
	bool HasSameState(const FEGChessHandIntent& Other) const
	{
		return bRaised == Other.bRaised && HeldSquare == Other.HeldSquare && PlaceSquare == Other.PlaceSquare;
	}
};

/**
 * Where a seated character's hand should be this frame while it moves pieces. The table works it
 * out on every machine and hands it to the character every frame
 * (IEGChessCharacterInterface::SetChessHandState): between moves from the seat's replicated hand
 * intent (the hand follows its player's pointer), and once a move is committed from the move record,
 * picking up wherever the hand already is.
 *
 * The character's animation turns it into a pose: lean and reach so the hand's grip point (between
 * the thumb and the fingertips) lands on GripLocation, close the fingers by GripAlpha, look at
 * LookLocation. The table puts the carried piece on GripLocation itself, so a hand that reaches it
 * holds the piece exactly.
 */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGChessHandState
{
	GENERATED_BODY()

	/** False once the hand has let go of the board: ease back to the animation over BlendOutTime, holding the last GripLocation. */
	UPROPERTY(BlueprintReadOnly, Category = "Chess")
	bool bActive = false;

	/** The hand that moves pieces: the one on the clock's side, so the same hand presses the clock. */
	UPROPERTY(BlueprintReadOnly, Category = "Chess")
	bool bRightHand = true;

	/** 0..1: how far the hand follows GripLocation instead of its animation. Rises while the hand reaches out. */
	UPROPERTY(BlueprintReadOnly, Category = "Chess")
	float ReachAlpha = 0.0f;

	/** 0 open, 1 closed on a piece. */
	UPROPERTY(BlueprintReadOnly, Category = "Chess")
	float GripAlpha = 0.0f;

	/**
	 * 0..1: how far the fingers take the hand's hover pose (open and relaxed over the board, ready to
	 * pick) instead of their animation. GripAlpha closes them from there.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Chess")
	float HoverAlpha = 0.0f;

	/**
	 * True while the hand follows its player's pointer between moves, false while it plays a committed
	 * move. A camera that leans its aim towards the hand can tell the two apart: following a hand that
	 * follows the mouse pulls the view after the pointer.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Chess")
	bool bFollowsPointer = false;

	/** World space. */
	UPROPERTY(BlueprintReadOnly, Category = "Chess")
	FVector GripLocation = FVector::ZeroVector;

	/** World space. */
	UPROPERTY(BlueprintReadOnly, Category = "Chess")
	FVector LookLocation = FVector::ZeroVector;

	/** The board's up, world space. The hand comes down onto pieces along its negative. */
	UPROPERTY(BlueprintReadOnly, Category = "Chess")
	FVector UpVector = FVector::UpVector;

	/** How high the hand arcs (cm) while easing between its animation and GripLocation, so it rises over the table edge. */
	UPROPERTY(BlueprintReadOnly, Category = "Chess")
	float BlendArcHeight = 12.0f;

	/** Seconds to ease back to the animation once bActive goes false. */
	UPROPERTY(BlueprintReadOnly, Category = "Chess")
	float BlendOutTime = 0.3f;
};
