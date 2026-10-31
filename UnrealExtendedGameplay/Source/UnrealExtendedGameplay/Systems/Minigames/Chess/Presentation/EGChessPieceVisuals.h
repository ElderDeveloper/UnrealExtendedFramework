// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Player/EGChessHandTypes.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Table/EGChessMatchTypes.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Table/EGChessSeatComponent.h"

#include "EGChessPieceVisuals.generated.h"

class AEGChessTableActor;
class APawn;
class UCurveFloat;
class UEGChessAnimationSet;
class UEGChessPieceSet;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class USceneComponent;
class USkeletalMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * UEGChessPieceVisuals
 *
 * The pieces you see. Deliberately behind the match state: the server commits a move at once,
 * and the pieces follow the character's hand (or their own timing) a moment later.
 *
 * A seated character that moves pieces by hand gets the whole motion planned here from the move
 * record: reach out, come down on the piece, close the fingers, carry it over the board, set it
 * down, let go, then the clock press clip. A capture first takes the captured piece to the tray;
 * castling carries the king and then the rook. Every machine plans the same motion from the same
 * record, so it needs no replication. The carried piece follows the planned grip point and the hand
 * is told to reach the same point (IEGChessCharacterInterface::SetChessHandState), so the two meet
 * without either waiting on the other's animation.
 *
 * Between moves a seat that uses a live hand (AEGChessTableActor::UsesLiveHand) has its hand follow
 * its player's hand intent instead: during their turn it hovers over the board under the pointer,
 * comes down on the piece they click, carries it, and sets it down on the square they click next
 * while the move goes to the server. When the move lands its plan starts from that hand, holding
 * that piece, so the release and the clock press follow on without a jump. The intent replicates
 * through the seat, so every machine shows the same hand here too.
 *
 * Every piece component has a stable identity, its start square, so replaying the move list
 * moves the same knight every time and captured pieces go to the capturing seat's tray.
 *
 * Moves are presented one at a time. A new move (or takeback) arriving while the previous one
 * still plays finishes the previous one at once — its piece lands, its remaining sounds are
 * skipped — so the board never shows two half-done moves and never falls more than one behind.
 * Anything that does not match the real position is snapped by the table (late joiners, a
 * RepNotify that beat its multicast).
 *
 * Everything lives in the BoardSurface frame, so a colour swap is just a different square
 * mapping and the pieces glide to it.
 */
UCLASS(ClassGroup = (Chess))
class UNREALEXTENDEDGAMEPLAY_API UEGChessPieceVisuals : public UActorComponent
{
	GENERATED_BODY()

public:
	UEGChessPieceVisuals();

	/** Build (or rebuild) the piece components from the current piece set. */
	void Initialize(AEGChessTableActor* InTable);

	/** Put every piece where StartFEN + History says, instantly. */
	void Snap(const FString& StartFEN, const TArray<FEGChessMoveRecord>& History, const FEGChessResult& Result);

	/** Present one committed move. PlyIndex must be the next ply the visuals expect. */
	void PresentMove(const FEGChessMoveRecord& Record, int32 PlyIndex, EEGChessSeat MoverSeat, const FEGChessMoveTiming& Timing);

	/** Glide every piece to the state after the first NewLength moves. */
	void PresentTakeback(const TArray<FEGChessMoveRecord>& History, int32 NewLength);

	/** New game: every piece glides home, to mirrored squares after a colour swap. */
	void PresentReset(const FString& StartFEN);

	/** The losing king tips over once the last move has landed. */
	void PresentGameEnd(const FEGChessResult& Result);

	void FinishCurrentStep();

	/** The clock press notify from the seated character's clock press clip. */
	void HandleCharacterNotify(EEGChessSeat Seat, FName Notify, FName ClockName);

	/**
	 * Seconds from the commit until a hand-moved move has its last piece down, from the board alone
	 * (squares and trays, not piece components), so the server can ask it too.
	 */
	float EstimateHandMoveDuration(const FEGChessMoveRecord& Record, EEGChessSeat MoverSeat, const FEGChessMoveTiming& Timing) const;

	/** True when the visuals show exactly StartFEN + History (queued step included). */
	bool MatchesHistory(const FString& StartFEN, const TArray<FEGChessMoveRecord>& History) const;

	int32 GetPresentedPlyCount() const { return PresentedHistory.Num(); }
	bool IsBusy() const { return Step.Kind != EStepKind::None; }

	/** Whose turn the board shows. It trails the replicated position until a committed move has been presented. */
	EEGChessColor GetPresentedSideToMove() const;

	/** True while a seat's hand is over the board between moves, or its player has just raised it. The table wakes this component for it. */
	bool WantsLiveHandTick() const;

	/** Re-place everything after the board mapping changed. */
	void RefreshPlacement();

	// -----------------------------------------------------------------
	// Local, per viewer
	// -----------------------------------------------------------------

	void SetSelection(int32 Square);

	/**
	 * Carry the selected piece: it rests at SurfaceLocal (BoardSurface space, on the playing
	 * surface), raised by the set's CarryLiftHeight, instead of on its square. Cleared by a
	 * selection change.
	 */
	void SetCarry(bool bActive, const FVector& SurfaceLocal = FVector::ZeroVector);
	/** Outlines the pieces on these squares; EmphasisSquare, if valid, gets EmphasisStencil instead. */
	void SetOutlines(const TArray<int32>& Squares, int32 Stencil, int32 EmphasisSquare = INDEX_NONE, int32 EmphasisStencil = 0);
	void ShowGhost(int32 FromSquare, int32 TargetSquare);
	void HideGhost();

	/** Ray (world) against the pieces' pick capsules. The square of the nearest piece hit. */
	bool PickPiece(const FVector& Origin, const FVector& Direction, int32& OutSquare) const;

	/** Clock buttons: the seat that just moved goes down, the other comes up. */
	void SetClockPressed(EEGChessSeat PressedSeat, bool bAnimate);

	UStaticMeshComponent* GetPieceComponentAt(int32 Square) const;

	/** Mesh and materials for one piece: the set's, or a tinted basic-shape stand-in. */
	static void ApplyPieceAppearance(UStaticMeshComponent* Component, const UEGChessPieceSet* Set, EEGChessPieceType Type, EEGChessColor Color);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	enum class EStepKind : uint8 { None, Move, Glide, Topple };

	struct FPieceState
	{
		EEGChessPieceType Type = EEGChessPieceType::None;
		EEGChessColor Color = EEGChessColor::White;
		int32 Square = INDEX_NONE;
		EEGChessSeat TraySeat = EEGChessSeat::A;
		int32 TrayIndex = INDEX_NONE;
		bool bToppled = false;
	};

	struct FTween
	{
		int32 Piece = INDEX_NONE;
		FTransform From;
		FTransform To;
		double StartTime = 0.0;
		float Duration = 0.3f;
		float Lift = 0.0f;
		bool bArc = false;
	};

	/** One pose of the hand's path. The hand eases from the previous key to this one. */
	struct FHandKey
	{
		/** Seconds after the step started. */
		float Time = 0.0f;
		/** Grip point, BoardSurface space. */
		FVector Location = FVector::ZeroVector;
		float Reach = 1.0f;
		float Grip = 0.0f;
		/** Above 0: the segment into this key is a carry, lifted this high over its ends. */
		float CarryHeight = 0.0f;
	};

	enum class EHandAction : uint8 { Grip, Release, LetGo };

	struct FHandAction
	{
		float Time = 0.0f;
		EHandAction Type = EHandAction::Grip;
		int32 Piece = INDEX_NONE;
	};

	/** A piece riding the hand from its grip to its release. */
	struct FHandCarry
	{
		int32 Piece = INDEX_NONE;
		float Start = 0.0f;
		float End = 0.0f;
		FTransform From;
		FTransform To;
		/** From the piece's pivot to its grip point, in the piece's own frame. */
		FVector GripOffset = FVector::ZeroVector;
	};

	struct FHandPlan
	{
		TArray<FHandKey> Keys;
		TArray<FHandAction> Actions;
		TArray<FHandCarry> Carries;
		int32 NextAction = 0;
		/** When the hand lets go of the board and the clock press clip starts. */
		float LetGoTime = 0.0f;
		bool bLetGo = false;
		FEGChessHandState State;
	};

	/** Where one trip of the hand starts and ends: a piece's resting transform before and after. */
	struct FHandTrip
	{
		int32 Piece = INDEX_NONE;
		EEGChessPieceType Type = EEGChessPieceType::None;
		FTransform From;
		FTransform To;
	};

	/** A live hand that was over the board when its player's move landed: the plan carries on from it. */
	struct FHandStart
	{
		bool bValid = false;
		/** Grip point, BoardSurface space. */
		FVector Location = FVector::ZeroVector;
		float Reach = 0.0f;
		float Grip = 0.0f;
		/** The first trip's piece is already in the fingers (its trip's From is where the hand holds it). */
		bool bHoldingFirst = false;
		/** Seconds that piece waits before it goes down: a capture's piece is still being cleared off its square. */
		float Dwell = 0.0f;
	};

	/**
	 * One seat's hand between moves, made from its player's hand intent on every machine. It follows
	 * its goal on a critically damped spring: the pointer, or a piece it goes down on to pick up or
	 * put back, or the square it sets its piece down on.
	 */
	struct FLiveHand
	{
		bool bActive = false;
		TWeakObjectPtr<APawn> Pawn;
		/** Made to tick after this component while the hand is up, so it reads this frame's state. */
		TWeakObjectPtr<USkeletalMeshComponent> TickMesh;
		/** Grip point, BoardSurface space, and its speed on the spring. */
		FVector Location = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		/** Seconds since the hand rose, and how far it has eased off its animation. */
		float RaisedTime = 0.0f;
		float Reach = 0.0f;
		float Grip = 0.0f;
		/** The piece in the fingers: from the moment they close on it until they open over its square. */
		int32 HeldPiece = INDEX_NONE;
		/** From the held piece's pivot to the grip point, in the piece's own frame, and its pose. */
		FVector HeldGripOffset = FVector::ZeroVector;
		FQuat HeldRotation = FQuat::Identity;
		FVector HeldScale = FVector::OneVector;
		/** The hover's slow rise and fall. */
		float BobTime = 0.0f;
		/** What the character was last told. */
		FEGChessHandState State;
	};

	struct FStep
	{
		EStepKind Kind = EStepKind::None;
		double StartTime = 0.0;
		FEGChessMoveRecord Record;
		EEGChessSeat MoverSeat = EEGChessSeat::A;
		FEGChessMoveTiming Timing;
		int32 MoverPiece = INDEX_NONE;
		int32 CapturedPiece = INDEX_NONE;
		int32 RookPiece = INDEX_NONE;
		int32 RookTo = INDEX_NONE;
		bool bPicked = false;
		bool bLanded = false;
		bool bPressed = false;
		double PickedAt = 0.0;
		TArray<FTween> Tweens;
		/** Timing.bHandDriven: the hand's path and the pieces it carries. */
		FHandPlan Hand;
	};

	// Identity
	bool BuildStates(const FString& StartFEN, const TArray<FEGChessMoveRecord>& History, int32 Count, TArray<FPieceState>& OutStates) const;
	void ApplyRecord(TArray<FPieceState>& States, TArray<int32>& PieceAt, const FEGChessMoveRecord& Record) const;
	void EnsureComponents(const TArray<FPieceState>& States);
	void ApplyMesh(int32 Piece);

	// Placement (BoardSurface space)
	FTransform RestingTransform(int32 Piece) const;
	FTransform SquareTransform(int32 Square, EEGChessPieceType Type, EEGChessColor Color) const;
	FTransform TrayTransform(EEGChessSeat Seat, int32 Index, EEGChessPieceType Type, EEGChessColor Color) const;
	float GetPieceScale() const;
	void PlacePiece(int32 Piece, const FTransform& Local);

	// Steps
	void BeginStep(EStepKind Kind);
	void TickStep(double Now);
	void DoPick(double Now);
	void DoLand(double Now);
	void DoPress();
	void AddTween(int32 Piece, const FTransform& To, float Duration, float Lift, bool bArc, double Now);
	void FinishTweens();
	bool AreTweensDone(double Now) const;
	void EvaluateTween(const FTween& Tween, double Now) const;
	double GetNow() const;

	// Hand (BoardSurface space unless named otherwise)
	/** From a piece's pivot to where the fingers close on it (HeightFraction up its capsule), in the piece's own frame. */
	FVector GetGripOffset(EEGChessPieceType Type, float HeightFraction) const;
	/** How high a carried piece's base must rise to clear the pieces between From and To. */
	float GetClearanceBetween(const FVector& From, const FVector& To, const TArray<int32>& Ignore) const;
	/**
	 * The hand's path through these trips (each picks a piece up and sets it down), in order. The
	 * first CaptureTrips are fitted into the animation set's CaptureTripTime. Pieces set in Trips
	 * are only read for their grip offsets; bWithClearance off plans over an empty board.
	 */
	void BuildHandPlan(FHandPlan& Out, const TArray<FHandTrip>& Trips, int32 CaptureTrips, const FEGChessMoveTiming& Timing, bool bWithClearance,
		const FHandStart& From = FHandStart()) const;
	void TickHand(double Now);
	/** The planned grip point at Time, with the reach and grip weights. */
	void EvaluateHandPath(const FHandPlan& Plan, float Time, FVector& OutLocation, float& OutReach, float& OutGrip) const;
	void DoHandAction(const FHandAction& Action, double Now);
	void LetGoOfBoard(double Now);
	/** Hands the character its state. bActive false eases it back to its animation. */
	void PushHandState(const FEGChessHandState& State);
	/** A hand step that ends early lets go of the board; either way the tick order is undone. */
	void EndHand();
	USkeletalMeshComponent* GetHandMesh(const APawn* Pawn) const;
	void EmitEvent(EEGChessEvent Event, int32 Square, EEGChessPieceType Type, EEGChessColor Color, bool bMovedByCharacter);

	// Live hands (between moves)
	void TickLiveHands(float DeltaTime);
	void TickLiveHand(EEGChessSeat Seat, float DeltaTime);
	/** True while a committed move of this seat's own has its hand (until it lets go of the board). */
	bool IsHandInMoveStep(EEGChessSeat Seat) const;
	/** The hand goes back to its animation (bTellCharacter) and whatever it held to its square. */
	void LowerLiveHand(FLiveHand& Hand, bool bTellCharacter);
	/** A committed move takes the hand over where it is; the plan tells the character from here on. */
	void HandLiveHandToPlan(FLiveHand& Hand);
	/** The board is about to move every piece (takeback, reset, snap): the hands let go of theirs. */
	void DropLiveHandPieces();
	/** The tallest piece standing within the set's HoverClearanceRadius of Location (BoardSurface space), Ignore aside. */
	float GetNearbyPieceTop(const FVector& Location, int32 Ignore, const UEGChessAnimationSet* Set) const;
	/** Where the fingers close on Piece when it stands on Square. */
	FVector GetGripPointOnSquare(int32 Piece, int32 Square, const UEGChessAnimationSet* Set) const;
	/** A pick-up or put-back by a live hand, which belongs to no move. */
	void EmitHandEvent(EEGChessSeat Seat, EEGChessEvent Event, int32 Square, EEGChessPieceType Type, EEGChessColor Color);

	TWeakObjectPtr<AEGChessTableActor> Table;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> PieceComponents;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> GhostComponent;

	/**
	 * The ghost again, in the piece's own opaque materials but drawn only into custom depth, so an
	 * outline post-process traces its silhouette. A translucent ghost writes no reliable custom depth.
	 */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> GhostDepthComponent;

	/** The tinted stand-in for a set with no GhostMaterial; rebuilt when the colour changes. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FallbackGhostMaterial;

	TArray<FPieceState> States;
	TArray<int32> PieceAt;
	FString PresentedStartFEN;
	TArray<FEGChessMoveRecord> PresentedHistory;
	FEGChessResult PendingGameEnd;
	bool bGameEndPending = false;

	/** True while Piece is moved by the current step; such a piece is placed only by the step. */
	bool IsPieceInStep(int32 Piece) const;

	FStep Step;
	/** The mesh made to tick after this component while a hand step runs, so it reads this frame's hand state. */
	TWeakObjectPtr<USkeletalMeshComponent> HandTickMesh;

	/** Seat A's and seat B's hands between moves. */
	FLiveHand LiveHands[2];

	/** Side to move in PresentedStartFEN, parsed once per FEN. */
	mutable FString CachedStartSideFEN;
	mutable EEGChessColor CachedStartSide = EEGChessColor::White;
	mutable bool bStartSideCached = false;
	int32 SelectedPiece = INDEX_NONE;
	bool bCarrying = false;
	FVector CarryLocal = FVector::ZeroVector;
	TArray<int32> OutlinedPieces;

	// Clock buttons
	FVector ButtonRaisedA = FVector::ZeroVector;
	FVector ButtonRaisedB = FVector::ZeroVector;
	bool bButtonsCaptured = false;
	EEGChessSeat PressedSeat = EEGChessSeat::B;
	double ButtonTweenStart = -1.0;
};
