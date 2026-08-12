// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "EGInteractionTypes.h"
#include "Engine/HitResult.h"

#include "EGInteractionComponent.generated.h"

class UEGInteractionPromptWidget;

/**
 * UEGInteractionComponent
 *
 * Local focus and prompt tracking with server-authoritative execution.
 *
 * The owning client traces every frame (or every TraceTickInterval), picks a focused
 * interactable and draws a prompt for it — all of which is cosmetic and never trusted. Pressing
 * the interaction input sends a server RPC; the server re-derives the whole decision from its own
 * world state (§IsServerInteractionValid) and only then dispatches to the interactable. Nothing
 * about the client's focus reaches gameplay except as a hint about which actor it meant.
 *
 * Interaction can be refused four ways, each logged separately:
 *  - the winning mode stack entry blocks it (a modal screen is open),
 *  - SetInteractionBlocked(true) was pushed by game code (a GAS tag gate, for instance),
 *  - CanOwnerInteract() returns false (the owner is dead, a ghost, riding something),
 *  - the target itself refuses via CanInteract or carries a BlockedTags tag.
 *
 * Gameplay never disables this component; it refuses instead, so prompts stay accurate.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class UNREALEXTENDEDGAMEPLAY_API UEGInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEGInteractionComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// -----------------------------------------------------------------
	// Trace
	// -----------------------------------------------------------------

	/** CameraCenter traces a line only. SphereSweep tries the line first, then widens. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extended|Interaction|Trace")
	EEGInteractionTraceMode TraceMode = EEGInteractionTraceMode::SphereSweep;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extended|Interaction|Trace")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	/** Radius of the sphere sweep fallback. Ignored in CameraCenter mode. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extended|Interaction|Trace", meta = (ClampMin = "0.1"))
	float TraceRadius = 24.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extended|Interaction|Trace")
	bool bTraceComplex = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extended|Interaction|Trace")
	bool bUseObjectTypeTrace = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extended|Interaction|Trace", meta = (EditCondition = "bUseObjectTypeTrace"))
	TArray<TEnumAsByte<EObjectTypeQuery>> TraceObjectTypes;

	// -----------------------------------------------------------------
	// Range and timing
	// -----------------------------------------------------------------

	/** Max interaction distance, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extended|Interaction|Range", meta = (ClampMin = "50.0"))
	float InteractionRange = 275.0f;

	/** Seconds between traces. 0 traces every frame. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extended|Interaction|Range", meta = (ClampMin = "0.0"))
	float TraceTickInterval = 0.0f;

	/** Seconds of missed traces tolerated before focus is dropped. Stops prompts flickering on jittery aim. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extended|Interaction|Range", meta = (ClampMin = "0.0"))
	float LostFocusGracePeriod = 0.08f;

	/**
	 * Extra distance the server allows over InteractionRange + TraceRadius when validating.
	 * Covers the latency between the client's trace and the request landing on the server.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extended|Interaction|Range", meta = (ClampMin = "0.0"))
	float ValidationSlack = 75.0f;

	// -----------------------------------------------------------------
	// Filtering
	// -----------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extended|Interaction|Filtering")
	bool bIgnoreOwnerAndInstigator = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extended|Interaction|Filtering")
	TArray<TSoftObjectPtr<AActor>> AdditionalIgnoredActors;

	/** Reject a target carrying ANY of these tags. Read through IGameplayTagAssetInterface. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extended|Interaction|Filtering")
	FGameplayTagContainer BlockedTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extended|Interaction|Filtering")
	bool bMustBeFacing = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extended|Interaction|Filtering", meta = (ClampMin = "1.0", ClampMax = "180.0", EditCondition = "bMustBeFacing"))
	float FacingHalfAngle = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extended|Interaction|Filtering")
	EEGInteractionPriority InteractionPriority = EEGInteractionPriority::Nearest;

	// -----------------------------------------------------------------
	// Behaviour
	// -----------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extended|Interaction|Behavior")
	bool bRequiresLineOfSight = true;

	/** Trace under the mouse cursor instead of from the camera. For cursor-visible states. */
	UPROPERTY(BlueprintReadWrite, Category = "Extended|Interaction|Behavior")
	bool bUseMouseTrace = false;

	/** While mouse tracing, ignore SetInteractionBlocked — a dialogue screen can still allow clicks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extended|Interaction|Behavior")
	bool bMouseTraceIgnoresBlocking = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extended|Interaction|Widget")
	TSubclassOf<UEGInteractionPromptWidget> PromptWidgetClass;

	// -----------------------------------------------------------------
	// Debug
	// -----------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extended|Interaction|Debug")
	bool bDebugDraw = false;

	/** On-screen and world text for the focused actor. Independent of bDebugDraw. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extended|Interaction|Debug")
	bool bDebugDrawFocusInfo = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extended|Interaction|Debug", meta = (EditCondition = "bDebugDraw"))
	FColor DebugTraceHitColor = FColor::Green;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extended|Interaction|Debug", meta = (EditCondition = "bDebugDraw"))
	FColor DebugTraceMissColor = FColor::Red;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extended|Interaction|Debug", meta = (EditCondition = "bDebugDraw"))
	FColor DebugTraceBlockedColor = FColor::Yellow;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extended|Interaction|Debug", meta = (EditCondition = "bDebugDraw"))
	bool bDebugShowInteractionRadius = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extended|Interaction|Debug")
	bool bDebugLogTraceResults = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extended|Interaction|Debug", meta = (EditCondition = "bDebugLogTraceResults"))
	EEGInteractionDebugVerbosity DebugLogVerbosity = EEGInteractionDebugVerbosity::Basic;

	// -----------------------------------------------------------------
	// Delegates
	// -----------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "Extended|Interaction")
	FEGInteractionFocusChanged OnFocusChanged;

	UPROPERTY(BlueprintAssignable, Category = "Extended|Interaction")
	FEGInteractionFocusLost OnFocusLost;

	UPROPERTY(BlueprintAssignable, Category = "Extended|Interaction")
	FEGInteractionFailed OnInteractionFailed;

	UPROPERTY(BlueprintAssignable, Category = "Extended|Interaction")
	FEGInteractionHoldProgressChanged OnHoldProgressChanged;

	// -----------------------------------------------------------------
	// Actions
	// -----------------------------------------------------------------

	/** Re-run the trace now instead of waiting for the next tick. */
	UFUNCTION(BlueprintCallable, Category = "Extended|Interaction")
	void RefreshFocus();

	/** Press-and-release in one call. Equivalent to InteractionStart() followed by InteractionEnd(). */
	UFUNCTION(BlueprintCallable, Category = "Extended|Interaction")
	bool TryInteract();

	/** Interaction input pressed. Routed to the server, which decides whether it runs. */
	UFUNCTION(BlueprintCallable, Category = "Extended|Interaction")
	void InteractionStart();

	/** Authority-side hold pump. Called from the component tick; safe to call manually. */
	UFUNCTION(BlueprintCallable, Category = "Extended|Interaction")
	void InteractionTick(float DeltaTime);

	/** Interaction input released, or the target was lost. */
	UFUNCTION(BlueprintCallable, Category = "Extended|Interaction")
	void InteractionEnd();

	UFUNCTION(BlueprintCallable, Category = "Extended|Interaction")
	void EnableMouseInteraction();

	UFUNCTION(BlueprintCallable, Category = "Extended|Interaction")
	void DisableMouseInteraction();

	UFUNCTION(BlueprintPure, Category = "Extended|Interaction")
	bool IsMouseInteractionEnabled() const { return bUseMouseTrace; }

	// -----------------------------------------------------------------
	// Gating
	// -----------------------------------------------------------------

	/**
	 * Push an external block. Game code owns the policy — a GAS tag gate, a cutscene, a vehicle.
	 * While blocked the component clears focus, hides the prompt and refuses every request.
	 */
	UFUNCTION(BlueprintCallable, Category = "Extended|Interaction")
	void SetInteractionBlocked(bool bBlocked);

	UFUNCTION(BlueprintPure, Category = "Extended|Interaction")
	bool IsInteractionBlocked() const { return bInteractionBlocked; }

	/**
	 * Owner-side veto, checked on both client and server. Override for game state that has no
	 * business being in the plugin — dead, downed, a ghost, mid-cutscene. Default allows.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Extended|Interaction")
	bool CanOwnerInteract() const;
	virtual bool CanOwnerInteract_Implementation() const { return true; }

	/** True when anything at all currently refuses interaction. */
	UFUNCTION(BlueprintPure, Category = "Extended|Interaction")
	bool IsInteractionAllowed() const;

	/** The hit that produced the current focus, or the client-supplied hit during server dispatch. */
	UFUNCTION(BlueprintPure, Category = "Extended|Interaction", meta = (DefaultToSelf = "Interactor"))
	static FHitResult GetInteractionHit(const AActor* Interactor);

	// -----------------------------------------------------------------
	// Prompt widget
	// -----------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Extended|Interaction|Widget")
	void ShowPromptWidget();

	UFUNCTION(BlueprintCallable, Category = "Extended|Interaction|Widget")
	void HidePromptWidget();

	UFUNCTION(BlueprintPure, Category = "Extended|Interaction|Widget")
	UEGInteractionPromptWidget* GetPromptWidget() const { return PromptWidgetInstance; }

	/** Keep the prompt hidden regardless of focus. Interaction itself keeps working. */
	UFUNCTION(BlueprintCallable, Category = "Extended|Interaction|Widget")
	void SetPromptSuppressed(bool bSuppress);

	UFUNCTION(BlueprintPure, Category = "Extended|Interaction|Widget")
	bool IsPromptSuppressed() const { return bPromptSuppressed; }

	// -----------------------------------------------------------------
	// Queries
	// -----------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Extended|Interaction")
	AActor* GetFocusedActor() const { return FocusedActor.Get(); }

	UFUNCTION(BlueprintPure, Category = "Extended|Interaction")
	bool HasFocus() const { return FocusedActor.IsValid(); }

	UFUNCTION(BlueprintPure, Category = "Extended|Interaction")
	FText GetFocusedInteractionText() const { return FocusedPresentation.Text; }

	UFUNCTION(BlueprintPure, Category = "Extended|Interaction")
	UTexture2D* GetFocusedInteractionIcon() const { return FocusedPresentation.Icon; }

	UFUNCTION(BlueprintPure, Category = "Extended|Interaction")
	FHitResult GetLastHitResult() const { return LastHitResult; }

	/** The interactable currently mid-interaction on this component, if any. */
	UFUNCTION(BlueprintPure, Category = "Extended|Interaction")
	AActor* GetActiveInteractionActor() const { return ActiveInteractionActor.Get(); }

	/** 0..1 progress of the running hold, or 0 when nothing is holding. */
	UFUNCTION(BlueprintPure, Category = "Extended|Interaction")
	float GetHoldProgress() const;

protected:
	/** Creates the prompt widget. Override to route through a HUD or widget stack instead. */
	virtual void CreatePromptWidget();

	/** Tears down whatever CreatePromptWidget made. */
	virtual void DestroyPromptWidget();

	/** The created widget, for subclasses that route creation elsewhere. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Extended|Interaction|Widget")
	TObjectPtr<UEGInteractionPromptWidget> PromptWidgetInstance;

private:
	TWeakObjectPtr<AActor> FocusedActor;
	FEGInteractionPresentation FocusedPresentation;
	FHitResult LastHitResult;
	TWeakObjectPtr<AActor> ActiveInteractionActor;

	float TraceAccumulator = 0.0f;
	float LostFocusRemaining = 0.0f;
	float LastBroadcastHoldProgress = 0.0f;
	bool bPromptSuppressed = false;
	bool bInteractionBlocked = false;

	UFUNCTION(Server, Reliable)
	void ServerInteractionStart(AActor* TargetActor, FHitResult ClientHitResult);

	UFUNCTION(Server, Reliable)
	void ServerInteractionEnd(AActor* TargetActor);

	UFUNCTION(Client, Reliable)
	void ClientInteractionResult(bool bSuccess, AActor* TargetActor, FHitResult ConfirmedHitResult, const FText& FailureReason);

	// Trace
	void PerformTrace();
	void PerformMouseTrace();
	bool GatherTraceHits(const FVector& Start, const FVector& End, TArray<FHitResult>& OutHits) const;
	bool IsCandidateValid(AActor* Candidate, const FHitResult& HitResult, const FVector& ViewLocation, bool bServerValidation) const;
	AActor* SelectBestCandidate(const TArray<FHitResult>& Hits, const FVector& Start, const FVector& Direction, FHitResult& OutHit) const;
	bool PassesTagFilter(const AActor* Candidate) const;
	void GetViewPoint(FVector& OutLocation, FRotator& OutRotation) const;

	// Focus
	void SetFocusedActor(AActor* NewFocusedActor, const FHitResult& HitResult);
	void ClearFocus();
	void NotifyFocusUpdated();
	void UpdateHoldProgressFeedback();

	// Authority
	bool BeginInteractionOnServer(AActor* TargetActor, const FHitResult& HitResult);
	void EndInteractionOnServer(AActor* TargetActor);
	bool IsServerInteractionValid(AActor* TargetActor, const FHitResult& HitResult) const;

	// Mode stack

	// Debug
	void DrawDebugInfo(const FVector& Start, const FVector& End, bool bHitValid, bool bHitBlocked) const;
};
