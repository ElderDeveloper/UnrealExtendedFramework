// Copyright Devil of the Plague. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GameplayTagContainer.h"
#include "EGQuestFactsSubsystem.h"
#include "EGQuestTargetRegistry.h"
#include "EGQuestTypes.h"
#include "EGQuestScript.generated.h"

class AActor;
class APlayerState;
class UEGQuestComponent;

/**
 * The event graph of a quest: one instance of this class runs per quest instance, on the authority.
 *
 * Assign a Blueprint subclass to the quest asset's QuestScriptClass and script against the
 * lifecycle: OnQuestStarted, OnStageEntered/OnStageExited, OnObjectiveProgress,
 * OnObjectiveResolved, OnRoleLost, OnQuestEnded. The script may drive the quest back through the
 * helpers below - they go through the same authority-only component API everything else uses.
 *
 * Prefer EmitQuestEvent over CompleteObjective: an emitted event flows through the accepted-event
 * pipeline, so the authored trackers count it, milestones fire, and analytics stay truthful.
 * CompleteObjective/FailObjective are hard overrides for when the graph must be forced.
 *
 * SERVER ONLY. The script never exists on clients: anything a player must see has to reach them
 * through the replicated snapshots or the relayed GameplayNotify pipeline, never from here.
 *
 * Scripts are transient and are not saved: a resumed quest gets a fresh instance. The resume
 * contract is: OnQuestResumed fires first, then OnStageEntered replays for the restored active
 * stage - so per-stage logic belongs in OnStageEntered and survives save/load for free, and
 * OnQuestResumed is only for resume-specific rebuilding. Durable memory belongs in facts
 * (SetFact/GetFact), never in script members.
 */
UCLASS(Abstract, Blueprintable, BlueprintType, ClassGroup = "Quest")
class UNREALEXTENDEDQUEST_API UEGQuestScript : public UObject
{
	GENERATED_BODY()

public:
	// Lets Blueprint world-context nodes (timers, spawns, gameplay statics) work inside the script.
	virtual UWorld* GetWorld() const override;

	/** Called by the owning component right after construction, before any lifecycle event. */
	void Initialize(UEGQuestComponent* InOwnerComponent, const FGuid& InQuestInstanceGuid);

	//
	// Entry points for the owning component. These call the Blueprint events below.
	//

	void HandleQuestStarted();
	void HandleQuestResumed();
	void HandleStageEntered(const FGuid& StageGuid, FName StageId, const FText& StageTitle);
	void HandleStageExited(const FGuid& StageGuid, FName StageId, EEGQuestStageExitReason Reason);
	void HandleObjectiveProgress(const FEGQuestSnapshotObjective& Objective);
	void HandleObjectiveResolved(const FEGQuestSnapshotObjective& Objective);
	void HandleRoleLost(FName RoleName);
	void HandleQuestEnded(EEGQuestResult Result);

	//
	// What the script knows.
	//

	/** The component running this quest instance. Its delegates (e.g. OnGameplayEventAccepted) are bindable from the script. */
	UFUNCTION(BlueprintPure, Category = "Quest Script")
	UEGQuestComponent* GetQuestComponent() const;

	UFUNCTION(BlueprintPure, Category = "Quest Script")
	FGuid GetQuestInstanceGuid() const { return QuestInstanceGuid; }

	/** The current replicated snapshot of this quest instance: the journal the players see. */
	UFUNCTION(BlueprintPure, Category = "Quest Script")
	bool GetQuestSnapshot(FEGQuestViewSnapshot& OutSnapshot) const;

	//
	// Facts: the sanctioned way for a script to remember something. The facts subsystem owns
	// durability and save/restore; the script itself stays stateless.
	//

	UFUNCTION(BlueprintPure, Category = "Quest Script|Facts")
	int32 GetFact(FGameplayTag Tag, EEGQuestFactScope Scope = EEGQuestFactScope::World, APlayerState* Player = nullptr) const;

	UFUNCTION(BlueprintPure, Category = "Quest Script|Facts")
	bool HasFact(FGameplayTag Tag, EEGQuestFactScope Scope = EEGQuestFactScope::World, APlayerState* Player = nullptr) const;

	UFUNCTION(BlueprintCallable, Category = "Quest Script|Facts")
	bool SetFact(FGameplayTag Tag, int32 Value, EEGQuestFactScope Scope = EEGQuestFactScope::World, APlayerState* Player = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Quest Script|Facts")
	bool AddToFact(FGameplayTag Tag, int32 Delta, EEGQuestFactScope Scope = EEGQuestFactScope::World, APlayerState* Player = nullptr);

	//
	// Roles: reaching the entities this quest instance resolved.
	//

	/** Live actors currently bound to a role of this instance. Unresolvable handles are skipped. */
	UFUNCTION(BlueprintPure, Category = "Quest Script|Roles")
	TArray<AActor*> ResolveRoleActors(FName RoleName) const;

	/** The first bound entity's transform (also answers for unloaded targets that registered one). */
	UFUNCTION(BlueprintPure, Category = "Quest Script|Roles")
	bool GetRoleTransform(FName RoleName, FTransform& OutTransform) const;

	UFUNCTION(BlueprintPure, Category = "Quest Script|Roles")
	FText GetRoleDisplayText(FName RoleName) const;

	//
	// What the script may do. Thin wrappers over the component's authority-only API,
	// already scoped to this quest instance.
	//

	/**
	 * Feeds a gameplay event into the quest event pipeline, exactly as authoritative gameplay code
	 * would. Trackers count it, magnitudes apply, analytics see it. Prefer this over the hard
	 * overrides below.
	 */
	UFUNCTION(BlueprintCallable, Category = "Quest Script")
	bool EmitQuestEvent(FGameplayTag EventTag, float Magnitude = 1.0f, FName ContextName = NAME_None);

	/** Succeeds an objective of the active stage. A hard override: trackers are bypassed. */
	UFUNCTION(BlueprintCallable, Category = "Quest Script")
	bool CompleteObjective(FGuid ObjectiveGuid);

	/** Fails an objective of the active stage. Rejected when the objective has no fail routing. */
	UFUNCTION(BlueprintCallable, Category = "Quest Script")
	bool FailObjective(FGuid ObjectiveGuid);

	/** Overrides one objective's authored RequiredCount for this instance. Pass 0 to restore the authored count. */
	UFUNCTION(BlueprintCallable, Category = "Quest Script")
	bool SetObjectiveRequiredCount(FGuid ObjectiveGuid, int32 RequiredCount);

	/** Scales every objective of this quest counting EventTag. Returns the objectives it scaled. */
	UFUNCTION(BlueprintCallable, Category = "Quest Script")
	TArray<FGuid> SetObjectiveRequiredCountByEventTag(FGameplayTag EventTag, int32 RequiredCount);

protected:
	//
	// The lifecycle, in the order it arrives. All of it runs on the authority only.
	//

	/** The quest instance just started for the first time. Fires before the first OnStageEntered. */
	UFUNCTION(BlueprintNativeEvent, Category = "Quest Script")
	void OnQuestStarted();
	virtual void OnQuestStarted_Implementation() {}

	/** The quest instance was resumed from a save. Fires before OnStageEntered for the resumed stage. */
	UFUNCTION(BlueprintNativeEvent, Category = "Quest Script")
	void OnQuestResumed();
	virtual void OnQuestResumed_Implementation() {}

	/**
	 * A stage became active and the snapshot already describes it: reading the quest back sees this
	 * stage. StageId is the stage card's authored script-facing name (may be None).
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Quest Script")
	void OnStageEntered(const FGuid& StageGuid, FName StageId, const FText& StageTitle);
	virtual void OnStageEntered_Implementation(const FGuid& StageGuid, FName StageId, const FText& StageTitle) {}

	/**
	 * The stage stopped being active - it advanced, or the quest ended while it was active (check
	 * Reason). The mirror of OnStageEntered: fires on every exit path, so per-stage setup made in
	 * OnStageEntered has exactly one place to be torn down.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Quest Script")
	void OnStageExited(const FGuid& StageGuid, FName StageId, EEGQuestStageExitReason Reason);
	virtual void OnStageExited_Implementation(const FGuid& StageGuid, FName StageId, EEGQuestStageExitReason Reason) {}

	/** An objective's progress count changed without resolving it. Fires after the snapshot updated. */
	UFUNCTION(BlueprintNativeEvent, Category = "Quest Script")
	void OnObjectiveProgress(const FEGQuestSnapshotObjective& Objective);
	virtual void OnObjectiveProgress_Implementation(const FEGQuestSnapshotObjective& Objective) {}

	/**
	 * An objective of the active stage reached an outcome (check Objective.Outcome for which).
	 * Objectives that complete on stage enter fire this too, right after OnStageEntered.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Quest Script")
	void OnObjectiveResolved(const FEGQuestSnapshotObjective& Objective);
	virtual void OnObjectiveResolved_Implementation(const FEGQuestSnapshotObjective& Objective) {}

	/**
	 * A role with the Notify loss policy lost its bound entity (destroyed, unloaded, unregistered).
	 * Roles with ReResolve/Suspend policies handle loss themselves and do not fire this.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Quest Script")
	void OnRoleLost(FName RoleName);
	virtual void OnRoleLost_Implementation(FName RoleName) {}

	/** The quest instance reached a terminal state. The script is discarded right after this returns. */
	UFUNCTION(BlueprintNativeEvent, Category = "Quest Script")
	void OnQuestEnded(EEGQuestResult Result);
	virtual void OnQuestEnded_Implementation(EEGQuestResult Result) {}

private:
	UEGQuestFactsSubsystem* GetFactsSubsystem() const;

	TWeakObjectPtr<UEGQuestComponent> OwnerComponent;
	FGuid QuestInstanceGuid;
};
