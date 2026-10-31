// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "EGTurnInPlaceTypes.h"
#include "Engine/EngineBaseTypes.h"
#include "UnrealExtendedGameplay/Animation/AnimInstance/EGAnimStateProvider.h"

#include "EGTurnInPlaceComponent.generated.h"

class ACharacter;
class UAnimInstance;
class UAnimMontage;
class UCharacterMovementComponent;
class UEGTurnInPlaceComponent;
class UEGTurnInPlaceMovementSource;
class UEGTurnInPlaceSet;
class UPrimitiveComponent;
class USkeletalMeshComponent;

/**
 * The component's extra ticks. The driver tick runs its own rotation after the movement component
 * and before the mesh, so the animation sees the final rotation the frame it is applied. The
 * snapshot tick records the rotation just before the movement component, so the driver replaces
 * only the movement's own turn and keeps rotations other systems made.
 */
USTRUCT()
struct FEGTurnInPlaceDriverTickFunction : public FTickFunction
{
	GENERATED_BODY()

	UEGTurnInPlaceComponent* Target = nullptr;

	/** The pre-movement snapshot rather than the driver. */
	bool bSnapshot = false;

	virtual void ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent) override;
	virtual FString DiagnosticMessage() override;
};

template <>
struct TStructOpsTypeTraits<FEGTurnInPlaceDriverTickFunction> : public TStructOpsTypeTraitsBase2<FEGTurnInPlaceDriverTickFunction>
{
	enum
	{
		WithCopy = false
	};
};

/**
 * UEGTurnInPlaceComponent
 *
 * Actor-based turn in place: while the owner stands still its capsule is held back from the
 * desired facing by a turn offset, and turn animations consume that offset through their
 * RemainingTurnYaw curve (or root track, or a procedural turn), so the capsule rotates exactly
 * as the animation does. The mesh is never counter-rotated.
 *
 * Works with no code: add the component, assign a turn set, and add the Turn In Place node to the
 * AnimBP. The component then rotates the owner itself after the movement component (Rotation
 * Driver: Component). That is not predicted, so player characters in multiplayer forward the
 * hooks instead (EGTurnInPlaceHooks.h); Auto switches to them as soon as they are seen.
 *
 * The owner is read through a Movement Source (Character Movement by default), so other movement
 * systems plug in without changing this component. Pause and lock come from anim state tags, so
 * any system can drive them without depending on this one. See Docs/TurnInPlace.md.
 *
 * Missing hooks and bad setups are reported once each in development builds.
 */
UCLASS(ClassGroup = (Extended), Blueprintable, meta = (BlueprintSpawnableComponent))
class UNREALEXTENDEDGAMEPLAY_API UEGTurnInPlaceComponent : public UActorComponent, public IEGAnimStateProvider
{
	GENERATED_BODY()

	friend struct FEGTurnInPlaceDriverTickFunction;

public:
	UEGTurnInPlaceComponent();

	// ----- Settings -----------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turn In Place")
	TObjectPtr<UEGTurnInPlaceSet> TurnSet;

	/** How the owner is read and rotated. Character Movement covers ACharacter; subclass it for other movement. */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Turn In Place")
	TObjectPtr<UEGTurnInPlaceMovementSource> MovementSource;

	/** What applies the rotation: the forwarded hooks (predicted) or this component (no code needed). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turn In Place")
	EEGTurnRotationDriver RotationDriver = EEGTurnRotationDriver::Auto;

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

	/** Hooks or Component: what applies the rotation this frame (Auto resolved). */
	UFUNCTION(BlueprintPure, Category = "Extended|Turn In Place")
	EEGTurnRotationDriver GetActiveRotationDriver() const;

	UFUNCTION(BlueprintPure, Category = "Extended|Turn In Place")
	FVector GetLastInputDirection() const { return LastInputDirection; }

	/** What the movement source reports this frame. False when it cannot read the owner. */
	UFUNCTION(BlueprintPure, Category = "Extended|Turn In Place")
	bool GetMovementInfo(FEGTurnInPlaceMovementInfo& OutInfo) const;

	/**
	 * Forget the held-back offset and take the current facing as the last input direction. Call
	 * after teleporting or setting the rotation from gameplay, or the character turns back.
	 */
	UFUNCTION(BlueprintCallable, Category = "Extended|Turn In Place")
	void ResetTurn();

	/**
	 * Curves from the anim instance: IEGTurnInPlaceAnimSource when it implements it, otherwise
	 * read from its evaluated curves. Pseudo states on a dedicated server. Game-thread reads use a
	 * copy taken after the mesh's animation finished this frame, never the anim instance while its
	 * worker may be writing it.
	 */
	FEGTurnInPlaceCurves GetCurves() const;

	/** The stance entry in use this frame (resolved once per frame). */
	const FEGTurnInPlaceEntry& GetEntry() const;
	FGameplayTag GetStanceKey() const;

	/** Game thread: what the anim graph needs this frame. */
	FEGTurnInPlaceGraphInput MakeGraphInput() const;

	/** Valid set and movement, and a curve source (an anim instance, or pseudo mode). */
	bool HasValidData() const;

	/** The mesh the movement source picked. */
	USkeletalMeshComponent* GetMesh() const;

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
	virtual FGameplayTag GetStanceTag_Implementation() const;

	/** Which turn angles apply. Default: Mode.Movement while orienting to movement, otherwise Mode.Strafe. */
	UFUNCTION(BlueprintNativeEvent, Category = "Extended|Turn In Place")
	FGameplayTag GetTurnModeTag() const;
	virtual FGameplayTag GetTurnModeTag_Implementation() const;

	/** Treat this root-motion montage as if it were not playing (in addition to the set's montage rules). */
	UFUNCTION(BlueprintNativeEvent, Category = "Extended|Turn In Place")
	bool ShouldIgnoreMontage(const UAnimMontage* Montage) const;
	virtual bool ShouldIgnoreMontage_Implementation(const UAnimMontage* Montage) const;

	/** Whether a turn animation is aborted when turn in place stops being enabled mid-turn. */
	UFUNCTION(BlueprintNativeEvent, Category = "Extended|Turn In Place")
	bool CanAbortTurn() const;
	virtual bool CanAbortTurn_Implementation() const;

	// ----- Interfaces and engine ------------------------------------------------------------------

	virtual FGameplayTagContainer GetAnimStateTags_Implementation() const override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void OnRegister() override;
	virtual void RegisterComponentTickFunctions(bool bRegister) override;
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

	/** After the movement component, before the mesh (Rotation Driver: Component). */
	UPROPERTY()
	FEGTurnInPlaceDriverTickFunction DriverTick;

	/** After the controller, before the movement component: the rotation the movement starts from. */
	UPROPERTY()
	FEGTurnInPlaceDriverTickFunction SnapshotTick;

private:
	struct FRequest
	{
		int32 Id = 0;
		bool bLock = false;
		FName Reason;
	};

	const UEGTurnInPlaceMovementSource* GetSource() const;
	bool GatherMovementInfo(FEGTurnInPlaceMovementInfo& OutInfo) const;
	bool HasValidData(const FEGTurnInPlaceMovementInfo& Info) const;
	EEGTurnInPlaceState ResolveEnabledState(const FEGTurnInPlaceMovementInfo& Info, EEGTurnStateReason& OutReason) const;
	EEGTurnRotationDriver ResolveDriver(const FEGTurnInPlaceMovementInfo& Info) const;
	static bool IsStationary(const FEGTurnInPlaceMovementInfo& Info);
	bool WantsPseudoAnimState() const;
	FGameplayTag ResolveStanceKey() const;

	/** Tags from every IEGAnimStateProvider on the owner, gathered once per frame (windows, rotation lock, stance). */
	const FGameplayTagContainer& GetStateTags() const;
	void RefreshStateProviders() const;

	/** One standing step: solver update, then the rotation (none on simulated proxies). Returns the rotation applied. */
	FRotator RunTurnInPlace(const FRotator& Current, const FRotator& Desired, EEGTurnInPlaceState State, bool bSimulatedProxy, const FVector& Up);
	void ApplyRotation(const FRotator& NewRotation);
	void UpdateLastInputDirection(const FEGTurnInPlaceMovementInfo& Info);
	void UpdateReplicatedOffset();
	FEGTurnInPlaceRequestHandle PushRequest(bool bLock, FName Reason);

	void TickDriver(float DeltaTime);
	void TickSnapshot();
	void UpdateSnapshotPrerequisite();
	/** Where the driver starts this frame: its own last rotation, or what other systems turned the owner to. */
	FRotator GetDriverStartRotation(const FEGTurnInPlaceMovementInfo& Info) const;
	void SetDriverRotation(const FRotator& Rotation);
	void UpdateSettle(const FEGTurnInPlaceMovementInfo& Info, float DeltaTime);
	void TickPseudoStates(float DeltaTime);
	void TickSimulatedProxy(const FEGTurnInPlaceMovementInfo& Info);
	void TickSelfCheck(const FEGTurnInPlaceMovementInfo& Info);
	void DrawDebug() const;
	void ReportProblem(const FString& Message) const;

	TWeakObjectPtr<USkeletalMeshComponent> CachedMesh;
	TWeakObjectPtr<UAnimInstance> CachedAnimInstance;
	bool bHasAnimSource = false;

	/** The anim instance's curves, copied in this component's tick (after the mesh's, by prerequisite). */
	FEGTurnInPlaceCurves CachedCurves;
	void RefreshCurveCache();

	FVector LastInputDirection = FVector::ForwardVector;
	double LastRootMotionTime = -1000.0;

	TArray<FRequest> Requests;
	int32 NextRequestId = 0;

	mutable TArray<TWeakObjectPtr<UObject>> StateProviders;
	mutable int32 ProviderComponentCount = INDEX_NONE;
	mutable uint64 CachedStateTagsFrame = MAX_uint64;
	mutable FGameplayTagContainer CachedStateTags;
	mutable uint64 CachedStanceFrame = MAX_uint64;
	mutable FGameplayTag CachedStanceKey;

	/** Component driver: the rotation it left the owner at, and what the owner stood on then. */
	FRotator DriverRotation = FRotator::ZeroRotator;
	bool bHasDriverRotation = false;
	FRotator PreMovementRotation = FRotator::ZeroRotator;
	uint64 PreMovementFrame = MAX_uint64;
	TWeakObjectPtr<AActor> SnapshotController;
	TWeakObjectPtr<UPrimitiveComponent> DriverBase;
	FRotator DriverBaseRotation = FRotator::ZeroRotator;

	float SettleTimer = 0.0f;
	bool bWantsSettle = false;

	EEGTurnPseudoState PseudoState = EEGTurnPseudoState::Idle;
	FEGTurnInPlaceNodeState PseudoNode;
	bool bPseudoTurnedThisEntry = false;

	bool bFaceHookSeen = false;
	bool bPhysicsHookSeen = false;
	bool bSavedMoveHookSeen = false;
	int32 FramesWithoutFaceHook = 0;
	int32 FramesWithoutPhysicsHook = 0;
	int32 ClientFramesWithoutSavedMove = 0;
	int32 FramesDrivenUnpredicted = 0;
	int32 FramesWaitingForTurn = 0;
};
