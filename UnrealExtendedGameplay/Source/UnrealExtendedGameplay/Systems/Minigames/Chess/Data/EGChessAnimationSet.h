// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Table/EGChessMatchTypes.h"

#include "EGChessAnimationSet.generated.h"

class UAnimMontage;

/**
 * Montages for one character (or skeleton). Every slot is optional.
 *
 * The enter and exit clips may carry root motion: the table reads it to know where each clip
 * starts and ends, and drives the pawn along it on every machine. With bScriptedRootMotion off
 * the pawn is simply placed on the seat and the animation carries the body.
 *
 * A character that moves pieces by hand (IEGChessCharacterInterface::CanMoveChessPiecesByHand) gets
 * the motion worked out by the table from the Hand settings below, then plays the clock press clip.
 * With bHandFollowsPointer its hand also hovers over the board during its player's turn, picks a
 * piece up on the first click and sets it down on the second (the Hover settings).
 * Any other character only plays the clock press while the pieces move by themselves.
 *
 * The clock press is synced with the engine's own Montage Notify ("PlayMontageNotify") named as
 * below, so no plugin class ends up inside game assets.
 */
UCLASS(BlueprintType)
class UNREALEXTENDEDGAMEPLAY_API UEGChessAnimationSet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Seat")
	TSoftObjectPtr<UAnimMontage> EnterLeft;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Seat")
	TSoftObjectPtr<UAnimMontage> EnterRight;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Seat")
	TSoftObjectPtr<UAnimMontage> EnterBack;

	/** A montage with a looping section. Empty leaves the seated pose to the character's AnimBP. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Seat")
	TSoftObjectPtr<UAnimMontage> SeatedIdle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Seat")
	TSoftObjectPtr<UAnimMontage> ExitLeft;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Seat")
	TSoftObjectPtr<UAnimMontage> ExitRight;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Seat")
	TSoftObjectPtr<UAnimMontage> ExitBack;

	/**
	 * After the piece is down, for a clock on the seat's right: the hand leaves the board, presses the
	 * clock and returns to rest. It starts as the hand lets go of the last piece, so its first frame
	 * should have the hand just above the board.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move")
	TSoftObjectPtr<UAnimMontage> ClockPressRightHand;

	/** For a clock on the seat's left. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move")
	TSoftObjectPtr<UAnimMontage> ClockPressLeftHand;

	/** Speed-up of the hand motion and the clock press under MoveAnimation = Fast. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move", meta = (ClampMin = "1"))
	float FastPlayRate = 1.75f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move")
	FName NotifyClockPress = TEXT("Chess.ClockPress");

	/** Used when the clock press clip lacks the notify: seconds into the clip at play rate 1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move", meta = (ClampMin = "0", Units = "s"))
	float FallbackClockPressTime = 0.75f;

	/** From rest until the hand hovers over the first piece; the hand eases from its animation onto the board. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hand", meta = (ClampMin = "0.05", Units = "s"))
	float ReachTime = 0.45f;

	/**
	 * Across the board, carrying or not, in cm/s. A leg takes its distance over this, kept within
	 * MinTravelTime..MaxTravelTime.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hand", meta = (ClampMin = "1", Units = "cm/s"))
	float TravelSpeed = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hand", meta = (ClampMin = "0.05", Units = "s"))
	float MinTravelTime = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hand", meta = (ClampMin = "0.05", Units = "s"))
	float MaxTravelTime = 0.7f;

	/** The hand comes down onto a piece, and leaves it, from this far above where it grips. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hand", meta = (ClampMin = "0", Units = "cm"))
	float ApproachHeight = 5.0f;

	/** A carried piece clears the pieces it passes over by this much. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hand", meta = (ClampMin = "0", Units = "cm"))
	float CarryClearance = 2.5f;

	/** Down from the approach height onto a piece, and back up after letting go. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hand", meta = (ClampMin = "0.01", Units = "s"))
	float LowerTime = 0.12f;

	/** The fingers closing on a piece before it leaves its square. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hand", meta = (ClampMin = "0.01", Units = "s"))
	float GripTime = 0.08f;

	/** The fingers opening after a piece is down. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hand", meta = (ClampMin = "0.01", Units = "s"))
	float ReleaseTime = 0.1f;

	/** Where on a piece the fingers close, as a share of its pick height (FEGChessPieceVisual::PickHeight). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hand", meta = (ClampMin = "0", ClampMax = "1"))
	float GripHeightFraction = 0.75f;

	/**
	 * What a capture adds: the hand takes the captured piece to the tray first, then comes back for its
	 * own. That whole trip is fitted into this time.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hand", meta = (ClampMin = "0.1", Units = "s"))
	float CaptureTripTime = 0.75f;

	/** How high the hand arcs while easing between its animation and the board, so it clears the table edge. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hand", meta = (ClampMin = "0", Units = "cm"))
	float BlendArcHeight = 12.0f;

	/** Easing back to the animation once the hand lets go of the board, under the clock press clip. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hand", meta = (ClampMin = "0.01", Units = "s"))
	float BlendOutTime = 0.3f;

	/**
	 * During its player's turn the hand rises over the board (over ReachTime) and follows the pointer.
	 * A click on a piece sends it down to pick the piece up, the next click on a square sets the piece
	 * down there, and the clock press follows once the move lands. Everyone sees it: the seat
	 * replicates its player's hand intent. Off, the hand moves only once a move is committed.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hover")
	bool bHandFollowsPointer = true;

	/** The open hand's grip point stays this far above the tallest piece near it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hover", meta = (ClampMin = "0", Units = "cm", EditCondition = "bHandFollowsPointer"))
	float HoverClearance = 3.5f;

	/** ...and never lower than this above the board, over empty squares. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hover", meta = (ClampMin = "0", Units = "cm", EditCondition = "bHandFollowsPointer"))
	float HoverMinHeight = 7.0f;

	/** Pieces within this many squares of the hand count as near it, for the hover and carry heights. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hover", meta = (ClampMin = "0", EditCondition = "bHandFollowsPointer"))
	float HoverClearanceRadius = 0.8f;

	/** How far the hand trails the pointer, in seconds (a critically damped spring). 0 follows it exactly. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hover", meta = (ClampMin = "0", Units = "s", EditCondition = "bHandFollowsPointer"))
	float HoverSmoothTime = 0.12f;

	/** The hand never crosses the board faster than this, so a flick of the mouse sends it across instead of teleporting it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hover", meta = (ClampMin = "1", Units = "cm/s", EditCondition = "bHandFollowsPointer"))
	float HoverMaxSpeed = 160.0f;

	/** A slow rise and fall of the hovering hand, and its period. 0 holds it still. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hover", meta = (ClampMin = "0", Units = "cm", EditCondition = "bHandFollowsPointer"))
	float HoverBobHeight = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hover", meta = (ClampMin = "0.1", Units = "s", EditCondition = "bHandFollowsPointer"))
	float HoverBobPeriod = 2.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Idle")
	TArray<TSoftObjectPtr<UAnimMontage>> ThinkingIdles;

	/** Seconds into a turn before the first fidget. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Idle", meta = (ClampMin = "0", Units = "s"))
	float ThinkingIdleDelay = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Idle", meta = (ClampMin = "0", Units = "s"))
	float ThinkingIdleMinGap = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reactions")
	TSoftObjectPtr<UAnimMontage> OfferDraw;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reactions")
	TSoftObjectPtr<UAnimMontage> Resign;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reactions")
	TSoftObjectPtr<UAnimMontage> Check;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reactions")
	TSoftObjectPtr<UAnimMontage> Win;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reactions")
	TSoftObjectPtr<UAnimMontage> Lose;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reactions")
	TSoftObjectPtr<UAnimMontage> Draw;

	/**
	 * Drive the pawn along the enter/exit clips' root motion on every machine. Off places the pawn
	 * on the seat and lets the animation carry the body (import those clips without root motion).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Seating")
	bool bScriptedRootMotion = true;

	/** Share of an enter clip over which a mis-placed start is corrected. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Seating", meta = (ClampMin = "0.05", ClampMax = "1"))
	float WarpFraction = 0.4f;

	/** Slide time when there is no enter or exit clip. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Seating", meta = (ClampMin = "0.05", Units = "s"))
	float FallbackSlideTime = 0.35f;

	/** Seat height the clips were authored for (floor to seat top). The editor warns on a mismatch. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Seating", meta = (Units = "cm"))
	float AuthoredSeatHeight = 45.0f;

	/** Distance from the sit anchor to the board centre the clips were authored for. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Seating", meta = (Units = "cm"))
	float AuthoredBoardDistance = 80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Seating", meta = (Units = "cm"))
	float LayoutTolerance = 8.0f;

	/** Where the eye view follows. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	FName EyeSocket = TEXT("head");

	/** Offset from the socket, in the actor's frame (forward, right, up). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	FVector EyeOffset = FVector(10.0f, 0.0f, 5.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blend", meta = (ClampMin = "0", Units = "s"))
	float BlendInTime = 0.25f;

	TSoftObjectPtr<UAnimMontage> GetEnterMontage(EEGChessEntrySide Side) const;
	TSoftObjectPtr<UAnimMontage> GetExitMontage(EEGChessEntrySide Side) const;
	TSoftObjectPtr<UAnimMontage> GetReactionMontage(EEGChessReaction Reaction) const;

	void GatherSoftPaths(TArray<FSoftObjectPath>& OutPaths) const;
};
