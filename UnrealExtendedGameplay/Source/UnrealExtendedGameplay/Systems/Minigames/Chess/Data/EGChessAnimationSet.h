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
 * Syncing pieces to the hand uses the engine's own Montage Notify ("PlayMontageNotify") named as
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

	/** Move feedback plus clock press, for a clock on the seat's right. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move")
	TSoftObjectPtr<UAnimMontage> MoveRightHand;

	/** For a clock on the seat's left. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move")
	TSoftObjectPtr<UAnimMontage> MoveLeftHand;

	/**
	 * On: the piece leaves its square on the clip's Pick notify and lands on Place, so it moves in the
	 * character's hand. Off: the piece moves the moment the move is committed, on the piece set's
	 * MoveTime, while the clip plays alongside; only the clock press still waits for the hand.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move")
	bool bPieceWaitsForHand = true;

	/** Play rate of the move clip under MoveAnimation = Fast. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move", meta = (ClampMin = "1"))
	float FastPlayRate = 1.75f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move")
	FName NotifyPick = TEXT("Chess.Pick");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move")
	FName NotifyPlace = TEXT("Chess.Place");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move")
	FName NotifyClockPress = TEXT("Chess.ClockPress");

	/** Used when a move clip lacks the notifies, in clip seconds at play rate 1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move", meta = (ClampMin = "0", Units = "s"))
	float FallbackPickTime = 0.77f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move", meta = (ClampMin = "0", Units = "s"))
	float FallbackPlaceTime = 1.53f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move", meta = (ClampMin = "0", Units = "s"))
	float FallbackClockPressTime = 2.37f;

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
