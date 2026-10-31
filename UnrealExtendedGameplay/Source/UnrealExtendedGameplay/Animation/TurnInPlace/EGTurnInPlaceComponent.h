// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "EGTurnInPlaceTypes.h"
#include "UnrealExtendedGameplay/Animation/AnimInstance/EGAnimStateProvider.h"

#include "EGTurnInPlaceComponent.generated.h"

class ACharacter;
class UAnimInstance;
class UAnimMontage;
class UCharacterMovementComponent;
class UEGTurnInPlaceSet;
class USkeletalMeshComponent;

/**
 * UEGTurnInPlaceComponent
 *
 * Actor-based turn in place: while the character stands still its capsule is held back from the
 * desired facing by a turn offset, and turn animations consume that offset through their
 * RemainingTurnYaw curve, so the capsule rotates exactly as the animation does. The mesh is never
 * counter-rotated.
 *
 * The project forwards ACharacter::FaceRotation and UCharacterMovementComponent::PhysicsRotation
 * to EGTurnInPlace::HandleFaceRotation / HandlePhysicsRotation, adds TEGTurnInPlaceSavedMove to its
 * saved move, and feeds the anim graph (UEGAnimInstance does it). See Docs/TurnInPlace.md.
 *
 * Missing hooks and bad setups are reported once each in development builds.
 */
UCLASS(ClassGroup = (Extended), Blueprintable, meta = (BlueprintSpawnableComponent))
class UNREALEXTENDEDGAMEPLAY_API UEGTurnInPlaceComponent : public UActorComponent, public IEGAnimStateProvider
{
	GENERATED_BODY()

public:
	UEGTurnInPlaceComponent();

	// ----- Settings -----------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turn In Place")
	TObjectPtr<UEGTurnInPlaceSet> TurnSet;

	/** OrientToMovement: keep turning towards the last input direction once the stick is released. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turn In Place")
	bool bRotateToLastInputDirection = true;

	/** How a dedicated server gets the turn curves. Pseudo avoids refreshing bones on the server. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turn In Place|Network")
	EEGTurnServerAnimMode DedicatedServerAnimMode = EEGTurnServerAnimMode::Animation;

	/** Simulated proxies consume their replicated offset with their own curves between updates. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turn In Place|Network")
	bool bSimulateCurvesOnProxies = true;

	/** Development builds: warn once about missing hooks and setup problems. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turn In Place")
	bool bReportSetupProblems = true;

	/** The component on an actor, or null. */
	static UEGTurnInPlaceComponent* FindFor(const AActor* Actor);

	/** Sets the character and movement flags for one of the three usual rotation setups. */
	UFUNCTION(BlueprintCallable, Category = "Extended|Turn In Place", meta = (DefaultToSelf = "Character"))
	static void SetRotationMode(ACharacter* Character, EEGCharacterRotationMode Mode);

	// ----- Hooks (through EGTurnInPlace::) ------------------------------------------------------

	/** True when handled: the character must not call Super::FaceRotation. */
	bool HandleFaceRotation(FRotator NewControlRotation, float DeltaTime);

	/** True when handled: the movement component must not call Super::PhysicsRotation. */
	bool HandlePhysicsRotation(UCharacterMovementComponent* Movement, float DeltaTime);

	/** True with a rotation towards the last input direction while there is no input. */
	bool ComputeOrientToMovementRotation(const UCharacterMovementComponent* Movement, const FRotator& CurrentRotation, FRotator& OutRotation) const;

	// ----- Saved moves (TEGTurnInPlaceSavedMove) --------------------------------------------------

	const FEGTurnInPlaceMoveState& GetMoveState() const { return MoveState; }
	void RestoreMoveState(const FEGTurnInPlaceMoveState& State) { MoveState = State; }
	void NotifySavedMoveHook() { bSavedMoveHookSeen = true; }

	// ----- Queries --------------------------------------------------------------------------------

	/** Desired yaw minus actor yaw. Not for aim offsets or locomotion: the actor rotation is already true. */
	UFUNCTION(BlueprintPure, Category = "Extended|Turn In Place")
	float GetTurnOffset() const { return MoveState.TurnOffset; }

	/** A turn animation is turning the character (its weight curve is above zero). */
	UFUNCTION(BlueprintPure, Category = "Extended|Turn In Place")
	bool IsTurningInPlace() const;

	UFUNCTION(BlueprintPure, Category = "Extended|Turn In Place")
	EEGTurnInPlaceState GetEnabledState(EEGTurnStateReason& OutReason) const;

	UFUNCTION(BlueprintPure, Category = "Extended|Turn In Place")
	EEGTurnMethod GetTurnMethod() const;

	UFUNCTION(BlueprintPure, Category = "Extended|Turn In Place")
	FVector GetLastInputDirection() const { return LastInputDirection; }

	/** Curves from the anim instance (IEGTurnInPlaceAnimSource), or from the pseudo states on a dedicated server. */
	FEGTurnInPlaceCurves GetCurves() const;

	/** The stance entry in use this frame (resolved once per frame). */
	const FEGTurnInPlaceEntry& GetEntry() const;
	FGameplayTag GetStanceKey() const;

	/** Game thread: what the anim graph needs this frame. */
	FEGTurnInPlaceGraphInput MakeGraphInput() const;

	/** Valid set, character, movement component, and a curve source (anim instance or pseudo mode). */
	bool HasValidData() const;

	// ----- Requests -----------------------------------------------------------------------------

	/** Pause turn in place (the character rotates normally) until the request is removed. */
	UFUNCTION(BlueprintCallable, Category = "Extended|Turn In Place")
	FEGTurnInPlaceRequestHandle PushPause(FName Reason);

	/** Lock turn in place (no rotation) until the request is removed. */
	UFUNCTION(BlueprintCallable, Category = "Extended|Turn In Place")
	FEGTurnInPlaceRequestHandle PushLock(FName Reason);

	/** Removes the request and invalidates the handle. Safe on an invalid handle. */
	UFUNCTION(BlueprintCallable, Category = "Extended|Turn In Place")
	void RemoveRequest(UPARAM(ref) FEGTurnInPlaceRequestHandle& Handle);

	// ----- Project rules (override in a Blueprint or C++ subclass) -------------------------------

	/** A stance key of TurnSet to use. Empty: the first Stances key found in the owner's anim state tags. */
	UFUNCTION(BlueprintNativeEvent, Category = "Extended|Turn In Place")
	FGameplayTag GetStanceTag() const;

	/** Which turn angles apply. Default: Mode.Movement while orienting to movement, otherwise Mode.Strafe. */
	UFUNCTION(BlueprintNativeEvent, Category = "Extended|Turn In Place")
	FGameplayTag GetTurnModeTag() const;

	/** Treat this root-motion montage as if it were not playing (in addition to the set's montage rules). */
	UFUNCTION(BlueprintNativeEvent, Category = "Extended|Turn In Place")
	bool ShouldIgnoreMontage(const UAnimMontage* Montage) const;

	/** Whether a turn animation is aborted when turn in place stops being enabled mid-turn. */
	UFUNCTION(BlueprintNativeEvent, Category = "Extended|Turn In Place")
	bool CanAbortTurn() const;

	// ----- Interfaces and engine ------------------------------------------------------------------

	virtual FGameplayTagContainer GetAnimStateTags_Implementation() const override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void OnRegister() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void OnRep_SimulatedOffset();

	UFUNCTION()
	void HandleAnimInitialized();

	/** The offset for simulated proxies: 16 bits, only to them, only when it changes. */
	UPROPERTY(ReplicatedUsing = OnRep_SimulatedOffset)
	FEGTurnInPlaceReplicatedOffset SimulatedOffset;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Turn In Place")
	FEGTurnInPlaceMoveState MoveState;

private:
	struct FRequest
	{
		int32 Id = 0;
		bool bLock = false;
		FName Reason;
	};

	ACharacter* GetCharacter() const;
	UCharacterMovementComponent* GetMovement() const;
	USkeletalMeshComponent* GetMesh() const;
	bool IsStationary() const;
	bool WantsPseudoAnimState() const;
	UAnimMontage* GetRootMotionMontage() const;
	void GetGravity(FVector& OutUp, FQuat& OutWorldToGravity, FQuat& OutGravityToWorld) const;
	FGameplayTag ResolveStanceKey() const;
	void RefreshStateProviders();

	/** One standing step: solver update, then the rotation (none on simulated proxies). */
	void RunTurnInPlace(const FRotator& Current, const FRotator& Desired, EEGTurnInPlaceState State, bool bSimulatedProxy);
	void ApplyYaw(const FRotator& Base, float AddYaw, const FQuat& WorldToGravity, const FQuat& GravityToWorld);
	void SetOwnerRotation(const FRotator& NewRotation);
	void UpdateLastInputDirection(const UCharacterMovementComponent* Movement);
	void UpdateReplicatedOffset();
	FEGTurnInPlaceRequestHandle PushRequest(bool bLock, FName Reason);

	void TickPseudoStates(float DeltaTime);
	void TickSimulatedProxy();
	void TickSelfCheck();
	void DrawDebug() const;
	void ReportProblem(const FString& Message) const;

	TWeakObjectPtr<ACharacter> CachedCharacter;
	TWeakObjectPtr<UAnimInstance> CachedAnimInstance;
	bool bHasAnimSource = false;

	FVector LastInputDirection = FVector::ForwardVector;
	double LastRootMotionTime = -1000.0;

	TArray<FRequest> Requests;
	int32 NextRequestId = 0;

	TArray<TWeakObjectPtr<UObject>> StateProviders;
	mutable uint64 CachedStanceFrame = MAX_uint64;
	mutable FGameplayTag CachedStanceKey;

	EEGTurnPseudoState PseudoState = EEGTurnPseudoState::Idle;
	FEGTurnInPlaceNodeState PseudoNode;
	bool bPseudoTurnedThisEntry = false;

	bool bFaceHookSeen = false;
	bool bPhysicsHookSeen = false;
	bool bSavedMoveHookSeen = false;
	int32 FramesWithoutFaceHook = 0;
	int32 FramesWithoutPhysicsHook = 0;
	int32 ClientFramesWithoutSavedMove = 0;
};
