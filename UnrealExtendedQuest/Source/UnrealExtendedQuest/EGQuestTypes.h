// Copyright Devil of the Plague. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "Subsystems/WorldSubsystem.h"
#include "EGQuestRole.h"
#include "EGQuestTypes.generated.h"

class AActor;
class UEGQuestComponent;
class UEGQuestGraph;

UENUM(BlueprintType)
enum class EEGQuestDirectivePhase : uint8 { Activate, Deactivate };

USTRUCT(BlueprintType)
struct UNREALEXTENDEDQUEST_API FEGQuestDirective
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest|Directive") FGameplayTag DirectiveTag;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest|Directive") TArray<FName> RoleNames;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest|Directive") float Magnitude = 1.0f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FEGQuestDirectiveBroadcast, FGuid, QuestInstanceGuid,
	const FEGQuestDirective&, Directive, EEGQuestDirectivePhase, Phase);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FEGQuestDirectiveNative, FGuid, const FEGQuestDirective&, EEGQuestDirectivePhase);

/** Transient server-side router. Subscribers replicate durable consequences through game-owned systems. */
UCLASS()
class UNREALEXTENDEDQUEST_API UEGQuestDirectiveSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	FDelegateHandle Subscribe(const FGameplayTagQuery& Query, FEGQuestDirectiveNative::FDelegate&& Callback);
	void Unsubscribe(FDelegateHandle Handle);
	void Dispatch(FGuid QuestInstanceGuid, const FEGQuestDirective& Directive, EEGQuestDirectivePhase Phase);
	UPROPERTY(BlueprintAssignable, Category = "Quest|Directive") FEGQuestDirectiveBroadcast OnDirective;
private:
	struct FSubscription
	{
		FDelegateHandle Handle;
		FGameplayTagQuery Query;
		FEGQuestDirectiveNative::FDelegate Callback;
	};
	TArray<FSubscription> Subscriptions;
};

UENUM(BlueprintType)
enum class EEGQuestResult : uint8
{
	Completed,
	Failed,
	Abandoned
};

UENUM(BlueprintType)
enum class EEGQuestLifecycleState : uint8
{
	Inactive,
	Active,
	Completed,
	Failed,
	Abandoned
};

/** Stable outcome contract for every authority-side mutation request. */
UENUM(BlueprintType)
enum class EEGQuestOperationStatus : uint8
{
	Applied,
	AcceptedNoChange,
	Deferred,
	Rejected
};

/**
 * The public mutation result. Callers never have to infer failure from an invalid GUID or a bare
 * bool, and tests can assert the exact revision contract of a request.
 */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDQUEST_API FEGQuestOperationResult
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	EEGQuestOperationStatus Status = EEGQuestOperationStatus::Rejected;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	FName Diagnostic = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	FGuid RunId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	int32 BeforeRevision = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	int32 AfterRevision = 0;

	/** Populated by fan-out operations such as scaling every matching objective. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	TArray<FGuid> AffectedElementGuids;

	bool IsSuccess() const
	{
		return Status == EEGQuestOperationStatus::Applied ||
			Status == EEGQuestOperationStatus::AcceptedNoChange ||
			Status == EEGQuestOperationStatus::Deferred;
	}

	bool WasApplied() const { return Status == EEGQuestOperationStatus::Applied; }
	bool IsValid() const { return IsSuccess() && RunId.IsValid(); }

	// Source compatibility while game-owned wrappers migrate to inspecting Status explicitly.
	//
	// These are a footgun and are kept only because callers still rely on them: operator bool is
	// IsSuccess(), which is true for Deferred and AcceptedNoChange as well as Applied, so `if (Result)`
	// does not mean "the thing happened". It also sits next to operator FGuid, so an unintended
	// conversion has two candidates to pick from. Prefer inspecting Status, or WasApplied().
	operator bool() const { return IsSuccess(); }
	operator FGuid() const { return RunId; }
	operator TArray<FGuid>() const { return AffectedElementGuids; }

	static FEGQuestOperationResult Applied(FGuid InRunId, int32 InBeforeRevision, int32 InAfterRevision)
	{
		FEGQuestOperationResult Result;
		Result.Status = EEGQuestOperationStatus::Applied;
		Result.RunId = InRunId;
		Result.BeforeRevision = InBeforeRevision;
		Result.AfterRevision = InAfterRevision;
		return Result;
	}

	static FEGQuestOperationResult NoChange(FGuid InRunId, int32 InRevision, FName InDiagnostic = NAME_None)
	{
		FEGQuestOperationResult Result;
		Result.Status = EEGQuestOperationStatus::AcceptedNoChange;
		Result.Diagnostic = InDiagnostic;
		Result.RunId = InRunId;
		Result.BeforeRevision = InRevision;
		Result.AfterRevision = InRevision;
		return Result;
	}

	static FEGQuestOperationResult Rejected(FGuid InRunId, int32 InRevision, FName InDiagnostic)
	{
		FEGQuestOperationResult Result;
		Result.Status = EEGQuestOperationStatus::Rejected;
		Result.Diagnostic = InDiagnostic;
		Result.RunId = InRunId;
		Result.BeforeRevision = InRevision;
		Result.AfterRevision = InRevision;
		return Result;
	}
};

/**
 * What an arrow leaving an objective routes on. An arrow is satisfied when its source objective
 * reached the matching outcome; a node fires when every arrow pointing into it is satisfied.
 */
UENUM(BlueprintType)
enum class EEGQuestArrowOutcome : uint8
{
	Success,
	Fail
};

/** Where an objective got to within its stage. Reset every time the stage is entered. */
UENUM(BlueprintType)
enum class EEGQuestObjectiveOutcome : uint8
{
	Pending,
	Succeeded,
	Failed,
	Obsolete,
	Expired
};

UENUM(BlueprintType)
enum class EEGQuestSemanticProgressType : uint8
{
	Boolean,
	Count,
	Ratio,
	Time,
	UniqueSet
};

/** Stable presentation union. Consumers switch on Type and never infer semantics from tracker classes. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDQUEST_API FEGQuestSemanticProgress
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest|Presentation") EEGQuestSemanticProgressType Type = EEGQuestSemanticProgressType::Boolean;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest|Presentation") bool bValue = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest|Presentation") int32 Count = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest|Presentation") int32 RequiredCount = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest|Presentation") float Ratio = 0.0f;
	/** Absolute authority clock time, so clients can calculate remaining time without per-tick replication. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest|Presentation") double EndServerTime = 0.0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest|Presentation") TArray<FName> UniqueSet;
};

UENUM(BlueprintType)
enum class EEGQuestAutoTrackPolicy : uint8
{
	Never,
	IfNone,
	Always
};

UENUM(BlueprintType)
enum class EEGQuestTelemetryEventType : uint8
{
	Started,
	Resumed,
	StageEntered,
	ObjectiveProgress,
	ObjectiveResolved,
	Directive,
	RoleLost,
	Completed,
	Failed,
	Abandoned
};

UENUM(BlueprintType)
enum class EEGQuestTrackType : uint8
{
	Main,
	Sentinel
};

/** Why a stateful tracker is being activated. Restore and migration must never replay fresh-start effects. */
UENUM(BlueprintType)
enum class EEGQuestActivationReason : uint8
{
	Started,
	Restored,
	Migrated
};

/** Why a tracker is leaving service. This is an explicit cleanup contract, not a destructor guess. */
UENUM(BlueprintType)
enum class EEGQuestDeactivationReason : uint8
{
	Completed,
	Failed,
	Cancelled,
	Obsolete,
	Expired
};

/**
 * What to do with a saved run whose ContentVersion no longer matches the graph it was recorded
 * against. Authored per graph on UEGQuestGraph, and applied by UEGQuestComponent when resuming.
 */
UENUM(BlueprintType)
enum class EEGQuestResumePolicy : uint8
{
	// Throw the run away and start the quest again from its entry points. The safe default: a graph
	// edit can move, delete or repurpose the very node the run is sitting on.
	Restart,

	// Resume anyway, keeping whatever still resolves against the new graph. For quests whose author
	// accepts that a mid-run edit may land the player somewhere the edit did not anticipate.
	BestEffortResume
};

/**
 * One authored arrow priority: "on this outcome, this node prefers this destination this much".
 *
 * This is what replaced the destination's canvas position. Arbitration used to be decided by
 * NodePosX/NodePosY, so tidying a layout silently rerouted a quest; now layout is cosmetic and this
 * is the data. Keyed by destination GUID rather than by edge index because edge indices are compiler
 * output, rebuilt from the pins on every compile.
 *
 * Lives on the source node (UEGQuestNode::RoutePriorities), never on FEGQuestEdge: the edge array is
 * regenerated wholesale by every compile, and authored data may not live in compiler output.
 */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDQUEST_API FEGQuestRoutePriority
{
	GENERATED_BODY()

	/** The GUID of the node this arrow points at. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "QuestRoute")
	FGuid DestinationGuid;

	/**
	 * Which outcome group this route arbitrates in. Success and Fail are independent groups - an
	 * objective's success arrows never compete with its fail arrows, because only one outcome is ever
	 * reached - and each group is exactly the links of one pin.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "QuestRoute")
	EEGQuestArrowOutcome Outcome = EEGQuestArrowOutcome::Success;

	/** Ascending order within (source node, outcome): lower is tried first. Ties are an authoring error. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuestRoute", Meta = (ClampMin = "0"))
	int32 Priority = 0;
};

/** One checklist line of the active stage: the journal entry for a single objective. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDQUEST_API FEGQuestSnapshotObjective
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	FGuid Guid;

	/** Objective text with {arguments} resolved on the authority. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	FText Text;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	EEGQuestObjectiveOutcome Outcome = EEGQuestObjectiveOutcome::Pending;

	/** Progress towards RequiredCount. Only counting success conditions ever move it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	int32 Count = 0;

	/** The count this objective actually needs this run: the authored one, or an applied override. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	int32 RequiredCount = 0;

	/** Progress towards the fail condition, when it is a counting one. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	int32 FailCount = 0;

	/** Stateful sequence tracker's next expected authored entry. Authority/save state. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	int32 SequenceIndex = 0;

	/** Distinct tracker keys and compact composite child state. Authority/save state. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	TArray<FName> DistinctKeys;

	/** Absolute injected-clock deadline for timer trackers. Zero means not initialized. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	double TrackerEndServerTime = 0.0;

	/** Versioned extension fragment for scripted trackers that cannot use the built-in fields. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	int32 TrackerFragmentVersion = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	TArray<uint8> TrackerFragment;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest|Presentation") FEGQuestSemanticProgress Progress;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest|Presentation") bool bOptional = false;
	/** Security/audience flag. Unauthorized projections omit the row entirely. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest|Presentation") bool bHidden = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest|Presentation") FName UITag = NAME_None;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest|Presentation") int32 SortOrder = 0;
	/** Authored milestone indices already emitted; persisted to make resume idempotent. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest|Presentation") TArray<int32> EmittedMilestones;

	bool IsResolved() const { return Outcome != EEGQuestObjectiveOutcome::Pending; }
};

/**
 * A per-instance replacement for one objective's authored RequiredCount, so one graph asset can
 * scale its target per session (party size, difficulty).
 *
 * Keyed by objective GUID rather than applied to the active objective, because the target is
 * usually chosen when the quest starts and the objective it scales activates much later.
 */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDQUEST_API FEGQuestObjectiveCountOverride
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	FGuid ObjectiveGuid;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	int32 RequiredCount = 0;
};

/** Typed inputs applied before a template quest's first stage is published. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDQUEST_API FEGQuestTemplateParameters
{
	GENERATED_BODY()

	/** Explicit bindings win over authored resolvers and survive save/resume as handles. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest|Template")
	TArray<FEGQuestRoleBinding> RoleBindings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest|Template")
	TArray<FEGQuestObjectiveCountOverride> ObjectiveCountOverrides;
};

/** One independently settling flow within a logical run. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDQUEST_API FEGQuestTrackState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest|Track") FName TrackName = TEXT("Main");
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest|Track") EEGQuestTrackType TrackType = EEGQuestTrackType::Main;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest|Track") FGuid ActiveNodeGuid;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest|Track") FText StageTitle;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest|Track") FText StageDescription;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest|Track") TArray<FEGQuestSnapshotObjective> ActiveObjectives;
	/** Resolved rows retained for presentation; pending rows become Obsolete when their stage exits. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest|Track") TArray<FEGQuestSnapshotObjective> ObjectiveHistory;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, NotReplicated, Category = "Quest|Track") TArray<FGuid> VisitedNodeGuids;
};

/** Why a quest script's OnStageExited fired: the stage advanced, or the whole quest ended. */
UENUM(BlueprintType)
enum class EEGQuestStageExitReason : uint8
{
	Advanced,
	QuestCompleted,
	QuestFailed,
	QuestAbandoned
};

USTRUCT(BlueprintType)
struct UNREALEXTENDEDQUEST_API FEGQuestGameplayEvent
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FGameplayTag EventTag;

	/**
	 * Progress delta applied to gameplay-event objectives, rounded to the nearest integer.
	 * A magnitude that rounds to 0 counts as +1 (the default single completion tick).
	 * Negative magnitudes remove progress; the accumulated count never drops below 0.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	float Magnitude = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FGameplayTagContainer ContextTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FName ContextName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	TObjectPtr<AActor> Instigator = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	TObjectPtr<AActor> Target = nullptr;

	/** Stable producer/stream identity. Sequence is monotonic only within this publisher. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FName PublisherId = NAME_None;

	/**
	 * Optional monotonic per-publisher sequence. When non-zero, UEGQuestComponent drops events whose
	 * sequence is not greater than this run's persisted cursor for PublisherId.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	int64 Sequence = 0;

	/** Informational publish timestamp (server world seconds). Forwarded to OnGameplayEventAccepted listeners. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	double ServerTime = 0.0;
};

/** Persisted idempotency cursor for one event producer. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDQUEST_API FEGQuestPublisherCursor
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FName PublisherId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	int64 Sequence = 0;
};

/**
 * Complete authority-side state for one logical run. This is never replicated. Mutations operate
 * on a transaction-local copy and the component builds an audience-safe view after commit.
 */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDQUEST_API FEGQuestRunRecord
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	FPrimaryAssetId QuestAssetId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	TSoftObjectPtr<UEGQuestGraph> QuestGraph;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	FGuid GraphGuid;

	/** Namespaced catalog identity stamped when the run starts. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	FName DefinitionId = NAME_None;

	/** Authored graph version used for resume compatibility. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	int32 ContentVersion = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	FGuid QuestInstanceGuid;

	/** Audience class used when building the replicated projection. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	bool bPrivate = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	EEGQuestLifecycleState LifecycleState = EEGQuestLifecycleState::Inactive;

	/** The active stage. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	FGuid ActiveNodeGuid;

	/** The journal entry for the active stage, with {arguments} resolved on the authority. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	FText ActiveStageTitle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	FText ActiveStageDescription;

	/** The active stage's checklist, in authored order. Rebuilt whenever a stage is entered. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	TArray<FEGQuestSnapshotObjective> ActiveObjectives;

	/** Main first, then sentinels in authored entry order. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	TArray<FEGQuestTrackState> Tracks;

	/** Stable role handles only. Live actor pointers are resolved on demand through the registry. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest|Roles")
	TArray<FEGQuestRoleBinding> RoleBindings;

	/**
	 * Per-instance RequiredCount replacements, keyed by objective GUID. Survives stage changes so a
	 * target chosen at quest start still applies to an objective that activates later.
	 * Set through UEGQuestComponent::SetObjectiveRequiredCount.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	TArray<FEGQuestObjectiveCountOverride> ObjectiveRequiredCountOverrides;

	/**
	 * Visited history. Authority/save-game only: excluded from replication because clients have no
	 * use for it and it grows unbounded on looping quests.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, NotReplicated, Category = "Quest")
	TArray<FGuid> VisitedNodeGuids;

	/** Authority/save only. Replication views never expose producer progress. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, NotReplicated, Category = "Quest")
	TArray<FEGQuestPublisherCursor> PublisherCursors;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	double StartServerTime = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	double CompletionServerTime = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	int32 Revision = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest|Presentation")
	bool bTracked = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest|Roles")
	bool bSuspendedByRoleLoss = false;

	bool IsTerminal() const
	{
		return LifecycleState == EEGQuestLifecycleState::Completed ||
			LifecycleState == EEGQuestLifecycleState::Failed ||
			LifecycleState == EEGQuestLifecycleState::Abandoned;
	}
};

/** Audience-built journal projection. Authority-only history, cursors and overrides never enter it. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDQUEST_API FEGQuestViewSnapshot : public FFastArraySerializerItem
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	FPrimaryAssetId QuestAssetId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	TSoftObjectPtr<UEGQuestGraph> QuestGraph;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	FGuid GraphGuid;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	FName DefinitionId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	FGuid QuestInstanceGuid;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	EEGQuestLifecycleState LifecycleState = EEGQuestLifecycleState::Inactive;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	FGuid ActiveNodeGuid;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	FText ActiveStageTitle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	FText ActiveStageDescription;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	TArray<FEGQuestSnapshotObjective> ActiveObjectives;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	TArray<FEGQuestTrackState> Tracks;

	/** Audience-safe role marker snapshots. Unloaded handles remain present with bResolved=false. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest|Roles")
	TArray<FEGQuestRoleMarker> RoleMarkers;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	double StartServerTime = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	double CompletionServerTime = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	int32 Revision = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest|Presentation")
	bool bTracked = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest|Roles")
	bool bSuspendedByRoleLoss = false;

	bool IsTerminal() const
	{
		return LifecycleState == EEGQuestLifecycleState::Completed ||
			LifecycleState == EEGQuestLifecycleState::Failed ||
			LifecycleState == EEGQuestLifecycleState::Abandoned;
	}
};

/** Source-only transition alias. New reflected APIs expose FEGQuestViewSnapshot explicitly. */
using FEGQuestRuntimeSnapshot = FEGQuestViewSnapshot;

/** Fast-array wrapper for snapshot replication: per-item deltas and per-item client callbacks. */
USTRUCT()
struct UNREALEXTENDEDQUEST_API FEGQuestSnapshotArray : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FEGQuestViewSnapshot> Items;

	/** Client-side notification sink. Set by the owning component; never replicated (outer keeps it alive). */
	UEGQuestComponent* OwnerComponent = nullptr;

	void PostReplicatedAdd(const TArrayView<int32>& AddedIndices, int32 FinalSize);
	void PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 FinalSize);
	void PreReplicatedRemove(const TArrayView<int32>& RemovedIndices, int32 FinalSize);

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FastArrayDeltaSerialize<FEGQuestViewSnapshot, FEGQuestSnapshotArray>(Items, DeltaParms, *this);
	}
};

template<>
struct TStructOpsTypeTraits<FEGQuestSnapshotArray> : public TStructOpsTypeTraitsBase2<FEGQuestSnapshotArray>
{
	enum { WithNetDeltaSerializer = true };
};

/** Explicit persistence payload. Transient events and actor references are intentionally excluded. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDQUEST_API FEGQuestSaveEnvelope
{
	GENERATED_BODY()

	/** Independent save schema; intentionally not tied to graph or plugin object versions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	int32 SchemaVersion = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FEGQuestRunRecord RunRecord;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	bool bPrivate = false;

	/** Ordered migration identifiers already applied to this envelope. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	TArray<FName> AppliedMigrations;
};
