// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "EGTurnInPlaceTypes.generated.h"

class UAnimMontage;
class UAnimSequence;
class UEGTurnInPlaceSet;

/** Whether turn in place runs, holds the character still, or steps aside. */
UENUM(BlueprintType)
enum class EEGTurnInPlaceState : uint8
{
	/** Builds up an offset while standing and plays turn animations to close it. */
	Enabled,
	/** No rotation at all; the body stays upright to gravity. */
	Locked,
	/** No offset builds up: the character rotates as if turn in place did not exist. For root motion and scripted moments. */
	Paused
};

/** A per-montage rule. Default means "use the normal root-motion handling". */
UENUM(BlueprintType)
enum class EEGTurnInPlaceOverride : uint8
{
	Default,
	ForceEnabled,
	ForceLocked,
	ForcePaused
};

UENUM(BlueprintType)
enum class EEGTurnSelectMode : uint8
{
	/** The largest step not above the offset (at 175 degrees with 90/180 steps, the 90). */
	Greater,
	/** The closest step (at 175 degrees, the 180). Can over-turn and turn back with small steps. */
	Nearest
};

/** The three usual character rotation setups. */
UENUM(BlueprintType)
enum class EEGCharacterRotationMode : uint8
{
	/** bOrientRotationToMovement: face the movement direction. Turns towards the last input when stopped. */
	OrientToMovement,
	/** bUseControllerDesiredRotation: turn towards the camera at the rotation rate. */
	StrafeDesired,
	/** bUseControllerRotationYaw: face the camera instantly. */
	StrafeDirect
};

/** Which engine function rotates the character in its current setup. */
UENUM(BlueprintType)
enum class EEGTurnMethod : uint8
{
	None,
	/** ACharacter::FaceRotation (StrafeDirect). */
	FaceRotation,
	/** UCharacterMovementComponent::PhysicsRotation (OrientToMovement, StrafeDesired). */
	PhysicsRotation
};

/** How a dedicated server produces the turn curves. */
UENUM(BlueprintType)
enum class EEGTurnServerAnimMode : uint8
{
	/** Run the real anim graph. The mesh must use AlwaysTickPoseAndRefreshBones. */
	Animation,
	/** Track the turn states without animating and read the curves straight from the sequences. */
	Pseudo
};

UENUM(BlueprintType)
enum class EEGTurnPseudoState : uint8
{
	Idle,
	Turn,
	Recovery
};

/** Why the state resolved the way it did (debug display). */
UENUM(BlueprintType)
enum class EEGTurnStateReason : uint8
{
	Entry,
	ConsoleOverride,
	PauseRequest,
	LockRequest,
	PauseCurve,
	LockCurve,
	PauseWindow,
	LockWindow,
	Montage,
	RootMotionSource,
	MovementModifier,
	NoData
};

USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGTurnAngles
{
	GENERATED_BODY()

	FEGTurnAngles() = default;
	FEGTurnAngles(float InMin, float InMax) : MinTurnAngle(InMin), MaxTurnAngle(InMax) {}

	/** Start a turn once the offset reaches this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turn In Place", meta = (ClampMin = "0", ClampMax = "180", Units = "Degrees"))
	float MinTurnAngle = 60.0f;

	/** The offset never exceeds this: past it the character is dragged along. 0 disables. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turn In Place", meta = (ClampMin = "0", ClampMax = "180", Units = "Degrees"))
	float MaxTurnAngle = 0.0f;
};

/** Which montages pause turn in place. Networked root-motion montages pause it unless listed here. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGTurnInPlaceMontageRules
{
	GENERATED_BODY()

	/** Montage-specific behaviour. Takes priority over everything below. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turn In Place")
	TMap<TObjectPtr<UAnimMontage>, EEGTurnInPlaceOverride> Overrides;

	/** Additive montages (aim, recoil, breathing) do not count as playing a montage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turn In Place")
	bool bIgnoreAdditive = true;

	/** Montages using any of these slots do not count (upper-body actions). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turn In Place")
	TArray<FName> IgnoredSlots = { TEXT("UpperBody"), TEXT("UpperBodyAdditive"), TEXT("UpperBodyDynAdditiveBase"), TEXT("UpperBodyDynAdditive"), TEXT("Attack") };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turn In Place")
	TArray<TObjectPtr<UAnimMontage>> IgnoredMontages;
};

/** Curve names. The curve modifier writes the first two; the last two are authored by hand on any animation. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGTurnInPlaceCurveNames
{
	GENERATED_BODY()

	/** Yaw the turn animation still has to turn. Add to every Inertialization node's Filtered Curves. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turn In Place")
	FName RemainingTurnYaw = TEXT("RemainingTurnYaw");

	/** 1 while turning, 0 in the recovery frames. Add to every Inertialization node's Filtered Curves. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turn In Place")
	FName TurnYawWeight = TEXT("TurnYawWeight");

	/** At 1 on any playing animation, turn in place is paused. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turn In Place")
	FName PauseTurnInPlace = TEXT("PauseTurnInPlace");

	/** At 1 on any playing animation, turn in place is locked. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turn In Place")
	FName LockTurnInPlace = TEXT("LockTurnInPlace");
};

/** One stance's turn animations and settings. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGTurnInPlaceEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turn In Place")
	EEGTurnInPlaceState State = EEGTurnInPlaceState::Enabled;

	/** Used while orienting to movement (ExtendedGameplay.TurnInPlace.Mode.Movement). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turn In Place|Angles")
	FEGTurnAngles MovementAngles = FEGTurnAngles(60.0f, 0.0f);

	/** Used while strafing (ExtendedGameplay.TurnInPlace.Mode.Strafe). A max angle keeps the character facing the camera. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turn In Place|Angles")
	FEGTurnAngles StrafeAngles = FEGTurnAngles(60.0f, 135.0f);

	/** Angles for a game's own modes (UEGTurnInPlaceComponent::GetTurnModeTag). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turn In Place|Angles")
	TMap<FGameplayTag, FEGTurnAngles> CustomModeAngles;

	/** Ascending yaw of each step animation. LeftTurns and RightTurns hold one sequence per step. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turn In Place|Steps", meta = (ClampMin = "1", ClampMax = "180"))
	TArray<int32> StepSizes = { 60, 90, 180 };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turn In Place|Steps")
	EEGTurnSelectMode SelectMode = EEGTurnSelectMode::Greater;

	/** Added to the offset before picking a step. Large values can pick a step that needs a correcting turn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turn In Place|Steps", meta = (ClampMin = "-180", ClampMax = "180", Units = "Degrees"))
	float SelectOffset = 0.0f;

	/** Index = step. Each needs the RemainingTurnYaw and TurnYawWeight curves (UEGTurnInPlaceCurveModifier). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turn In Place|Steps")
	TArray<TObjectPtr<UAnimSequence>> LeftTurns;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turn In Place|Steps")
	TArray<TObjectPtr<UAnimSequence>> RightTurns;

	/** Play rate while dragged at the max angle, so the turn keeps up with a fast camera. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turn In Place|Play Rate", meta = (ClampMin = "1"))
	float PlayRateAtMaxAngle = 1.3f;

	/** Play rate while the playing turn goes the wrong way (the player reversed), to finish it quickly. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turn In Place|Play Rate", meta = (ClampMin = "1"))
	float PlayRateOnDirectionChange = 1.7f;

	/** Once a turn reaches the max angle, keep its faster rate until it ends (a mouse keeps re-entering the max angle). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turn In Place|Play Rate")
	bool bKeepMaxAnglePlayRate = true;

	/** StrafeDirect only: how fast the offset eases away once the character moves (1 = one second). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turn In Place", meta = (ClampMin = "0"))
	float MovingInterpOutRate = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turn In Place")
	FEGTurnInPlaceMontageRules Montages;

	/** Angles for a turn mode tag; null when the mode has none. */
	const FEGTurnAngles* GetAngles(const FGameplayTag& ModeTag) const;

	/** The sequence for a step and side, or null. */
	UAnimSequence* GetTurnSequence(int32 StepIndex, bool bRight) const;
};

/** Curve values cached on the worker thread at the point the turn states are decided. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGTurnInPlaceCurves
{
	GENERATED_BODY()

	UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, Category = "Turn In Place")
	float RemainingTurnYaw = 0.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, Category = "Turn In Place")
	float TurnYawWeight = 0.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, Category = "Turn In Place")
	float PauseTurnInPlace = 0.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, Category = "Turn In Place")
	float LockTurnInPlace = 0.0f;

	bool IsTurning() const { return !FMath::IsNearlyZero(TurnYawWeight, KINDA_SMALL_NUMBER); }
};

/**
 * What a movement step works with and changes. Saved with each predicted move and restored when
 * moves are combined (TEGTurnInPlaceSavedMove).
 */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGTurnInPlaceMoveState
{
	GENERATED_BODY()

	/** Desired yaw minus actor yaw, in degrees. Positive: the character still has to turn right. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Turn In Place")
	float TurnOffset = 0.0f;

	/** RemainingTurnYaw x TurnYawWeight from the previous step. */
	UPROPERTY(BlueprintReadOnly, Category = "Turn In Place")
	float CurveValue = 0.0f;

	/** StrafeDirect: progress of easing the offset away after starting to move. */
	UPROPERTY(BlueprintReadOnly, Category = "Turn In Place")
	float InterpOutAlpha = 0.0f;

	/** The previous step had a curve value, so this one may apply a delta. */
	UPROPERTY()
	bool bLastCurveValid = false;
};

/** Gathered on the game thread for the anim graph. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGTurnInPlaceGraphInput
{
	GENERATED_BODY()

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Turn In Place")
	bool bValid = false;

	/** The set and stance in use this frame. Assets are read-only at runtime, so the worker reads the entry from here. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Turn In Place")
	TObjectPtr<UEGTurnInPlaceSet> Set = nullptr;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Turn In Place")
	FGameplayTag StanceKey;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Turn In Place")
	FEGTurnInPlaceCurveNames CurveNames;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Turn In Place")
	EEGTurnInPlaceState State = EEGTurnInPlaceState::Locked;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Turn In Place")
	float TurnOffset = 0.0f;

	/** From the curves; refreshed on the worker before the state machine uses it. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Turn In Place")
	bool bIsTurning = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Turn In Place")
	bool bWantsToTurn = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Turn In Place")
	bool bAbortTurn = false;

	/** The offset asks for a right turn. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Turn In Place")
	bool bTurnRight = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Turn In Place")
	int32 StepIndex = 0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Turn In Place")
	FGameplayTag ModeTag;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Turn In Place")
	bool bIsStrafing = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Turn In Place")
	bool bHasAngles = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Turn In Place")
	FEGTurnAngles Angles;

	/** A dedicated server in Pseudo mode: the graph plays nothing and the component tracks the states. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Turn In Place")
	bool bPseudo = false;

	/** The entry for StanceKey (an empty entry without a set). Safe on the worker. */
	const FEGTurnInPlaceEntry& GetEntry() const;
};

/** What the anim graph's transitions and nodes read. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGTurnInPlaceGraphOutput
{
	GENERATED_BODY()

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Turn In Place")
	float TurnOffset = 0.0f;

	/** Idle -> Turn, Recovery -> Turn. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Turn In Place")
	bool bWantsToTurn = false;

	/** Turn -> Recovery. Never on the frame a turn starts, never while turning, never when aborting. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Turn In Place")
	bool bWantsRecovery = false;

	/** Abort alias -> Idle: turn in place stopped being enabled mid-turn. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Turn In Place")
	bool bAbortTurn = false;

	/** bWantsToTurn and the graph, not the pseudo states, plays the turns. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Turn In Place")
	bool bPlayTurnAnim = false;

	/** Locomotion: strafing with an offset past the min angle, skip Start and go to Cycle. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Turn In Place")
	bool bStartToCycleFromTurn = false;

	/** Locomotion: leave Stop early because a turn is needed or running. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Turn In Place")
	bool bStopToIdleForTurn = false;
};

/** Kept across frames by the anim graph's node functions (or the pseudo states). */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGTurnInPlaceNodeState
{
	GENERATED_BODY()

	UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, Category = "Turn In Place")
	int32 StepIndex = 0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, Category = "Turn In Place")
	double AnimTime = 0.0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, Category = "Turn In Place")
	float PlayRate = 1.0f;

	/** The current turn has reached the max angle at least once (play-rate latch). */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, Category = "Turn In Place")
	bool bReachedMaxAngle = false;

	/** The playing turn animation turns right. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, Category = "Turn In Place")
	bool bTurningRight = true;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, Category = "Turn In Place")
	bool bRecoveryTurningRight = true;

	/** The turn animation has played to its end in this entry (escape from a turn whose weight never registered). */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, Category = "Turn In Place")
	bool bReachedAnimEnd = false;

	/** Recovery has played the rest of the clip: Recovery -> Idle. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, Category = "Turn In Place")
	bool bRecoveryFinished = false;
};

/** Everything the anim instance keeps for turn in place. Pass it by reference to UEGTurnInPlaceAnimLibrary. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGTurnInPlaceAnimData
{
	GENERATED_BODY()

	UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, Category = "Turn In Place")
	FEGTurnInPlaceGraphInput Input;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, Category = "Turn In Place")
	FEGTurnInPlaceCurves Curves;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, Category = "Turn In Place")
	FEGTurnInPlaceGraphOutput Output;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, Category = "Turn In Place")
	FEGTurnInPlaceNodeState Node;

	/** Latched once the weight curve has been seen driving the turn since the graph last entered Turn. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, Category = "Turn In Place")
	bool bTurnedThisEntry = false;
};

/** The offset as replicated to simulated proxies. */
USTRUCT()
struct UNREALEXTENDEDGAMEPLAY_API FEGTurnInPlaceReplicatedOffset
{
	GENERATED_BODY()

	UPROPERTY()
	uint16 Compressed = 0;

	void Set(float Angle) { Compressed = FRotator::CompressAxisToShort(Angle); }
	float Get() const { return static_cast<float>(FRotator::NormalizeAxis(FRotator::DecompressAxisFromShort(Compressed))); }
};

/** Identifies a pause or lock request. Keep it to remove the request. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGTurnInPlaceRequestHandle
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Id = 0;

	bool IsValid() const { return Id != 0; }
	void Invalidate() { Id = 0; }
};
