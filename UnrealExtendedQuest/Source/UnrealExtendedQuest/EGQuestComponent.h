// Copyright Devil of the Plague. All Rights Reserved.
#pragma once

#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "GameplayTagContainer.h"
#include "EGQuestTypes.h"
#include "EGQuestClock.h"
#include "EGQuestDirective.h"
#include "Templates/Function.h"
#include "EGQuestComponent.generated.h"

class UEGQuestContext;
class UEGQuestGraph;
class UEGQuestNode_Objective;
class UEGQuestNode_Stage;
class UEGQuestScript;
class UEGQuestEventCustom;

/**
 * The transient objective evaluators of one quest instance's active stage, in authored order.
 * Authority only, never replicated: each entry is a duplicate of an authored objective node,
 * activated so it can evaluate itself and report back with CompleteObjective.
 */
USTRUCT()
struct FEGQuestObjectiveEvaluatorSet
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TArray<TObjectPtr<UEGQuestNode_Objective>> Evaluators;
};

USTRUCT()
struct FEGQuestActiveTrackRuntime
{
	GENERATED_BODY()
	UPROPERTY(Transient) FName TrackName = NAME_None;
	UPROPERTY(Transient) EEGQuestTrackType TrackType = EEGQuestTrackType::Sentinel;
	UPROPERTY(Transient) TObjectPtr<UEGQuestContext> Context = nullptr;
	UPROPERTY(Transient) FEGQuestObjectiveEvaluatorSet Evaluators;
};

USTRUCT()
struct FEGQuestActiveTrackRuntimeSet
{
	GENERATED_BODY()
	UPROPERTY(Transient) TArray<FEGQuestActiveTrackRuntime> Tracks;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FEGQuestSnapshotsChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEGQuestInstanceEvent, FGuid, QuestInstanceGuid);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEGQuestGameplayEventAccepted, const FEGQuestGameplayEvent&, Event);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEGQuestRequestRejected, FGuid, QuestInstanceGuid, FName, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FEGQuestGameplayNotify, FGuid, QuestInstanceGuid, FGameplayTag, NotifyTag, float, Magnitude);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FEGQuestRoleLost, FGuid, QuestInstanceGuid, FName, RoleName, FEGQuestEntityHandle, Handle);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FEGQuestTelemetry, EEGQuestTelemetryEventType, EventType,
	FName, DefinitionId, FGuid, RunId, FGuid, ElementGuid, double, ServerTime);

/**
 * Server-authoritative quest execution owner. Clients consume immutable replicated snapshots.
 *
 * This is where the stage model actually runs. A quest has exactly one active stage; the stage's
 * objectives progress independently, and when an objective resolves this component asks whether any
 * destination is now fully satisfied - every arrow pointing into it matched. The first such
 * destination fires, the stage is left, and everything still pending in it is cancelled.
 *
 * Delegate timing: on the authority (including the listen-server host) delegates fire synchronously
 * inside the mutating call; on remote clients they fire from replication callbacks, possibly
 * coalescing several server-side changes into one notification. UI code must treat every
 * notification as "re-read the snapshot", never as "exactly one server mutation happened".
 */
UCLASS(BlueprintType, ClassGroup = (Quest), meta = (BlueprintSpawnableComponent))
class UNREALEXTENDEDQUEST_API UEGQuestComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEGQuestComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * Seconds between automatic PulseActiveTrackers calls while this component has a live run.
	 * Zero or less disables the driver entirely.
	 *
	 * This component never ticks: fact- and event-driven trackers wake themselves, but a timer
	 * tracker and a predicate that declares no fact dependencies can only be reevaluated by a pulse.
	 * Without this driver they are authorable but never fire. The timer is authority-only and only
	 * runs while a non-terminal run exists, so an idle component costs nothing.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest|Tracker", Meta = (ClampMin = "0.0", Units = "s"))
	float TrackerPulseInterval = 1.0f;

	UFUNCTION(BlueprintPure, Category = "Quest")
	const TArray<FEGQuestViewSnapshot>& GetSharedQuestSnapshots() const { return SharedQuestSnapshots.Items; }
	UFUNCTION(BlueprintPure, Category = "Quest")
	const TArray<FEGQuestViewSnapshot>& GetPrivateQuestSnapshots() const { return PrivateQuestSnapshots.Items; }
	UFUNCTION(BlueprintPure, Category = "Quest")
	bool FindQuestSnapshot(FGuid QuestInstanceGuid, FEGQuestViewSnapshot& OutSnapshot) const;

	/**
	 * Replaces the time source this component stamps snapshots with, for tests and the
	 * simulator. Pass null to go back to the default. Authority-side timing only.
	 *
	 * There is no "install the default clock" call, and that is the point: with no injected clock,
	 * GetServerTime re-reads the world on every call, exactly as it does today. A default clock
	 * cached on first use would capture whatever world that first call saw - and if it saw none
	 * (a component ticked during travel, a CDO, a test with no world yet) the component would return
	 * 0.0 for the rest of its life instead of recovering on the next call. So an injected clock is
	 * cached, because the injector owns it and it is answerable by construction; the default is not,
	 * because it is only correct when re-derived. FEGQuestWorldClock exists for anyone who needs the
	 * default behaviour behind the interface, but this component never constructs one.
	 */
	void SetQuestClock(const TSharedPtr<IEGQuestClock>& InClock) { QuestClock = InClock; }

	/** The only time source trackers and simulation may read. */
	UFUNCTION(BlueprintPure, Category = "Quest|Time")
	double GetQuestServerTime() const { return GetServerTime(); }

	/** Owner policy. Override to host shared quests on a custom always-relevant replicated actor. */
	virtual bool CanHostSharedQuests() const;
	/** Owner policy. Override to host private quests on something other than a PlayerState. */
	virtual bool CanHostPrivateQuests() const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Quest")
	FEGQuestOperationResult StartSharedQuest(UEGQuestGraph* QuestGraph);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Quest")
	FEGQuestOperationResult StartPrivateQuest(UEGQuestGraph* QuestGraph);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Quest|Template")
	FEGQuestOperationResult StartQuestFromTemplate(UEGQuestGraph* QuestGraph, const FEGQuestTemplateParameters& Parameters);
	UFUNCTION(BlueprintPure, Category = "Quest|Roles")
	FText GetRoleDisplayText(FGuid QuestInstanceGuid, FName RoleName) const;
	/** Live actors currently bound to a role of this instance. Unresolvable handles are skipped. */
	UFUNCTION(BlueprintPure, Category = "Quest|Roles")
	TArray<AActor*> ResolveRoleActors(FGuid QuestInstanceGuid, FName RoleName) const;
	/** The first bound entity's transform (also answers for unloaded targets that registered one). */
	UFUNCTION(BlueprintPure, Category = "Quest|Roles")
	bool GetRoleTransform(FGuid QuestInstanceGuid, FName RoleName, FTransform& OutTransform) const;
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Quest|Save")
	bool BuildQuestSaveData(FGuid QuestInstanceGuid, FEGQuestSaveEnvelope& OutSaveData) const;
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Quest|Save")
	FEGQuestOperationResult ResumeQuest(const FEGQuestSaveEnvelope& SaveData);

	/** Save payloads for every non-terminal quest on this component. Call before map travel. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Quest|Save")
	TArray<FEGQuestSaveEnvelope> ExtractAllQuestSaveData() const;
	/**
	 * Resumes every payload. When bReplaceExisting is true, colliding run ids (active or terminal
	 * history) are purged before resume so travel restore is idempotent.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Quest|Save")
	TArray<FEGQuestOperationResult> RestoreAllQuestSaveData(const TArray<FEGQuestSaveEnvelope>& SaveData, bool bReplaceExisting = true);

	/**
	 * Succeeds an objective of the active stage from authoritative gameplay code.
	 * ObjectiveGuid identifies which objective and doubles as the staleness check: an objective that
	 * is not in the active stage, or is already resolved, is rejected.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Quest")
	FEGQuestOperationResult CompleteActiveObjective(FGuid QuestInstanceGuid, FGuid ObjectiveGuid);

	/**
	 * As CompleteActiveObjective, but fails the objective. Rejected when the objective has no fail
	 * routing (UEGQuestNode_Objective::CanEverFail): failing it would hang the stage.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Quest")
	FEGQuestOperationResult FailActiveObjective(FGuid QuestInstanceGuid, FGuid ObjectiveGuid);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Quest")
	FEGQuestOperationResult AbandonQuest(FGuid QuestInstanceGuid);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Quest")
	FEGQuestOperationResult FailQuest(FGuid QuestInstanceGuid);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Quest")
	FEGQuestOperationResult NotifyGameplayEvent(const FEGQuestGameplayEvent& Event);

	/**
	 * Reevaluates timer and explicitly-pulsed predicate trackers through one normal transaction.
	 * TrackerPulseInterval drives this automatically; call it directly to pulse off-schedule.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Quest|Tracker")
	FEGQuestOperationResult PulseActiveTrackers();
	/** Rechecks stable handles and applies each role's authored loss policy. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Quest|Roles")
	FEGQuestOperationResult RefreshRoleBindings();

	/** Selects this run as the owner's single tracked quest. Invalid GUID clears tracking. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Quest|Presentation")
	FEGQuestOperationResult SetTrackedQuest(FGuid QuestInstanceGuid);
	UFUNCTION(BlueprintPure, Category = "Quest|Presentation")
	FGuid GetTrackedQuest() const;

	/**
	 * Overrides one objective's authored RequiredCount for this instance, letting one graph asset
	 * scale its target per session. Pass 0 to fall back to the authored count.
	 *
	 * The override is remembered by objective GUID, so it may be set at quest start for an objective
	 * that only activates later. Lowering the target below what an active objective has already
	 * counted resolves it immediately, rather than waiting for an event that may never come.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Quest")
	FEGQuestOperationResult SetObjectiveRequiredCount(FGuid QuestInstanceGuid, FGuid ObjectiveGuid, int32 RequiredCount);

	/**
	 * Applies SetObjectiveRequiredCount to every objective of the quest counting EventTag, wherever
	 * it sits in the graph. Returns the objectives it scaled, so the caller can track progress later.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Quest")
	FEGQuestOperationResult SetObjectiveRequiredCountByEventTag(FGuid QuestInstanceGuid, FGameplayTag EventTag, int32 RequiredCount);

	/** Reads one objective's checklist line. Only objectives of the active stage are known. */
	UFUNCTION(BlueprintPure, Category = "Quest")
	bool FindObjectiveSnapshot(FGuid QuestInstanceGuid, FGuid ObjectiveGuid, FEGQuestSnapshotObjective& OutObjective) const;

	/** The RequiredCount override recorded for an objective, or 0 when it uses its authored count. */
	UFUNCTION(BlueprintPure, Category = "Quest")
	int32 GetObjectiveRequiredCountOverride(FGuid QuestInstanceGuid, FGuid ObjectiveGuid) const;

	/** Valid only when this component is owned by the requesting client's PlayerState. */
	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "Quest")
	void ServerAbandonPrivateQuest(FGuid QuestInstanceGuid);

	/** Fires whenever any snapshot changes. Coarse; prefer the per-instance delegates below. */
	UPROPERTY(BlueprintAssignable, Category = "Quest") FEGQuestSnapshotsChanged OnQuestSnapshotsChanged;
	/** A quest instance appeared (started/resumed on authority, replicated-in on clients). */
	UPROPERTY(BlueprintAssignable, Category = "Quest") FEGQuestInstanceEvent OnQuestStarted;
	/** A quest instance's snapshot changed (progress, values, options, lifecycle). */
	UPROPERTY(BlueprintAssignable, Category = "Quest") FEGQuestInstanceEvent OnQuestUpdated;
	/** A quest instance's snapshot left the replicated array. */
	UPROPERTY(BlueprintAssignable, Category = "Quest") FEGQuestInstanceEvent OnQuestRemoved;
	/** Authority only: fires when the authority accepts a gameplay event. Never fires on remote clients. */
	UPROPERTY(BlueprintAssignable, Category = "Quest") FEGQuestGameplayEventAccepted OnGameplayEventAccepted;
	/**
	 * Authority only: a GameplayNotify quest event fired on one of this component's instances.
	 * Games relay this into their own event pipeline; remote clients must react to the relayed
	 * (replicated) consequence, never to this delegate.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Quest") FEGQuestGameplayNotify OnQuestGameplayNotify;
	/** Fires on the authority for every rejection, and on the owning client for rejected RPC requests. */
	UPROPERTY(BlueprintAssignable, Category = "Quest") FEGQuestRequestRejected OnQuestRequestRejected;
	UPROPERTY(BlueprintAssignable, Category = "Quest|Roles") FEGQuestRoleLost OnQuestRoleLost;
	/** Raw transient output mirror; the world subsystem additionally offers tag-query subscriptions. */
	UPROPERTY(BlueprintAssignable, Category = "Quest|Directive") FEGQuestDirectiveBroadcast OnQuestDirective;
	UPROPERTY(BlueprintAssignable, Category = "Quest|Telemetry") FEGQuestTelemetry OnQuestTelemetry;

	/** Non-shipping cheat backend used by Quest.JumpToStage. */
	FEGQuestOperationResult DebugJumpToStage(FGuid QuestInstanceGuid, FGuid StageGuid);

	// Internal: replication callbacks routed from FEGQuestSnapshotArray. Do not call directly.
	void NotifySnapshotReplicatedAdd(const FEGQuestViewSnapshot& Snapshot);
	void NotifySnapshotReplicatedChange(const FEGQuestViewSnapshot& Snapshot);
	void NotifySnapshotReplicatedRemove(const FEGQuestViewSnapshot& Snapshot);
	void NotifySnapshotsChanged();

protected:
	/** Owner-only rejection feedback for requests that arrived via a Server RPC. */
	UFUNCTION(Client, Reliable)
	void ClientQuestRequestRejected(FGuid QuestInstanceGuid, FName Reason);

	UPROPERTY(Replicated)
	FEGQuestSnapshotArray SharedQuestSnapshots;
	UPROPERTY(Replicated)
	FEGQuestSnapshotArray PrivateQuestSnapshots;

	/** Canonical authority state. A copy is mutated while an input transaction is open. */
	UPROPERTY(Transient)
	TMap<FGuid, FEGQuestRunRecord> RunRecords;

	// Authority-only execution contexts. Never replicated.
	UPROPERTY(Transient)
	TMap<FGuid, TObjectPtr<UEGQuestContext>> ActiveContexts;

	/** Additional independently-settling sentinel tracks. Main remains in ActiveContexts for compatibility. */
	UPROPERTY(Transient)
	TMap<FGuid, FEGQuestActiveTrackRuntimeSet> ActiveSentinelTracks;

	// Authority-only quest scripts, one per running instance whose graph declares a QuestScriptClass.
	// Never replicated, never saved: a resumed quest gets a fresh script and OnQuestResumed.
	UPROPERTY(Transient)
	TMap<FGuid, TObjectPtr<UEGQuestScript>> ActiveScripts;

	// Authority-only objective evaluators for each instance's active stage. Never replicated, never
	// saved: their counters live in the snapshot lines, so a resumed quest just gets fresh ones.
	UPROPERTY(Transient)
	TMap<FGuid, FEGQuestObjectiveEvaluatorSet> ActiveObjectiveEvaluators;

	// The injected clock, or null. Not a UPROPERTY: IEGQuestClock is not a UObject and an injected
	// clock is owned by whoever injected it. Null is a state, not a lazy-init hole - see SetQuestClock.
	TSharedPtr<IEGQuestClock> QuestClock;

private:
	// Objective evaluators report their outcome and progress through the private API below.
	friend class UEGQuestNode_Objective;

	/** Resolves an evaluator's checklist line; callbacks are queued until the transaction commits. */
	bool CompleteObjectiveFromEvaluator(UEGQuestNode_Objective& Evaluator, bool bSuccess);

	/**
	 * Moves one checklist counter by Delta, clamped at 0. A delta that clamps away is not a change
	 * and must not cost a replication. Returns whether anything changed.
	 */
	bool ApplyObjectiveProgress(FGuid QuestInstanceGuid, FGuid ObjectiveGuid, int32 Delta, bool bFailProgress);
	bool FindObjectiveAuthorityState(FGuid QuestInstanceGuid, FGuid ObjectiveGuid, FEGQuestSnapshotObjective& OutObjective) const;
	bool SetObjectiveSequenceIndex(FGuid QuestInstanceGuid, FGuid ObjectiveGuid, int32 NewIndex);
	int32 AddObjectiveDistinctKey(FGuid QuestInstanceGuid, FGuid ObjectiveGuid, FName Key);
	bool SetObjectiveTrackerEndTime(FGuid QuestInstanceGuid, FGuid ObjectiveGuid, double EndServerTime);

	/** Duplicates and activates an evaluator for every pending line of the active stage. */
	void CreateObjectiveEvaluators(FGuid QuestInstanceGuid,
		EEGQuestActivationReason Reason = EEGQuestActivationReason::Started);
	void CreateTrackEvaluators(FGuid QuestInstanceGuid, FName TrackName, EEGQuestActivationReason Reason);
	/** Deactivates and discards this instance's evaluators. Safe when there are none. */
	void DestroyObjectiveEvaluators(FGuid QuestInstanceGuid);
	void DestroyTrackEvaluators(FGuid QuestInstanceGuid, FName TrackName);

	/** Public API bodies. Public functions enqueue these so callback re-entry becomes a new input. */
	FEGQuestOperationResult StartSharedQuestNow(UEGQuestGraph* QuestGraph);
	FEGQuestOperationResult StartPrivateQuestNow(UEGQuestGraph* QuestGraph);
	FEGQuestOperationResult StartQuestFromTemplateNow(UEGQuestGraph* QuestGraph, FEGQuestTemplateParameters Parameters);
	FEGQuestOperationResult ResumeQuestNow(const FEGQuestSaveEnvelope& SaveData);
	FEGQuestOperationResult CompleteActiveObjectiveNow(FGuid QuestInstanceGuid, FGuid ObjectiveGuid);
	FEGQuestOperationResult FailActiveObjectiveNow(FGuid QuestInstanceGuid, FGuid ObjectiveGuid);
	FEGQuestOperationResult AbandonQuestNow(FGuid QuestInstanceGuid);
	FEGQuestOperationResult FailQuestNow(FGuid QuestInstanceGuid);
	FEGQuestOperationResult NotifyGameplayEventNow(const FEGQuestGameplayEvent& Event);
	FEGQuestOperationResult PulseActiveTrackersNow();
	FEGQuestOperationResult RefreshRoleBindingsNow();
	FEGQuestOperationResult SetTrackedQuestNow(FGuid QuestInstanceGuid);
	FEGQuestOperationResult DebugJumpToStageNow(FGuid QuestInstanceGuid, FGuid StageGuid);
	FEGQuestOperationResult SetObjectiveRequiredCountNow(FGuid QuestInstanceGuid, FGuid ObjectiveGuid, int32 RequiredCount);
	FEGQuestOperationResult SetObjectiveRequiredCountByEventTagNow(FGuid QuestInstanceGuid, FGameplayTag EventTag, int32 RequiredCount);
	FEGQuestOperationResult ExecuteOrQueue(TFunction<FEGQuestOperationResult()>&& Input, FGuid RequestedRunId = {});
	void QueuePostCommit(TFunction<void()>&& Callback);

	/**
	 * Starts or stops the pulse timer to match current run state. Called after every transaction
	 * drains, so a quest starting arms it and the last one ending disarms it.
	 */
	void UpdateTrackerPulseTimer();
	void HandleTrackerPulseTimer();
	/** True when a run exists that a pulse could still advance. */
	bool HasPulsableRun() const;

	FTimerHandle TrackerPulseTimerHandle;
	/** What UpdateTrackerPulseTimer last armed - see its comment for why this is not read back off the timer. */
	bool bTrackerPulseTimerArmed = false;
	void MarkRunDirty(FGuid QuestInstanceGuid);
	void ProjectRun(FGuid QuestInstanceGuid);
	void QueueLifecycleFactWrite(FGuid QuestInstanceGuid, const FString& Suffix);
	FEGQuestRunRecord* FindMutableRunRecord(FGuid QuestInstanceGuid, bool* bOutPrivate = nullptr);
	const FEGQuestRunRecord* FindRunRecord(FGuid QuestInstanceGuid) const;
	FEGQuestSnapshotObjective* FindMutableObjectiveState(FGuid QuestInstanceGuid, FGuid ObjectiveGuid);
	const FEGQuestSnapshotObjective* FindObjectiveState(FGuid QuestInstanceGuid, FGuid ObjectiveGuid) const;
	FName FindTrackNameForObjective(FGuid QuestInstanceGuid, FGuid ObjectiveGuid) const;
	bool ResolveRoleDefinitions(FGuid QuestInstanceGuid, const TArray<FEGQuestRoleDefinition>& Definitions,
		bool bStageScoped, FName TrackName = NAME_None, FGuid StageGuid = {});
	bool ResolveStageRoles(FGuid QuestInstanceGuid, FName TrackName);
	void ReleaseStageRoles(FGuid QuestInstanceGuid, FName TrackName);
	void RebuildRoleTexts(FGuid QuestInstanceGuid, FName TrackName);
	void RefreshRoleMarkers(const FEGQuestRunRecord& Record, TArray<FEGQuestRoleMarker>& OutMarkers) const;
	bool ApplyRoleLossPolicies(FGuid QuestInstanceGuid);
	const FEGQuestRoleDefinition* FindRoleDefinition(const FEGQuestRunRecord& Record,
		const FEGQuestRoleBinding& Binding) const;
	void RefreshObjectivePresentation(FGuid QuestInstanceGuid, FName TrackName, FEGQuestSnapshotObjective& Line,
		const UEGQuestNode_Objective& Objective);
	void QueueObjectiveEvents(FGuid QuestInstanceGuid, FGuid ObjectiveGuid,
		const TArray<TObjectPtr<UEGQuestEventCustom>>& Events);
	void EvaluateObjectiveMilestones(FGuid QuestInstanceGuid, FGuid ObjectiveGuid);
	void ApplyAutoTrackPolicy(FGuid QuestInstanceGuid, const UEGQuestGraph& QuestGraph);
	void QueueStageDirectives(FGuid QuestInstanceGuid, FName TrackName,
		const UEGQuestNode_Stage& Stage, bool bEntering);
	void EmitTelemetry(EEGQuestTelemetryEventType EventType, FGuid QuestInstanceGuid, FGuid ElementGuid = {});

	FGuid StartQuestInternal(UEGQuestGraph* QuestGraph, bool bPrivate, const FEGQuestTemplateParameters* Parameters = nullptr, FGuid PreferredInstanceGuid = {});
	void BindContextDelegates(FGuid QuestInstanceGuid, UEGQuestContext& Context);
	const FEGQuestViewSnapshot* FindSnapshot(FGuid QuestInstanceGuid) const;
	bool SetTerminalState(FGuid QuestInstanceGuid, EEGQuestLifecycleState State);
	/** Moves a projected terminal view out of the FastArray (MarkArrayDirty) into local history. */
	void PurgeReplicatedTerminalSnapshot(FGuid QuestInstanceGuid);
	void CapTerminalRunHistory();
	void PurgeTerminalRun(FGuid QuestInstanceGuid);
	int32 GetRunRevision(FGuid QuestInstanceGuid) const;
	FEGQuestOperationResult MakeRejectedResult(FGuid QuestInstanceGuid, FName Reason);
	static void ConvertTimerDeadlinesForSave(FEGQuestRunRecord& Record, double NowServerTime);
	static void ConvertTimerDeadlinesForResume(FEGQuestRunRecord& Record, double NowServerTime);

	/** The stage this instance is on, or null when the quest is not running on a stage. */
	const UEGQuestNode_Stage* GetActiveStage(FGuid QuestInstanceGuid) const;
	const UEGQuestNode_Stage* GetActiveStage(FGuid QuestInstanceGuid, FName TrackName) const;
	UEGQuestContext* GetTrackContext(FGuid QuestInstanceGuid, FName TrackName) const;
	FEGQuestTrackState* FindMutableTrackState(FGuid QuestInstanceGuid, FName TrackName);
	const FEGQuestTrackState* FindTrackState(FGuid QuestInstanceGuid, FName TrackName) const;
	FEGQuestActiveTrackRuntime* FindSentinelRuntime(FGuid QuestInstanceGuid, FName TrackName);
	const FEGQuestActiveTrackRuntime* FindSentinelRuntime(FGuid QuestInstanceGuid, FName TrackName) const;
	void SyncMainTrack(FGuid QuestInstanceGuid);
	/** The objective node behind a checklist line of the active stage. */
	const UEGQuestNode_Objective* FindActiveObjectiveNode(FGuid QuestInstanceGuid, FGuid ObjectiveGuid) const;

	/** Makes StageIndex active, then rebuilds the journal from it. Cancels whatever the old stage had pending. */
	bool EnterStage(FGuid QuestInstanceGuid, int32 StageIndex);
	bool EnterStage(FGuid QuestInstanceGuid, FName TrackName, int32 StageIndex);
	/** Rebuilds ActiveObjectives from the active stage and resolves anything that starts resolved. */
	void RebuildActiveObjectives(FGuid QuestInstanceGuid,
		EEGQuestActivationReason Reason = EEGQuestActivationReason::Started);
	void RebuildTrackObjectives(FGuid QuestInstanceGuid, FName TrackName, EEGQuestActivationReason Reason);
	/** Fires destinations until nothing more is satisfied. Called after any objective resolves. */
	void SettleQuest(FGuid QuestInstanceGuid);
	void SettleTrack(FGuid QuestInstanceGuid, FName TrackName);
	/** The first destination whose every incoming arrow is satisfied, or INDEX_NONE. */
	int32 FindFiringDestination(FGuid QuestInstanceGuid) const;
	int32 FindFiringDestination(FGuid QuestInstanceGuid, FName TrackName) const;
	/** Is every arrow pointing into TargetIndex satisfied by the active stage's objectives? */
	bool IsDestinationSatisfied(FGuid QuestInstanceGuid, int32 TargetIndex) const;
	bool IsDestinationSatisfied(FGuid QuestInstanceGuid, FName TrackName, int32 TargetIndex) const;
	/**
	 * Records an outcome on a checklist line. Returns false when it was already resolved. The script
	 * hears about it inline, or on scope exit while a resolution scope is open.
	 */
	bool ResolveObjective(FGuid QuestInstanceGuid, FGuid ObjectiveGuid, EEGQuestObjectiveOutcome Outcome);
	/** The count an objective needs this run: its authored one, or this instance's override. */
	int32 ResolveRequiredCount(const FEGQuestRunRecord& Record, const UEGQuestNode_Objective& Objective) const;

	/** Creates the quest script instance for this quest, when its graph declares one. */
	void CreateScriptInstance(FGuid QuestInstanceGuid, const UEGQuestGraph& QuestGraph);
	UEGQuestScript* FindScript(FGuid QuestInstanceGuid) const;
	/** Tells the script about the active stage, when there is a script and an active stage. */
	void NotifyScriptStageEntered(FGuid QuestInstanceGuid);

	void NotifyScriptStageExited(FGuid QuestInstanceGuid, const UEGQuestNode_Stage& Stage, EEGQuestStageExitReason Reason);

	void NotifyScriptObjectiveProgress(FGuid QuestInstanceGuid, FEGQuestSnapshotObjective ProgressedLine);

	void NotifyScriptRoleLost(FGuid QuestInstanceGuid, FName RoleName);
	/** Tells the script one checklist line reached an outcome. Takes a copy: callers may be mid-iteration. */
	void NotifyScriptObjectiveResolved(FGuid QuestInstanceGuid, FEGQuestSnapshotObjective ResolvedLine);
	/** Tells the script the quest ended, then discards the script. */
	void NotifyScriptQuestEnded(FGuid QuestInstanceGuid, EEGQuestLifecycleState State);

	void RefreshRunRecord(FGuid QuestInstanceGuid, bool bIncrementRevision = true);
	void Reject(FGuid QuestInstanceGuid, FName Reason);
	void BroadcastUpdated(FGuid QuestInstanceGuid);
	bool HasQuestAuthority() const;

	/** QuestClock when one was injected; otherwise today's logic, re-read from the world every call. */
	double GetServerTime() const;

	// Reason of the most recent Reject() call; used to relay RPC failures back to the owner.
	FName LastRejectReason;

	/** Inputs requested by post-commit callbacks; drained iteratively on the game thread. */
	TArray<TFunction<FEGQuestOperationResult()>> QueuedInputs;
	/** Notifications and extension callbacks released only after authority commit + projection rebuild. */
	TArray<TFunction<void()>> PostCommitCallbacks;
	/** Transaction-local authority copy. No public input mutates RunRecords in place. */
	TMap<FGuid, FEGQuestRunRecord> TransactionRunRecords;
	TSet<FGuid> DirtyRunIds;
	bool bProcessingInputQueue = false;
	bool bTransactionActive = false;
	bool bTransactionStateChanged = false;
	bool bCollectingTrackerProposals = false;

	/** Non-replicated terminal views kept for authority/query after FastArray purge. */
	UPROPERTY(Transient)
	TMap<FGuid, FEGQuestViewSnapshot> TerminalSnapshotCache;

	/** Max terminal RunRecords retained before oldest CompletionServerTime entries are dropped. */
	static constexpr int32 MaxTerminalRunHistory = 64;
};
