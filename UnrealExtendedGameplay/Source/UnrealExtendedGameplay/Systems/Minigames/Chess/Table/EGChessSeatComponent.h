// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "Components/SceneComponent.h"
#include "CoreMinimal.h"
#include "EGChessMatchTypes.h"

#include "EGChessSeatComponent.generated.h"

class AEGChessTableActor;
class APawn;
class UAnimInstance;
class UAnimMontage;
class UCameraComponent;
class UEGChessAnimationSet;
class USkeletalMeshComponent;
class UStaticMeshComponent;
struct FBranchingPointNotifyPayload;

/** When the pieces of one move lift, land and press the clock, in seconds after the commit. */
struct FEGChessMoveTiming
{
	bool bCharacterDriven = false;
	/** False when the clip plays but the piece keeps its own timing (UEGChessAnimationSet::bPieceWaitsForHand). */
	bool bPieceFollowsHand = true;
	float PickTime = 0.1f;
	float PlaceTime = 0.55f;
	float ClockPressTime = 0.85f;
};

/**
 * UEGChessSeatComponent
 *
 * One chair. Its own transform is the sit anchor: the floor point under the seated character's
 * root, facing the board (+X). The table creates the chair mesh and the elevated camera as its
 * children.
 *
 * Holds the occupant and the seat's animation state, both replicated, and plays that state on
 * every machine. Sitting down and standing up are scripted sequences: the server stops the
 * pawn's movement and every machine moves the pawn along the clip's root motion from the same
 * replicated start, so nothing replicates while it plays and nothing fights character movement.
 */
UCLASS(ClassGroup = (Chess), meta = (BlueprintSpawnableComponent))
class UNREALEXTENDEDGAMEPLAY_API UEGChessSeatComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UEGChessSeatComponent();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess")
	EEGChessSeat SeatId = EEGChessSeat::A;

	/** Where a seat sequence without a clip starts and ends, sideways or behind. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess", meta = (Units = "cm"))
	float FallbackEntryDistance = 60.0f;

	/** Created by the table. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chess")
	TObjectPtr<UStaticMeshComponent> Chair = nullptr;

	/** Aimed by designers; never activated. The player's local camera copies it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chess")
	TObjectPtr<UCameraComponent> BoardCamera = nullptr;

	// -----------------------------------------------------------------
	// Queries
	// -----------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Chess")
	FEGChessSeatOccupant GetOccupant() const { return Occupant; }

	const FEGChessSeatOccupant& GetOccupantRef() const { return Occupant; }

	UFUNCTION(BlueprintPure, Category = "Chess")
	bool IsOccupied() const { return Occupant.IsOccupied(); }

	UFUNCTION(BlueprintPure, Category = "Chess")
	bool IsAI() const { return Occupant.bAI; }

	UFUNCTION(BlueprintPure, Category = "Chess")
	APawn* GetSeatedPawn() const { return Occupant.Pawn; }

	UFUNCTION(BlueprintPure, Category = "Chess")
	FEGChessSeatAnimState GetAnimState() const { return AnimState; }

	UFUNCTION(BlueprintPure, Category = "Chess")
	bool IsSequenceActive() const { return Sequence.bActive; }

	/** The actor transform a seated pawn has: the anchor lifted by the pawn's capsule half-height. */
	FTransform GetSeatedActorTransform(const APawn* Pawn) const;

	/** Unit direction of an entry side, in world space. */
	FVector GetSideDirection(EEGChessEntrySide Side) const;

	/** Duration of the enter clip for this side, or the fallback slide. For the camera blend. */
	float GetEnterDuration(const APawn* Pawn, EEGChessEntrySide Side) const;

	// -----------------------------------------------------------------
	// Server
	// -----------------------------------------------------------------

	void SetOccupant(const FEGChessSeatOccupant& NewOccupant);
	void ClearOccupant();
	void MarkDisconnected(double ForfeitServerTime);

	/** Pick the entry side facing the pawn with space to stand in. */
	EEGChessEntrySide ChooseEntrySide(const APawn* Pawn) const;
	EEGChessEntrySide ChooseExitSide(const APawn* Pawn) const;

	void BeginEnter(EEGChessEntrySide Side, const FTransform& StartTransform);
	void SnapSeated();
	void BeginExit(EEGChessEntrySide Side, bool bImmediate);

	// -----------------------------------------------------------------
	// Every machine
	// -----------------------------------------------------------------

	/**
	 * Play the move clip for the seated character. False when there is nobody (or no clip), in
	 * which case OutTiming keeps the table's defaults and pieces move by themselves.
	 */
	bool PlayMoveClip(float PlayRate, bool bClockOnRight, FEGChessMoveTiming& OutTiming);

	void PlayReaction(EEGChessReaction Reaction, int32 Variant);

	/** The montage length of the move clip that would play, divided by the play rate; 0 without one. */
	float GetMoveClipDuration(float PlayRate, bool bClockOnRight) const;

	void SetOwningTable(AEGChessTableActor* InTable) { OwningTable = InTable; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void OnUnregister() override;

protected:
	UPROPERTY(ReplicatedUsing = OnRep_Occupant)
	FEGChessSeatOccupant Occupant;

	UPROPERTY(ReplicatedUsing = OnRep_AnimState)
	FEGChessSeatAnimState AnimState;

	UFUNCTION()
	void OnRep_Occupant();

	UFUNCTION()
	void OnRep_AnimState();

	UFUNCTION()
	void HandleMontageNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& Payload);

	void HandleOneShotBlendingOut(UAnimMontage* Montage, bool bInterrupted);

private:
	struct FSequenceState
	{
		bool bActive = false;
		bool bExit = false;
		bool bScripted = false;
		bool bFallbackSlide = false;
		bool bIdleStarted = false;
		double LocalStartTime = 0.0;
		float Duration = 0.0f;
		float WarpDuration = 0.0f;
		FTransform Start;
		FTransform Desired;
		FTransform Final;
		TWeakObjectPtr<UAnimMontage> Montage;
		FQuat MeshRelativeRotation = FQuat::Identity;
	};

	AEGChessTableActor* GetTable() const;
	UEGChessAnimationSet* GetAnimationSet(const APawn* Pawn) const;
	USkeletalMeshComponent* GetPawnMesh(const APawn* Pawn) const;
	UAnimInstance* GetPawnAnimInstance(const APawn* Pawn) const;
	double GetServerTime() const;

	void ApplyAnimState();
	void BeginSequence(bool bExit, UAnimMontage* Montage, const FTransform& Start, float Elapsed);
	void FinishSequence();
	FTransform EvaluateSequence(float Time) const;
	FTransform ExtractActorRootMotion(UAnimMontage* Montage, float Time) const;

	bool PlayMontage(UAnimMontage* Montage, float PlayRate, float StartTime, bool bReturnToIdle);
	void StopMontage(UAnimMontage* Montage);
	void PlaySeatedIdle();

	void PrepareSeatedPawn(APawn* Pawn);
	void ReleaseSeatedPawn(APawn* Pawn);
	void BindPawnAnimation(APawn* Pawn);
	void UnbindPawnAnimation();
	void SetPawnTransform(APawn* Pawn, const FTransform& Transform);
	bool IsSpotClear(const APawn* Pawn, const FVector& Location) const;
	FVector GetEntryStartLocation(const APawn* Pawn, EEGChessEntrySide Side) const;

	TWeakObjectPtr<AEGChessTableActor> OwningTable;
	FSequenceState Sequence;
	TWeakObjectPtr<APawn> PreparedPawn;
	TWeakObjectPtr<UAnimInstance> BoundAnimInstance;
	TWeakObjectPtr<UAnimMontage> CurrentMoveMontage;
	uint8 AppliedSequence = 255;
	bool bNotifiedSeated = false;
};
