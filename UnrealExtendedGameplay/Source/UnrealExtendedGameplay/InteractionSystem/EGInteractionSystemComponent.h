// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EGInteractionSystemTypes.h"
#include "Engine/HitResult.h"
#include "GameplayTasksComponent.h"

#include "EGInteractionSystemComponent.generated.h"

class AController;
class APawn;
class UEGInteraction;
class UEGInteractionProviderComponent;
class UEnhancedInputComponent;
class UInputAction;
class UPrimitiveComponent;

/**
 * UEGInteractionSystemComponent
 *
 * The interactor side: the ability-system-component analog, and a UGameplayTasksComponent so
 * the interactions it owns can run latent tasks through it.
 *
 * It is a registry and nothing more. The local owner traces every tick (or every
 * TraceTickInterval), resolves the provider it is looking at, grants one UEGInteraction instance
 * per definition the provider hands back, binds the input slots, and publishes prompts. Pressing
 * a slot activates the highest-priority granted instance on it. Nothing about the client's focus
 * reaches gameplay except as a hint: the server re-derives the definition from its own provider,
 * re-checks range, occlusion and CanActivate, and only then creates and runs its own instance.
 *
 * Input slots are fixed on the component and bound once. A definition names a slot by tag, so
 * no target can reach for a key outside the set the pawn declared.
 *
 * Gating is code. SetInteractionBlocked is the external switch, CanOwnerInteract the owner's
 * veto, UEGInteraction::CanActivate the instance's own answer. There are no tag relationships.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class UNREALEXTENDEDGAMEPLAY_API UEGInteractionSystemComponent : public UGameplayTasksComponent
{
	GENERATED_BODY()

public:
	UEGInteractionSystemComponent(const FObjectInitializer& ObjectInitializer);

	/** Resolve the system for an actor: interface first, component search second. */
	UFUNCTION(BlueprintPure, Category = "Extended|Interaction System")
	static UEGInteractionSystemComponent* FindInteractionSystem(const AActor* Actor);

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual bool GetShouldTick() const override;

	// -----------------------------------------------------------------
	// Input
	// -----------------------------------------------------------------

	/** Slot tag to input action. Definitions reference the tag. Bound once at possession. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extended|Interaction System|Input")
	TMap<FGameplayTag, TObjectPtr<UInputAction>> InputSlots;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extended|Interaction System|Input")
	bool bAutoBindInput = true;

	UFUNCTION(BlueprintCallable, Category = "Extended|Interaction System|Input")
	void RefreshInputBindings();

	UFUNCTION(BlueprintCallable, Category = "Extended|Interaction System|Input")
	void ClearInputBindings();

	UFUNCTION(BlueprintPure, Category = "Extended|Interaction System|Input")
	bool HasInputBindings() const { return BoundInputComponent.IsValid(); }

	// -----------------------------------------------------------------
	// Trace
	// -----------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extended|Interaction System|Trace")
	EEGInteractionSystemTraceMode TraceMode = EEGInteractionSystemTraceMode::SphereSweep;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extended|Interaction System|Trace")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	/** Radius of the sphere sweep fallback. Ignored in CameraCenter mode. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extended|Interaction System|Trace", meta = (ClampMin = "0.0"))
	float TraceRadius = 16.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extended|Interaction System|Trace", meta = (ClampMin = "10.0"))
	float InteractionRange = 275.0f;

	/** Seconds between traces. 0 traces every frame. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extended|Interaction System|Trace", meta = (ClampMin = "0.0"))
	float TraceTickInterval = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extended|Interaction System|Trace")
	bool bTraceComplex = false;

	/** Sweep hits must also survive a line trace to the impact point. Line hits pass by construction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extended|Interaction System|Trace")
	bool bRequiresLineOfSight = true;

	/** Extra distance the server allows over InteractionRange + TraceRadius. Covers latency. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extended|Interaction System|Trace", meta = (ClampMin = "0.0"))
	float ValidationSlack = 75.0f;

	/** Maximum aim deviation in degrees allowed for camera replication latency. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extended|Interaction System|Trace", meta = (ClampMin = "0.0", ClampMax = "45.0"))
	float ValidationAimTolerance = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extended|Interaction System|Trace")
	TArray<TObjectPtr<AActor>> AdditionalIgnoredActors;

	// -----------------------------------------------------------------
	// Gating
	// -----------------------------------------------------------------

	/** External block pushed by game code: a modal screen, a cutscene, a vehicle. Clears focus and cancels holds. */
	UFUNCTION(BlueprintCallable, Category = "Extended|Interaction System")
	void SetInteractionBlocked(bool bBlocked);

	UFUNCTION(BlueprintPure, Category = "Extended|Interaction System")
	bool IsInteractionBlocked() const { return bInteractionBlocked; }

	/** Owner-side veto, checked on both sides. Dead, downed, a ghost. Default allows. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Extended|Interaction System")
	bool CanOwnerInteract() const;
	virtual bool CanOwnerInteract_Implementation() const { return true; }

	UFUNCTION(BlueprintPure, Category = "Extended|Interaction System")
	bool IsInteractionAllowed() const;

	// -----------------------------------------------------------------
	// Delegates
	// -----------------------------------------------------------------

	/** The visible prompt list changed. Progress moves on its own channel below. */
	UPROPERTY(BlueprintAssignable, Category = "Extended|Interaction System")
	FEGInteractionPromptsChanged OnPromptsChanged;

	UPROPERTY(BlueprintAssignable, Category = "Extended|Interaction System")
	FEGInteractionPromptProgressChanged OnPromptProgressChanged;

	UPROPERTY(BlueprintAssignable, Category = "Extended|Interaction System")
	FEGInteractionSystemFocusChanged OnFocusChanged;

	UPROPERTY(BlueprintAssignable, Category = "Extended|Interaction System")
	FEGInteractionInstanceStarted OnInteractionStarted;

	UPROPERTY(BlueprintAssignable, Category = "Extended|Interaction System")
	FEGInteractionInstanceEnded OnInteractionEnded;

	/** The server refused a request this component sent. */
	UPROPERTY(BlueprintAssignable, Category = "Extended|Interaction System")
	FEGInteractionActivationRejected OnActivationRejected;

	// -----------------------------------------------------------------
	// Actions
	// -----------------------------------------------------------------

	/** Press a slot. Activates the highest-priority granted instance that answers CanActivate. */
	UFUNCTION(BlueprintCallable, Category = "Extended|Interaction System")
	bool TryActivateSlot(FGameplayTag InputTag);

	/** Release a slot. Routed to every instance that slot activated. */
	UFUNCTION(BlueprintCallable, Category = "Extended|Interaction System")
	void ReleaseSlot(FGameplayTag InputTag);

	/** Activate a granted instance by definition id, bypassing the slot. */
	UFUNCTION(BlueprintCallable, Category = "Extended|Interaction System")
	bool TryActivateById(FGameplayTag Id);

	UFUNCTION(BlueprintCallable, Category = "Extended|Interaction System")
	void CancelAllInteractions();

	/** Re-run the trace now instead of waiting for the next tick. */
	UFUNCTION(BlueprintCallable, Category = "Extended|Interaction System")
	void RefreshFocus();

	/** Re-query every granted instance's presentation now. */
	UFUNCTION(BlueprintCallable, Category = "Extended|Interaction System")
	void RefreshPrompts();

	// -----------------------------------------------------------------
	// Queries
	// -----------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Extended|Interaction System")
	AActor* GetFocusedActor() const;

	UFUNCTION(BlueprintPure, Category = "Extended|Interaction System")
	UEGInteractionProviderComponent* GetFocusedProvider() const { return FocusedProvider.Get(); }

	UFUNCTION(BlueprintPure, Category = "Extended|Interaction System")
	FHitResult GetFocusHit() const { return FocusHit; }

	UFUNCTION(BlueprintPure, Category = "Extended|Interaction System")
	TArray<UEGInteraction*> GetGrantedInteractions() const { return TArray<UEGInteraction*>(Granted); }

	UFUNCTION(BlueprintPure, Category = "Extended|Interaction System")
	TArray<UEGInteraction*> GetActiveInteractions() const { return TArray<UEGInteraction*>(Active); }

	UFUNCTION(BlueprintPure, Category = "Extended|Interaction System")
	TArray<FEGInteractionPrompt> GetPrompts() const { return Prompts; }

	UFUNCTION(BlueprintPure, Category = "Extended|Interaction System")
	UEGInteraction* FindGrantedInteraction(FGameplayTag Id) const;

	UFUNCTION(BlueprintPure, Category = "Extended|Interaction System")
	bool IsAnyInteractionActive() const { return Active.Num() > 0; }

	/** The owner's view point: the player camera when controlled by a player, eyes otherwise. */
	UFUNCTION(BlueprintPure, Category = "Extended|Interaction System")
	void GetViewPoint(FVector& OutLocation, FRotator& OutRotation) const;

	/**
	 * The one line-of-sight rule, shared by focus, server validation and tasks. True when nothing
	 * on TraceChannel blocks the line from From to Point, ignoring the owner and Target.
	 */
	UFUNCTION(BlueprintPure, Category = "Extended|Interaction System")
	bool HasLineOfSightToPoint(const FVector& From, const AActor* Target, const FVector& Point) const;

	// -----------------------------------------------------------------
	// Called by UEGInteraction
	// -----------------------------------------------------------------

	void NotifyInteractionStarted(UEGInteraction* Interaction);
	void NotifyInteractionEnded(UEGInteraction* Interaction, bool bCancelled, bool bNotifyServer);
	void NotifyActivationRejected(UEGInteraction* Interaction);
	void SetPromptProgress(int32 Handle, float Progress);

	// -----------------------------------------------------------------
	// Debug
	// -----------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extended|Interaction System|Debug")
	bool bDebugDraw = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extended|Interaction System|Debug")
	bool bDebugDrawFocusInfo = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extended|Interaction System|Debug")
	bool bDebugLog = false;

private:
	friend class FEGInteractionRegressionTest;

	// Granted: one instance per definition the focused provider handed back. Local owner only.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UEGInteraction>> Granted;

	// Active: instances mid-activation. On a client these are granted instances. On the server,
	// for a remote pawn, they are transient instances created per request.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UEGInteraction>> Active;

	UPROPERTY(Transient)
	TArray<FEGInteractionPrompt> Prompts;

	TWeakObjectPtr<UEGInteractionProviderComponent> FocusedProvider;
	TWeakObjectPtr<UPrimitiveComponent> FocusedComponent;
	FHitResult FocusHit;
	TMap<int32, float> PromptProgress;

	int32 NextHandle = 1;
	int32 NextActivationId = 1;
	float TraceAccumulator = 0.0f;
	bool bInteractionBlocked = false;
	bool bWantsLocalTick = false;

	TWeakObjectPtr<UEnhancedInputComponent> BoundInputComponent;
	TArray<uint32> InputBindingHandles;
	/** Snapshot of what was bound, so a swapped action rebinds even when the map size is unchanged. */
	TArray<TPair<FGameplayTag, TWeakObjectPtr<UInputAction>>> BoundSlots;
	/** Provider definitions revision the current grants were built from. */
	int32 GrantedRevision = -1;

	// RPCs
	UFUNCTION(Server, Reliable)
	void ServerActivate(UEGInteractionProviderComponent* Provider, FGameplayTag Id, FHitResult Hit, UPrimitiveComponent* HitComponent, int32 ActivationId);

	UFUNCTION(Server, Reliable)
	void ServerInputReleased(int32 ActivationId);

	UFUNCTION(Server, Reliable)
	void ServerCancel(int32 ActivationId);

	UFUNCTION(Client, Reliable)
	void ClientActivationResult(int32 ActivationId, bool bAccepted);

	UFUNCTION(Client, Reliable)
	void ClientInteractionEnded(int32 ActivationId, bool bCancelled);

	// Authority
	/** Validates a client request and builds the server instance without activating it. Null on refusal. */
	UEGInteraction* PrepareServerActivation(UEGInteractionProviderComponent* Provider, FGameplayTag Id, const FHitResult& ClientHit, UPrimitiveComponent* ClientHitComponent, int32 ActivationId);
	/** Server-derived geometry only: the client's point is a hint for where to look, never the thing measured. */
	bool ValidateServerRequest(UEGInteractionProviderComponent* Provider, FGameplayTag Id, const FHitResult& ClientHit, UPrimitiveComponent* ClientHitComponent, FHitResult& OutServerHit, FInstancedStruct& OutDefinition) const;

	// Input
	void EnsureInputBindings();
	void HandleSlotStarted(FGameplayTag InputTag);
	void HandleSlotReleased(FGameplayTag InputTag);

	// Ownership
	UFUNCTION()
	void HandleOwnerControllerChanged(APawn* Pawn, AController* OldController, AController* NewController);
	void RefreshOwnerControlState();
	bool HasAuthority() const;
	bool IsLocallyControlled() const;

	// Trace and focus
	void PerformTrace();
	void BuildQueryParams(FCollisionQueryParams& OutParams) const;
	bool HasLineOfSight(const FVector& ViewLocation, const FHitResult& Hit) const;
	void SetFocus(UEGInteractionProviderComponent* Provider, const FHitResult& Hit);
	void ClearFocus();

	// Grants
	void Regrant();
	void RevokeAll();
	void Revoke(UEGInteraction* Interaction);
	UEGInteraction* CreateInstance(UEGInteractionProviderComponent* Provider, const FInstancedStruct& Definition, const FHitResult& Hit, UPrimitiveComponent* HitComponent);
	bool ActivateInstance(UEGInteraction* Interaction);
	UEGInteraction* FindGrantedForSlot(FGameplayTag InputTag) const;
	UEGInteraction* FindByActivationId(int32 ActivationId) const;

	// Prompts
	void RebuildPrompts();

	// Debug
	void DrawDebugInfo(const FVector& Start, const FVector& End, bool bFocused) const;
};
