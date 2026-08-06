// Copyright Devil of the Plague. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "EGQuestFactsSubsystem.h"
#include "EGQuestTypes.h"
#include "EGQuestRunActor.generated.h"

class APlayerState;
class UEGQuestComponent;

/**
 * The logic and state of one quest run. One instance per run, spawned by UEGQuestComponent when the
 * graph names a QuestRunActorClass, destroyed when the run reaches a terminal state.
 *
 * Assign a subclass to the quest asset's QuestRunActorClass and script against the lifecycle:
 * OnQuestStarted, OnStageEntered/OnStageExited, OnObjectiveProgress, OnObjectiveResolved, OnRoleLost,
 * OnQuestEnded. The run actor may drive the quest back through the helpers below - they go through the
 * same authority-only component API everything else uses.
 *
 * Prefer EmitQuestEvent over CompleteObjective: an emitted event flows through the accepted-event
 * pipeline, so the authored trackers count it, milestones fire, and analytics stay truthful.
 * CompleteObjective/FailObjective are hard overrides for when the graph must be forced.
 *
 * **This is an actor, on purpose.** Tick, components, timers, overlaps and ordinary replicated
 * properties are all available, and per-run state that clients need does not have to be squeezed
 * through the snapshot or encoded into integer facts - replicate it here and read it directly off
 * UEGQuestComponent::GetRunActor.
 *
 * **It exists on clients, also on purpose.** Every lifecycle hook fires on the authority *and* on each
 * client the run is relevant to, so per-client work (spawning a local effect for a private quest, say)
 * is just work in a hook. Check HasAuthority() where a hook should only do one side. What stays
 * authority-only is quest *state*: EmitQuestEvent, CompleteObjective, SetObjectiveRequiredCount and the
 * fact setters are gated at the component, so a client reacts but never decides.
 *
 * Relevancy follows the run: a shared run is always relevant, a private run is owned by its
 * PlayerState and relevant only to that connection.
 *
 * Nothing here is saved. The plugin persists only the fact ledger; a run actor that wants durable
 * state writes facts, or the game saves whatever it likes off this actor in its own SaveGame.
 */
UCLASS(Abstract, Blueprintable, BlueprintType, ClassGroup = "Quest", NotPlaceable, Transient)
class UNREALEXTENDEDQUEST_API AEGQuestRunActor : public AActor
{
	GENERATED_BODY()

public:
	AEGQuestRunActor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Called by the owning component immediately after spawn, before FinishSpawning. */
	void Initialize(UEGQuestComponent* InOwnerComponent, const FGuid& InQuestInstanceGuid);

	//
	// Entry points for the owning component. Authority side; each fires the matching hook below and
	// lets replication carry the same hook to the clients that can see this run.
	//

	void HandleQuestStarted();
	void HandleStageEntered(const FGuid& StageGuid, FName StageId, const FText& StageTitle);
	void HandleStageExited(const FGuid& StageGuid, FName StageId, EEGQuestStageExitReason Reason);
	void HandleObjectiveProgress(const FEGQuestSnapshotObjective& Objective);
	void HandleObjectiveResolved(const FEGQuestSnapshotObjective& Objective);
	void HandleRoleLost(FName RoleName);
	/** Fires OnQuestEnded, then lets the actor tear itself down once the result has replicated. */
	void HandleQuestEnded(EEGQuestResult Result);

	//
	// What the run actor knows.
	//

	/** The component running this quest instance. Its delegates are bindable from here. */
	UFUNCTION(BlueprintPure, Category = "Quest Run")
	UEGQuestComponent* GetQuestComponent() const { return QuestComponent; }

	UFUNCTION(BlueprintPure, Category = "Quest Run")
	FGuid GetQuestInstanceGuid() const { return QuestInstanceGuid; }

	/** The current snapshot of this quest instance: the journal the players see. Valid on clients. */
	UFUNCTION(BlueprintPure, Category = "Quest Run")
	bool GetQuestSnapshot(FEGQuestViewSnapshot& OutSnapshot) const;

	//
	// Facts: the ledger of what has happened. The one thing the plugin persists.
	//

	UFUNCTION(BlueprintPure, Category = "Quest Run|Facts")
	int32 GetFact(FGameplayTag Tag, EEGQuestFactScope Scope = EEGQuestFactScope::World, APlayerState* Player = nullptr) const;

	UFUNCTION(BlueprintPure, Category = "Quest Run|Facts")
	bool HasFact(FGameplayTag Tag, EEGQuestFactScope Scope = EEGQuestFactScope::World, APlayerState* Player = nullptr) const;

	UFUNCTION(BlueprintCallable, Category = "Quest Run|Facts")
	bool SetFact(FGameplayTag Tag, int32 Value, EEGQuestFactScope Scope = EEGQuestFactScope::World, APlayerState* Player = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Quest Run|Facts")
	bool AddToFact(FGameplayTag Tag, int32 Delta, EEGQuestFactScope Scope = EEGQuestFactScope::World, APlayerState* Player = nullptr);

	//
	// Roles: reaching the entities this quest instance resolved.
	//

	/** Live actors currently bound to a role of this instance. Unresolvable handles are skipped. */
	UFUNCTION(BlueprintPure, Category = "Quest Run|Roles")
	TArray<AActor*> ResolveRoleActors(FName RoleName) const;

	/** The first bound entity's transform (also answers for unloaded targets that registered one). */
	UFUNCTION(BlueprintPure, Category = "Quest Run|Roles")
	bool GetRoleTransform(FName RoleName, FTransform& OutTransform) const;

	UFUNCTION(BlueprintPure, Category = "Quest Run|Roles")
	FText GetRoleDisplayText(FName RoleName) const;

	//
	// What the run actor may do. Thin wrappers over the component's authority-only API, already
	// scoped to this quest instance. They no-op on a client, by the component's own gating.
	//

	/**
	 * Feeds a gameplay event into the quest event pipeline, exactly as authoritative gameplay code
	 * would. Trackers count it, magnitudes apply, analytics see it. Prefer this over the hard
	 * overrides below.
	 */
	UFUNCTION(BlueprintCallable, Category = "Quest Run")
	bool EmitQuestEvent(FGameplayTag EventTag, float Magnitude = 1.0f, FName ContextName = NAME_None);

	/** Succeeds an objective of the active stage. A hard override: trackers are bypassed. */
	UFUNCTION(BlueprintCallable, Category = "Quest Run")
	bool CompleteObjective(FGuid ObjectiveGuid);

	/** Fails an objective of the active stage. Rejected when the objective has no fail routing. */
	UFUNCTION(BlueprintCallable, Category = "Quest Run")
	bool FailObjective(FGuid ObjectiveGuid);

	/** Overrides one objective's authored RequiredCount for this instance. Pass 0 to restore it. */
	UFUNCTION(BlueprintCallable, Category = "Quest Run")
	bool SetObjectiveRequiredCount(FGuid ObjectiveGuid, int32 RequiredCount);

	/** Scales every objective of this quest counting EventTag. Returns the objectives it scaled. */
	UFUNCTION(BlueprintCallable, Category = "Quest Run")
	TArray<FGuid> SetObjectiveRequiredCountByEventTag(FGameplayTag EventTag, int32 RequiredCount);

protected:
	//
	// The lifecycle, in the order it arrives. Fires on the authority and on every client the run is
	// relevant to. Check HasAuthority() inside a hook that should only run on one side.
	//

	/** The quest instance started. On a client, fires as soon as this actor and its binding arrive. */
	UFUNCTION(BlueprintNativeEvent, Category = "Quest Run")
	void OnQuestStarted();
	virtual void OnQuestStarted_Implementation() {}

	/**
	 * A stage became active and the snapshot already describes it. StageId is the stage card's
	 * authored script-facing name (may be None). A client that joins mid-run receives this for the
	 * stage that is already active, so per-stage setup catches up without a special case.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Quest Run")
	void OnStageEntered(const FGuid& StageGuid, FName StageId, const FText& StageTitle);
	virtual void OnStageEntered_Implementation(const FGuid& StageGuid, FName StageId, const FText& StageTitle) {}

	/**
	 * The stage stopped being active - it advanced, or the quest ended while it was active (check
	 * Reason). The mirror of OnStageEntered: fires on every exit path, so per-stage setup made in
	 * OnStageEntered has exactly one place to be torn down.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Quest Run")
	void OnStageExited(const FGuid& StageGuid, FName StageId, EEGQuestStageExitReason Reason);
	virtual void OnStageExited_Implementation(const FGuid& StageGuid, FName StageId, EEGQuestStageExitReason Reason) {}

	/** An objective's progress count changed without resolving it. Fires after the snapshot updated. */
	UFUNCTION(BlueprintNativeEvent, Category = "Quest Run")
	void OnObjectiveProgress(const FEGQuestSnapshotObjective& Objective);
	virtual void OnObjectiveProgress_Implementation(const FEGQuestSnapshotObjective& Objective) {}

	/**
	 * An objective of the active stage reached an outcome (check Objective.Outcome for which).
	 * Objectives that complete on stage enter fire this too, right after OnStageEntered.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Quest Run")
	void OnObjectiveResolved(const FEGQuestSnapshotObjective& Objective);
	virtual void OnObjectiveResolved_Implementation(const FEGQuestSnapshotObjective& Objective) {}

	/**
	 * A role with the Notify loss policy lost its bound entity. Roles with ReResolve/Suspend policies
	 * handle loss themselves and do not fire this. Authority only - role bindings are not replicated.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Quest Run")
	void OnRoleLost(FName RoleName);
	virtual void OnRoleLost_Implementation(FName RoleName) {}

	/** The quest instance reached a terminal state. The actor is destroyed shortly after. */
	UFUNCTION(BlueprintNativeEvent, Category = "Quest Run")
	void OnQuestEnded(EEGQuestResult Result);
	virtual void OnQuestEnded_Implementation(EEGQuestResult Result) {}

private:
	/** Set on the authority at spawn; both are needed before a client can track anything. */
	UPROPERTY(ReplicatedUsing = OnRep_QuestBinding)
	TObjectPtr<UEGQuestComponent> QuestComponent;

	UPROPERTY(ReplicatedUsing = OnRep_QuestBinding)
	FGuid QuestInstanceGuid;

	/**
	 * Carries the outcome to clients independently of the snapshot: a terminal run's snapshot is
	 * purged from the replicated array, so the end hook cannot be derived from it.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_RunEnded)
	EEGQuestResult FinalResult = EEGQuestResult::Completed;

	UPROPERTY(ReplicatedUsing = OnRep_RunEnded)
	bool bRunEnded = false;

	UFUNCTION()
	void OnRep_QuestBinding();

	UFUNCTION()
	void OnRep_RunEnded();

	/** Bound on clients once the binding resolves; the snapshot is what drives their hooks. */
	UFUNCTION()
	void HandleSnapshotsChanged();

	void BeginClientTracking();
	void EndClientTracking();
	/** Diffs the snapshot against what this client last saw and fires the hooks for the difference. */
	void SyncClientState();
	void DispatchQuestEnded(EEGQuestResult Result);

	/** Client-side diff baseline. Unused on the authority, which is told about changes directly. */
	FGuid LastStageGuid;
	FName LastStageId = NAME_None;
	TArray<FEGQuestSnapshotObjective> LastObjectives;
	bool bClientTrackingBound = false;
	bool bQuestStartedDispatched = false;
	bool bQuestEndedDispatched = false;
};
