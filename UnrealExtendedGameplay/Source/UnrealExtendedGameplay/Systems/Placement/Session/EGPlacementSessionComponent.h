// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UnrealExtendedGameplay/Systems/Placement/Aim/EGPlacementAimProvider.h"
#include "UnrealExtendedGameplay/Systems/Placement/Session/EGPlacementSessionTypes.h"

#include "EGPlacementSessionComponent.generated.h"

struct FStreamableHandle;
class UEGPlacementCommitHandler;
class UEGPlacementPreviewRenderer;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEGPlacementSessionStateChanged, EEGPlacementSessionState, State);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEGPlacementSessionEvaluationChanged, const FEGPlacementEvaluation&, Evaluation, FGameplayTag, ModeTag);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEGPlacementSessionConfirmRefused, const TArray<FEGPlacementReason>&, Reasons);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEGPlacementSessionDispatched, const FEGPlacementCommitRequest&, Request);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEGPlacementSessionOutcome, const FEGPlacementOutcome&, Outcome, FGameplayTag, ModeTag);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEGPlacementSessionEnded, bool, bCancelled);

/**
 * UEGPlacementSessionComponent
 * A local placement session: previews a selection where its modes aim, and on Confirm hands a
 * fresh valid result to the commit handler. Lives on a pawn, a controller or any actor; each
 * instance is independent, so two local players never share preview or input state.
 *
 * States: Idle -> Preparing (loads the visual and soft data; skipped when loaded) -> Previewing
 * -> AwaitingOutcome (after a dispatch) -> Previewing again or Idle, as AfterCommit says.
 *
 * Input arrives as semantic calls that answer Handled or NotHandled. Confirm on an invalid or
 * pending result is Handled and dispatches nothing, so the press never reaches the equipped item.
 * Held modifiers clear on release (the caller's), on cancel and when the window loses focus.
 *
 * The session never equips, reserves inventory, hides an original actor or deletes a placement;
 * whether there is another item to place after a commit is the project's call.
 */
UCLASS(ClassGroup = (Extended), Blueprintable, meta = (BlueprintSpawnableComponent))
class UNREALEXTENDEDGAMEPLAY_API UEGPlacementSessionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEGPlacementSessionComponent();

	/** Used when a selection brings no modes. Tried in order. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	TArray<FEGPlacementMode> DefaultModes;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category = "Placement")
	TObjectPtr<UEGPlacementPreviewRenderer> PreviewRenderer;

	/** Empty: Confirm refuses with NotConfigured. */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category = "Placement")
	TObjectPtr<UEGPlacementCommitHandler> CommitHandler;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	EEGPlacementAfterCommit AfterCommit = EEGPlacementAfterCommit::PreviewAgain;

	/** Spin per Rotate step when the spec has no spin step. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement", meta = (ClampMin = "0.0"))
	float RotateStepDegrees = 15.0f;

	/** Cached rule results expire after this long, so a pawn walking into the spot is noticed while the aim is still. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement|Cache", meta = (ClampMin = "0.0"))
	float RuleResultMaxAge = 0.2f;

	/** Intents closer than this (cm) share a cache entry. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement|Cache", meta = (ClampMin = "0.001"))
	float CacheLocationQuantum = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	bool bClearModifiersOnFocusLoss = true;

	// -----------------------------------------------------------------
	// Lifecycle
	// -----------------------------------------------------------------

	/** Starts previewing Selection, cancelling any current session. False when no mode is configured. */
	UFUNCTION(BlueprintCallable, Category = "Extended|Placement")
	bool BeginSession(const FEGPlacementSelection& InSelection);

	/** Ends the session without counting it as cancelled (the project decided it is done). */
	UFUNCTION(BlueprintCallable, Category = "Extended|Placement")
	void EndSession();

	// -----------------------------------------------------------------
	// Semantic input
	// -----------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Extended|Placement")
	EEGPlacementInputResult Confirm();

	UFUNCTION(BlueprintCallable, Category = "Extended|Placement")
	EEGPlacementInputResult Cancel();

	/** Spins by Steps spin steps (negative turns the other way). */
	UFUNCTION(BlueprintCallable, Category = "Extended|Placement")
	EEGPlacementInputResult Rotate(float Steps);

	UFUNCTION(BlueprintCallable, Category = "Extended|Placement")
	EEGPlacementInputResult AdjustDistance(float Delta);

	UFUNCTION(BlueprintCallable, Category = "Extended|Placement")
	EEGPlacementInputResult SetFreePosition(bool bFree);

	UFUNCTION(BlueprintCallable, Category = "Extended|Placement")
	EEGPlacementInputResult SetFreeRotation(bool bFree);

	/** Releases held modifiers (free position and rotation). */
	UFUNCTION(BlueprintCallable, Category = "Extended|Placement")
	void ClearModifiers();

	// -----------------------------------------------------------------
	// View
	// -----------------------------------------------------------------

	/** Aim from this view instead of the owner's (a motion controller, a script). */
	UFUNCTION(BlueprintCallable, Category = "Extended|Placement")
	void SetViewOverride(const FEGPlacementView& View);

	UFUNCTION(BlueprintCallable, Category = "Extended|Placement")
	void ClearViewOverride();

	/** The view the modes aim from. Default: the override, else the owning player's camera, else the owner's eyes. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Extended|Placement")
	bool BuildView(FEGPlacementView& OutView) const;
	virtual bool BuildView_Implementation(FEGPlacementView& OutView) const;

	/** Evaluates again now, ignoring the cache. */
	UFUNCTION(BlueprintCallable, Category = "Extended|Placement")
	void RefreshPreview();

	// -----------------------------------------------------------------
	// Queries
	// -----------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Extended|Placement")
	EEGPlacementSessionState GetState() const { return State; }

	UFUNCTION(BlueprintPure, Category = "Extended|Placement")
	bool IsPlacing() const { return State != EEGPlacementSessionState::Idle; }

	/** The result being shown. Meaningful while previewing and HasPose. */
	UFUNCTION(BlueprintPure, Category = "Extended|Placement")
	FEGPlacementEvaluation GetEvaluation() const { return CurrentEvaluation; }
	const FEGPlacementEvaluation& GetEvaluationRef() const { return CurrentEvaluation; }

	UFUNCTION(BlueprintPure, Category = "Extended|Placement")
	bool HasPose() const { return bHasPose; }

	/** The mode whose result is shown. */
	UFUNCTION(BlueprintPure, Category = "Extended|Placement")
	FGameplayTag GetModeTag() const;

	UFUNCTION(BlueprintPure, Category = "Extended|Placement")
	FEGPlacementSelection GetSelection() const { return Selection; }
	const FEGPlacementSelection& GetSelectionRef() const { return Selection; }

	UFUNCTION(BlueprintPure, Category = "Extended|Placement")
	float GetSpinDegrees() const { return SpinDegrees; }

	/** The context the session evaluates in, for project code that evaluates beside it. */
	FEGPlacementContext MakeContext(EEGPlacementScope Scope) const;

	// -----------------------------------------------------------------
	// Events
	// -----------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "Extended|Placement")
	FEGPlacementSessionStateChanged OnStateChanged;

	/** The shown result changed status, mode, anchor or support. */
	UPROPERTY(BlueprintAssignable, Category = "Extended|Placement")
	FEGPlacementSessionEvaluationChanged OnEvaluationChanged;

	/** Confirm found nothing to commit (invalid, pending, no handler, refused dispatch). */
	UPROPERTY(BlueprintAssignable, Category = "Extended|Placement")
	FEGPlacementSessionConfirmRefused OnConfirmRefused;

	UPROPERTY(BlueprintAssignable, Category = "Extended|Placement")
	FEGPlacementSessionDispatched OnCommitDispatched;

	UPROPERTY(BlueprintAssignable, Category = "Extended|Placement")
	FEGPlacementSessionOutcome OnOutcome;

	UPROPERTY(BlueprintAssignable, Category = "Extended|Placement")
	FEGPlacementSessionEnded OnSessionEnded;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	struct FCacheEntry
	{
		bool bSet = false;
		FIntVector Location = FIntVector::ZeroValue;
		FIntVector4 Rotation = FIntVector4(0, 0, 0, 0);
		int32 Spin = 0;
		uint8 Flags = 0;
		TWeakObjectPtr<UObject> Target;
		TWeakObjectPtr<const UEGPlacementProfile> Profile;
		TWeakObjectPtr<UPrimitiveComponent> Support;
		FTransform SupportTransform = FTransform::Identity;
		double Time = 0.0;
		FEGPlacementEvaluation Evaluation;
	};

	void SetState(EEGPlacementSessionState NewState);
	void StartPreviewing();
	void UpdatePreview(bool bForceFresh);
	bool EvaluateMode(int32 ModeIndex, const FEGPlacementView& View, bool bForceFresh, FEGPlacementEvaluation& OutEvaluation, FEGPlacementIntent& OutIntent);
	FCacheEntry MakeCacheKey(const FEGPlacementIntent& Intent, const UEGPlacementProfile* Profile) const;
	bool IsCacheEntryCurrent(const FCacheEntry& Entry, const FCacheEntry& Key) const;
	void HandleOutcome(uint32 ForGeneration, const FEGPlacementOutcome& Outcome, FGameplayTag ModeTag);
	void FinishSession(bool bCancelled);
	void Announce(const FGameplayTag& Channel, const FGameplayTag& ModeTag, const FTransform& Pose, const FEGPlacementOutcome* Outcome = nullptr, const TArray<FEGPlacementReason>* Reasons = nullptr) const;

	UPROPERTY(Transient)
	FEGPlacementSelection Selection;

	UPROPERTY(Transient)
	TArray<FEGPlacementMode> ActiveModes;

	EEGPlacementSessionState State = EEGPlacementSessionState::Idle;
	FEGPlacementEvaluation CurrentEvaluation;
	FEGPlacementIntent CurrentIntent;
	int32 CurrentModeIndex = INDEX_NONE;
	bool bHasPose = false;

	float SpinDegrees = 0.0f;
	bool bFreePosition = false;
	bool bFreeRotation = false;
	bool bForceFreshNextTick = false;
	bool bWasFocused = true;

	TOptional<FEGPlacementView> ViewOverride;
	FEGPlacementView LastView;
	bool bHasLastView = false;

	/** Bumps on every begin and end, so load callbacks and outcomes for an older selection are dropped. */
	uint32 Generation = 0;
	TSharedPtr<FStreamableHandle> PreloadHandle;
	TArray<FCacheEntry> Cache;
};
