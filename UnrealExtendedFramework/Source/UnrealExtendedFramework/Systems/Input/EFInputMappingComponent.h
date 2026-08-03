// Fill out your copyright notice in the Description page of Project Settings.
// ─────────────────────────────────────────────────────────────────────────────
// EFInputMappingComponent.h
//
// Owns which UInputMappingContext the local player is driving right now.
//
// One list, one winner. Every push — persistent or temporary — becomes a row in
// InputMappingStack. The row with the highest Priority is applied to the
// Enhanced Input subsystem and NOTHING else is. Contexts never overlap. When
// the winner is removed the next-highest row takes over automatically, which is
// why losing rows stay in the list instead of being discarded.
//
// Ties break toward the most recently added row, so pushing a second context at
// the same tier behaves like a stack.
//
// Mapping contexts live on ULocalPlayer, so none of this replicates. The
// component simply refuses to touch the subsystem unless the owner is locally
// controlled, and re-applies itself when possession changes.
//
// This class is mapping routing only. It deliberately owns no InputActions and
// no bindings — those belong to the game module subclassing it.
// ─────────────────────────────────────────────────────────────────────────────

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Systems/Input/EFInputMappingTypes.h"
#include "EFInputMappingComponent.generated.h"

class AController;
class APawn;
class UEnhancedInputLocalPlayerSubsystem;
class UInputMappingContext;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEFOnActiveInputMappingChanged, UInputMappingContext*, NewMappingContext, UInputMappingContext*, PreviousMappingContext);

UCLASS(ClassGroup=(ExtendedFramework), meta=(BlueprintSpawnableComponent), Blueprintable, BlueprintType)
class UNREALEXTENDEDFRAMEWORK_API UEFInputMappingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEFInputMappingComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ── Persistent entries ───────────────────────────────────────────────────
	// Keyed by mapping context. Re-adding the same context re-prioritizes it
	// rather than duplicating it.

	UFUNCTION(BlueprintCallable, Category="ExtendedInput")
	bool AddManagedInputMapping(const FEFInputMappingSpec& MappingSpec);

	UFUNCTION(BlueprintCallable, Category="ExtendedInput")
	bool RemoveManagedInputMapping(UInputMappingContext* MappingContext);

	// ── Temporary entries ────────────────────────────────────────────────────
	// Keyed by handle. Source is tracked weakly so a destroyed pusher cannot
	// strand its mapping.

	UFUNCTION(BlueprintCallable, Category="ExtendedInput")
	bool ApplyInputMappingOverride(UObject* Source, const TArray<FEFInputMappingSpec>& MappingSpecs, FEFInputMappingHandle& OutHandle);

	UFUNCTION(BlueprintCallable, Category="ExtendedInput")
	bool ReleaseInputMappingOverride(UPARAM(ref) FEFInputMappingHandle& Handle);

	UFUNCTION(BlueprintCallable, Category="ExtendedInput")
	int32 ReleaseInputMappingOverridesFromSource(UObject* Source);

	UFUNCTION(BlueprintCallable, Category="ExtendedInput")
	void ClearInputMappings();

	/** Re-resolve the winner and reconcile it against what is actually applied. Idempotent. */
	UFUNCTION(BlueprintCallable, Category="ExtendedInput")
	void RefreshInputMappings();

	// ── Queries ──────────────────────────────────────────────────────────────

	/** The one context currently applied, or null if none is. */
	UFUNCTION(BlueprintPure, Category="ExtendedInput")
	UInputMappingContext* GetActiveInputMapping() const;

	/** Index into GetInputMappingStack() of the winning entry, INDEX_NONE if none. */
	UFUNCTION(BlueprintPure, Category="ExtendedInput")
	FORCEINLINE int32 GetActiveInputMappingIndex() const { return ActiveInputMappingIndex; }

	UFUNCTION(BlueprintPure, Category="ExtendedInput")
	int32 GetActiveInputMappingPriority() const;

	/** Every entry in push order — index 0 is the oldest, not the winner. */
	UFUNCTION(BlueprintPure, Category="ExtendedInput")
	TArray<FEFInputMappingSpec> GetInputMappingStack() const;

	UFUNCTION(BlueprintPure, Category="ExtendedInput")
	bool HasInputMapping(UInputMappingContext* MappingContext) const;

	UFUNCTION(BlueprintPure, Category="ExtendedInput")
	bool IsInputMappingHandleActive(const FEFInputMappingHandle& Handle) const;

	UPROPERTY(BlueprintAssignable, Category="ExtendedInput")
	FEFOnActiveInputMappingChanged OnActiveInputMappingChanged;

protected:
	/** Seeded into the list as managed entries on BeginPlay. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="ExtendedInput")
	TArray<FEFInputMappingSpec> DefaultInputMappings;

	/**
	 * Null whenever the owner has no local input surface — remote proxy,
	 * dedicated server, or not yet possessed. Callers must treat null as
	 * "try again after possession", not as an error.
	 */
	virtual UEnhancedInputLocalPlayerSubsystem* GetEnhancedInputSubsystem() const;

	/** Fires after the applied context actually changed, before the delegate. */
	virtual void NotifyActiveInputMappingChanged(UInputMappingContext* NewMappingContext, UInputMappingContext* PreviousMappingContext);

	/** Next tick after the owner's controller changes. Subclasses rebind actions here. */
	virtual void OnOwnerControllerChanged(AController* OldController, AController* NewController);

	/** Coalesced next-tick RefreshInputMappings, for ordering-sensitive callers. */
	void ScheduleRefreshInputMappings();

	int32 FindWinningEntryIndex() const;

	UPROPERTY(Transient)
	TArray<FEFInputMappingEntry> InputMappingStack;

	/** Mirror of the winner for PIE/debugger inspection. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="ExtendedInput|Debug")
	int32 ActiveInputMappingIndex = INDEX_NONE;

private:
	UFUNCTION()
	void HandleOwnerControllerChanged(APawn* Pawn, AController* OldController, AController* NewController);

	/** Always removes from AppliedInputSubsystem, never from the current one. */
	void RemoveAppliedInputMapping();

	void PruneStaleEntries();

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> AppliedMappingContext = nullptr;

	/**
	 * The subsystem AppliedMappingContext was pushed to. Kept separately so a
	 * possession change removes from the OLD local player rather than leaking a
	 * context onto whoever the owner belongs to now.
	 */
	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> AppliedInputSubsystem;

	int32 AppliedPriority = 0;
	int32 NextInputMappingHandleId = 1;
	bool bRefreshScheduled = false;
};
