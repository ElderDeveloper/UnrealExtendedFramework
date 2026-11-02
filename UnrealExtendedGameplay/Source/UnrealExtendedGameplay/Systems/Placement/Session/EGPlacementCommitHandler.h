// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Async/EFPendingCalls.h"
#include "UObject/Object.h"
#include "UnrealExtendedGameplay/Systems/Placement/Session/EGPlacementSessionTypes.h"

#include "EGPlacementCommitHandler.generated.h"

class UEGPlacementSessionComponent;

/**
 * UEGPlacementCommitHandler
 * Carries out a confirmed placement. Commit answers at once with Dispatched or Refused and later
 * calls its completion exactly once with Applied, Rejected, Failed or Cancelled. A handler that
 * cannot report outcomes returns false from ReportsOutcomes; the session then treats dispatch as
 * final and never invents a success.
 *
 * Subclasses implement DispatchCommit and call CompleteCommit with the call id when the outcome is
 * known (the same frame, a later frame, after a server round trip). Instanced into a session, so
 * an instance serves one session.
 *
 * Everything that makes a commit real belongs here and in the project: permissions, inventory,
 * payment, RPCs, de-duplication, replication, saving.
 */
UCLASS(Abstract, EditInlineNew, DefaultToInstanced, CollapseCategories, Blueprintable)
class UNREALEXTENDEDGAMEPLAY_API UEGPlacementCommitHandler : public UObject
{
	GENERATED_BODY()

public:
	using FOnComplete = TFunction<void(const FEGPlacementOutcome&)>;

	/** Dispatches Request. OnComplete runs exactly once later, unless the dispatch is refused (then never). */
	FEGPlacementDispatchResult Commit(const FEGPlacementCommitRequest& Request, FOnComplete OnComplete);

	/** Reports the outcome of the call CallId. False when the call is unknown or already completed. */
	UFUNCTION(BlueprintCallable, Category = "Extended|Placement")
	bool CompleteCommit(int64 CallId, const FEGPlacementOutcome& Outcome);

	/** Completes every pending call with Cancelled. */
	UFUNCTION(BlueprintCallable, Category = "Extended|Placement")
	void CancelPendingCommits();

	UFUNCTION(BlueprintPure, Category = "Extended|Placement")
	int32 GetNumPendingCommits() const { return Pending.Num(); }

	/** Whether this handler calls CompleteCommit for its dispatches. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Extended|Placement")
	bool ReportsOutcomes() const;
	virtual bool ReportsOutcomes_Implementation() const { return true; }

	UFUNCTION(BlueprintNativeEvent, Category = "Extended|Placement")
	void OnSessionBegin(UEGPlacementSessionComponent* Session, const FEGPlacementSelection& Selection);
	virtual void OnSessionBegin_Implementation(UEGPlacementSessionComponent* Session, const FEGPlacementSelection& Selection) {}

	/** Every preview update while previewing. */
	UFUNCTION(BlueprintNativeEvent, Category = "Extended|Placement")
	void OnPreviewUpdated(UEGPlacementSessionComponent* Session, const FEGPlacementEvaluation& Evaluation, FGameplayTag ModeTag);
	virtual void OnPreviewUpdated_Implementation(UEGPlacementSessionComponent* Session, const FEGPlacementEvaluation& Evaluation, FGameplayTag ModeTag) {}

	UFUNCTION(BlueprintNativeEvent, Category = "Extended|Placement")
	void OnSessionEnd(UEGPlacementSessionComponent* Session, bool bCancelled);
	virtual void OnSessionEnd_Implementation(UEGPlacementSessionComponent* Session, bool bCancelled) {}

	virtual void BeginDestroy() override;

protected:
	/** Do the work for CallId; call CompleteCommit(CallId, ...) when the outcome is known (it may be inside this call). */
	UFUNCTION(BlueprintNativeEvent, Category = "Extended|Placement")
	FEGPlacementDispatchResult DispatchCommit(const FEGPlacementCommitRequest& Request, int64 CallId);
	virtual FEGPlacementDispatchResult DispatchCommit_Implementation(const FEGPlacementCommitRequest& Request, int64 CallId);

	/** Whether the request's world lets this machine change the world itself. */
	static bool HasLocalAuthority(const FEGPlacementCommitRequest& Request);

private:
	TEFPendingCalls<const FEGPlacementOutcome&> Pending;

	/** Outcomes completed inside DispatchCommit, delivered after Commit returns so the caller always sees Dispatched first. */
	TArray<TPair<int64, FEGPlacementOutcome>> EarlyOutcomes;
	int64 DispatchingCallId = 0;
};

/** Spawns ActorClass (the request's, else DefaultActorClass) at the pose. Needs authority; networked clients route through the project's command. */
UCLASS(meta = (DisplayName = "Spawn actor"))
class UNREALEXTENDEDGAMEPLAY_API UEGPlacementCommit_SpawnActor : public UEGPlacementCommitHandler
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	TSoftClassPtr<AActor> DefaultActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	ESpawnActorCollisionHandlingMethod CollisionHandling = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

protected:
	virtual FEGPlacementDispatchResult DispatchCommit_Implementation(const FEGPlacementCommitRequest& Request, int64 CallId) override;
};

/**
 * Moves the request's MovedActor in place. With bMoveWhilePreviewing the actor follows the valid
 * preview and is restored to where it started when the session is cancelled. Needs authority.
 */
UCLASS(meta = (DisplayName = "Move actor"))
class UNREALEXTENDEDGAMEPLAY_API UEGPlacementCommit_MoveActor : public UEGPlacementCommitHandler
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	bool bMoveWhilePreviewing = false;

	virtual void OnSessionBegin_Implementation(UEGPlacementSessionComponent* Session, const FEGPlacementSelection& Selection) override;
	virtual void OnPreviewUpdated_Implementation(UEGPlacementSessionComponent* Session, const FEGPlacementEvaluation& Evaluation, FGameplayTag ModeTag) override;
	virtual void OnSessionEnd_Implementation(UEGPlacementSessionComponent* Session, bool bCancelled) override;

protected:
	virtual FEGPlacementDispatchResult DispatchCommit_Implementation(const FEGPlacementCommitRequest& Request, int64 CallId) override;

private:
	TWeakObjectPtr<AActor> Moving;
	FTransform Origin = FTransform::Identity;
	bool bCommitted = false;
	bool bOriginStored = false;
};
