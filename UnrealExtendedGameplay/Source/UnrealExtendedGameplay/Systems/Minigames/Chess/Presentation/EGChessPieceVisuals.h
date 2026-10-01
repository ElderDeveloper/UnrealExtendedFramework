// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Table/EGChessMatchTypes.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Table/EGChessSeatComponent.h"

#include "EGChessPieceVisuals.generated.h"

class AEGChessTableActor;
class UCurveFloat;
class UEGChessPieceSet;
class UMaterialInterface;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * UEGChessPieceVisuals
 *
 * The pieces you see. Deliberately behind the match state: the server commits a move at once,
 * and the pieces follow the character's hand (or their own timing) a moment later.
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

	/** Pick/Place/ClockPress from the seated character's move clip. */
	void HandleCharacterNotify(EEGChessSeat Seat, FName Notify, FName PickName, FName PlaceName, FName ClockName);

	/** True when the visuals show exactly StartFEN + History (queued step included). */
	bool MatchesHistory(const FString& StartFEN, const TArray<FEGChessMoveRecord>& History) const;

	int32 GetPresentedPlyCount() const { return PresentedHistory.Num(); }
	bool IsBusy() const { return Step.Kind != EStepKind::None; }

	/** Re-place everything after the board mapping changed. */
	void RefreshPlacement();

	// -----------------------------------------------------------------
	// Local, per viewer
	// -----------------------------------------------------------------

	void SetSelection(int32 Square);
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
	void EmitEvent(EEGChessEvent Event, int32 Square, EEGChessPieceType Type, EEGChessColor Color, bool bMovedByCharacter);

	TWeakObjectPtr<AEGChessTableActor> Table;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> PieceComponents;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> GhostComponent;

	TArray<FPieceState> States;
	TArray<int32> PieceAt;
	FString PresentedStartFEN;
	TArray<FEGChessMoveRecord> PresentedHistory;
	FEGChessResult PendingGameEnd;
	bool bGameEndPending = false;

	FStep Step;
	int32 SelectedPiece = INDEX_NONE;
	TArray<int32> OutlinedPieces;

	// Clock buttons
	FVector ButtonRaisedA = FVector::ZeroVector;
	FVector ButtonRaisedB = FVector::ZeroVector;
	bool bButtonsCaptured = false;
	EEGChessSeat PressedSeat = EEGChessSeat::B;
	double ButtonTweenStart = -1.0;
};
