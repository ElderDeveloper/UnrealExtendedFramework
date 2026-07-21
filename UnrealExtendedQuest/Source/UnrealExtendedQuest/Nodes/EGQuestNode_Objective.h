// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "UnrealExtendedQuest/Nodes/EGQuestNode.h"
#include "UnrealExtendedQuest/EGQuestTextArgument.h"
#include "UnrealExtendedQuest/EGQuestTypes.h"
#include "UnrealExtendedQuest/EGQuestSearchTerm.h"
#include "UnrealExtendedQuest/EGQuestTracker.h"
#include "GameplayTagContainer.h"

#include "EGQuestNode_Objective.generated.h"

struct FEGQuestTextArgument;
class UEGQuestComponent;
class UEGQuestEventCustom;

USTRUCT(BlueprintType)
struct UNREALEXTENDEDQUEST_API FEGQuestObjectiveMilestone
{
	GENERATED_BODY()
	/** Normalized progress threshold in [0,1]. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest|Objective", Meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Threshold = 0.5f;
	/** Post-commit notification events. */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category = "Quest|Objective")
	TArray<TObjectPtr<UEGQuestEventCustom>> Events;
};

/**
 * A leaf owned by a stage: one thing the player must do.
 *
 * An objective is never entered as a node - its stage owns it, and its outgoing arrows route on the
 * outcome it reaches. The objective itself decides that outcome: when a stage becomes active,
 * UEGQuestComponent duplicates each pending objective into a transient, authority-only evaluator
 * instance and activates it. The evaluator reports the result with CompleteObjective(bSuccess).
 *
 * The built-in evaluation covers the common cases with no subclass needed:
 *  - EventTag set: succeeds once RequiredCount matching gameplay events are accepted.
 *  - FailEventTag set: fails once FailRequiredCount matching events are accepted.
 *  - bCompleteOnStageEnter: succeeds the moment its stage is entered (utility traversal).
 *  - Nothing set: waits for authority code (UEGQuestComponent::CompleteActiveObjective, a quest
 *    script, or a Blueprint subclass) to complete it.
 *
 * Blueprint subclasses override OnActivated / OnQuestGameplayEvent / OnDeactivated for custom logic
 * and call CompleteObjective(true/false) when they know the outcome. A subclass that can fail must
 * set bCanFail in its defaults so the editor gives its rows a fail pin.
 *
 * Progress counters live in the replicated snapshot line (FEGQuestSnapshotObjective), never on the
 * evaluator, so evaluators stay disposable and save/resume just reseeds them from the snapshot.
 */
UCLASS(Blueprintable, BlueprintType, ClassGroup = "Quest")
class UNREALEXTENDEDQUEST_API UEGQuestNode_Objective : public UEGQuestNode
{
	GENERATED_BODY()

public:

	//
	// Authored data
	//

	/**
	 * The count this objective's primary tracker requires, projected for UI and tools. Reads the
	 * authored tracker (EventCount / Distinct / Sequence); 1 for everything else. At runtime a
	 * per-instance override can win - clients should read the replicated snapshot's Progress instead.
	 */
	UFUNCTION(BlueprintPure, Category = "Quest|Objective")
	int32 GetPresentedRequiredCount() const;

	/** The event tag the primary tracker counts (EventCount / Distinct), if it counts one. For UI and tools. */
	UFUNCTION(BlueprintPure, Category = "Quest|Objective")
	FGameplayTag GetPresentedEventTag() const;

	/**
	 * Can this objective ever reach the Failed outcome? Drives the fail pin in the editor and gates
	 * every fail path at runtime: failing an objective with no fail routing would hang its stage.
	 * True exactly when a failure tracker exists (or the success slot's tracker resolves to Failed) -
	 * an objective with no failure tracker cannot fail.
	 */
	UFUNCTION(BlueprintPure, Category = "Quest|Objective")
	bool CanEverFail() const;

	/** Stateful evaluation template. The active evaluator owns a transient duplicate. */
	UFUNCTION(BlueprintPure, Category = "Quest|Objective")
	UEGQuestTracker* GetTracker() const { return Tracker; }
	void SetTracker(UEGQuestTracker* InTracker) { Tracker = InTracker; }

	/** Optional independent failure tracker, preserving success/fail race semantics. */
	UFUNCTION(BlueprintPure, Category = "Quest|Objective")
	UEGQuestTracker* GetFailureTracker() const { return FailureTracker; }
	void SetFailureTracker(UEGQuestTracker* InTracker) { FailureTracker = InTracker; }

	bool IsOptional() const { return bOptional; }
	void SetOptional(bool bInOptional) { bOptional = bInOptional; }
	bool IsHidden() const { return bHidden; }
	void SetHidden(bool bInHidden) { bHidden = bInHidden; }
	FName GetUITag() const { return UITag; }
	void SetUITag(FName InTag) { UITag = InTag; }
	int32 GetSortOrder() const { return SortOrder; }
	void SetSortOrder(int32 InOrder) { SortOrder = InOrder; }
	const TArray<TObjectPtr<UEGQuestEventCustom>>& GetSuccessEvents() const { return OnSuccessEvents; }
	const TArray<TObjectPtr<UEGQuestEventCustom>>& GetFailEvents() const { return OnFailEvents; }
	const TArray<FEGQuestObjectiveMilestone>& GetMilestones() const { return Milestones; }
	void SetSuccessEvents(const TArray<TObjectPtr<UEGQuestEventCustom>>& InEvents) { OnSuccessEvents = InEvents; }
	void SetFailEvents(const TArray<TObjectPtr<UEGQuestEventCustom>>& InEvents) { OnFailEvents = InEvents; }
	void SetMilestones(const TArray<FEGQuestObjectiveMilestone>& InMilestones) { Milestones = InMilestones; }

	// Begin UObject Interface.
	FString GetDesc() override
	{
		return TEXT("Localized quest objective that evaluates its own success and failure.");
	}

	void PostLoad() override;

#if WITH_EDITOR
	/**
	 * Called when a property on this object has been modified externally
	 *
	 * @param PropertyChangedEvent the property that was modified
	 */
	void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	//
	// Begin UEGQuestNode Interface.
	//

	void UpdateTextsValuesFromDefaultsAndRemappings(const UEGQuestPluginSettings& Settings, bool bUpdateGraphNode = true) override;
	void UpdateTextsNamespacesAndKeys(const UEGQuestPluginSettings& Settings, bool bUpdateGraphNode = true) override;
	void RebuildConstructedText(UEGQuestContext& Context) const override;
	void RebuildTextArguments(bool bUpdateGraphNode = true) override
	{
		Super::RebuildTextArguments(bUpdateGraphNode);
		FEGQuestTextArgument::UpdateTextArgumentArray(Text, TextArguments);
	}
	void RebuildTextArgumentsFromPreview(const FText& Preview) override { FEGQuestTextArgument::UpdateTextArgumentArray(Preview, TextArguments); }
	const TArray<FEGQuestTextArgument>& GetTextArguments() const override { return TextArguments; };

	// Getters:
	// Returns the raw authored text. Formatted (per-instance) text lives in UEGQuestContext and
	// reaches clients through FEGQuestSnapshotObjective - the shared asset holds no runtime state.
	const FText& GetNodeText() const override { return Text; }

#if WITH_EDITOR
	FString GetNodeTypeString() const override { return TEXT("Objective"); }
#endif

	//
	// Editor support
	//

	/** Compact summary of what this row needs, shown on the stage card and in Find-in-Quests. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Quest|Objective")
	FString GetEditorDisplayString(UEGQuestGraph* OwnerQuest);
	virtual FString GetEditorDisplayString_Implementation(UEGQuestGraph* OwnerQuest);

	/** Editor compile check. Return false and fill OutError to raise a warning on the owning stage card. */
	virtual bool ValidateForCompile(FString& OutError) const;

	/** Reports this objective's searchable fields to Find-in-Quests. The class name is added for you. */
	virtual void GetSearchTerms(TArray<FEGQuestSearchTerm>& OutTerms) const;

	//
	// Evaluator runtime. Everything below is only meaningful on the transient evaluator copies
	// UEGQuestComponent creates per active stage per quest instance - never on the authored asset
	// node, which is shared by every quest instance in the process.
	//

	/** Internal, component only: binds this evaluator to its quest instance and fires OnActivated. */
	void ActivateEvaluator(UEGQuestComponent& InOwner, const FGuid& InInstanceGuid,
		EEGQuestActivationReason Reason = EEGQuestActivationReason::Started);
	/** Internal, component only: fires OnDeactivated and unbinds. Safe to call on inactive objects. */
	void DeactivateEvaluator();
	/** Internal, component only: forwards an accepted gameplay event to the evaluation. */
	void HandleGameplayEvent(const FEGQuestGameplayEvent& Event, int32 ProgressDelta);
	/** Internal, component/simulator only: reevaluates clock and declared dependency trackers. */
	void PulseEvaluator();

	UFUNCTION(BlueprintPure, Category = "Quest|Objective")
	bool IsEvaluatorActive() const { return EvaluatorOwner.IsValid() && EvaluatorInstanceGuid.IsValid(); }

	/**
	 * Reports this objective's outcome: true succeeds it, false fails it. The one call every
	 * evaluation ends with. Failing requires fail routing (CanEverFail), or the stage would hang.
	 */
	UFUNCTION(BlueprintCallable, Category = "Quest|Objective")
	void CompleteObjective(bool bSuccess);

	/** Adds (or removes, negative) success progress on the checklist line. Returns false when nothing changed. */
	UFUNCTION(BlueprintCallable, Category = "Quest|Objective")
	bool AddProgress(int32 Delta = 1);

	/** Adds (or removes, negative) fail progress on the checklist line. Returns false when nothing changed. */
	UFUNCTION(BlueprintCallable, Category = "Quest|Objective")
	bool AddFailProgress(int32 Delta = 1);

	UFUNCTION(BlueprintPure, Category = "Quest|Objective")
	int32 GetProgress() const;

	UFUNCTION(BlueprintPure, Category = "Quest|Objective")
	int32 GetFailProgress() const;

	/** The count this objective needs this run: the authored one, or the instance's applied override. */
	UFUNCTION(BlueprintPure, Category = "Quest|Objective")
	int32 GetRequiredProgress() const;

	/** True once this evaluator's checklist line reached an outcome (also true when not active). */
	UFUNCTION(BlueprintPure, Category = "Quest|Objective")
	bool IsResolved() const;

	int32 GetTrackerSequenceIndex() const;
	bool SetTrackerSequenceIndex(int32 NewIndex);
	int32 AddTrackerDistinctKey(FName Key);
	double GetTrackerEndServerTime() const;
	bool SetTrackerEndServerTime(double EndServerTime);

	/** The component running the quest instance this evaluator belongs to. Null on the authored node. */
	UFUNCTION(BlueprintPure, Category = "Quest|Objective")
	UEGQuestComponent* GetQuestComponent() const;

	UFUNCTION(BlueprintPure, Category = "Quest|Objective")
	FGuid GetQuestInstanceGuid() const { return EvaluatorInstanceGuid; }

	//
	// Begin own functions.
	//

	// Sets the RawNodeText of the Node and rebuilds the constructed text
	virtual void SetNodeText(const FText& InText)
	{
		Text = InText;
		RebuildTextArguments(/* bUpdateGraphNode */ false);
	}

	TArray<FEGQuestTextArgument>& GetMutableTextArguments() { return TextArguments; }

	// Helper functions to get the names of some properties. Used by the QuestPluginEditor module.
	static FName GetMemberNameText() { return GET_MEMBER_NAME_CHECKED(UEGQuestNode_Objective, Text); }
	static FName GetMemberNameTextArguments() { return GET_MEMBER_NAME_CHECKED(UEGQuestNode_Objective, TextArguments); }

protected:
	//
	// The evaluation, overridable per subclass. Runs on the authority-only evaluator instance.
	//

	//
	// These two run only for an objective with no tracker - one that authors nothing to evaluate.
	// The authored fields are synthesised into trackers before activation (see EnsureTrackers), so
	// both native defaults are empty and exist purely as the Blueprint subclass hook.
	//

	/** The stage was entered and this evaluator is live. */
	UFUNCTION(BlueprintNativeEvent, Category = "Quest|Objective")
	void OnActivated();
	virtual void OnActivated_Implementation();

	/**
	 * The authority accepted a gameplay event while this objective is pending. ProgressDelta is the
	 * event's rounded, signed magnitude.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Quest|Objective")
	void OnQuestGameplayEvent(const FEGQuestGameplayEvent& Event, int32 ProgressDelta);
	virtual void OnQuestGameplayEvent_Implementation(const FEGQuestGameplayEvent& Event, int32 ProgressDelta);

	/** The stage is being left (resolved, cancelled or the quest ended). Undo any bindings made in OnActivated. */
	UFUNCTION(BlueprintNativeEvent, Category = "Quest|Objective")
	void OnDeactivated();
	virtual void OnDeactivated_Implementation() {}

protected:
	//
	// Legacy authored fields, load-only. Quests saved before tracker-only authoring store these
	// instead of a Tracker subobject; EnsureTrackers turns them into tracker templates on load.
	// Hidden from the editor and Blueprint on purpose - the tracker is the single authority for
	// what completes or fails an objective. Deletable once every asset has been resaved (verify
	// with a name-table scan: no uasset containing "EventTag" under an objective).
	//

	UPROPERTY()
	FGameplayTag EventTag;

	UPROPERTY()
	int32 RequiredCount = 1;

	UPROPERTY()
	bool bCompleteOnStageEnter = false;

	UPROPERTY()
	FGameplayTag FailEventTag;

	UPROPERTY()
	int32 FailRequiredCount = 1;

	// Localized objective text.
	// If you want replaceable portions inside your Text nodes just add {identifier} inside it and set the value it should have at runtime.
	UPROPERTY(EditAnywhere, Category = "Objective", Meta = (MultiLine = true))
	FText Text;

	// If you want replaceable portions inside your Text nodes just add {identifier} inside it and set the value it should have at runtime.
	UPROPERTY(EditAnywhere, EditFixedSize, Category = "Objective")
	TArray<FEGQuestTextArgument> TextArguments;

	/** What completes this objective. None = manual: only authority code or a Blueprint subclass completes it. */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Objective")
	TObjectPtr<UEGQuestTracker> Tracker = nullptr;

	/** What fails this objective. None = this objective can never fail. Its presence alone creates the fail pin. */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Objective")
	TObjectPtr<UEGQuestTracker> FailureTracker = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|Presentation") bool bOptional = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|Presentation") bool bHidden = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|Presentation") FName UITag = NAME_None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|Presentation") int32 SortOrder = 0;
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Objective|Feedback") TArray<TObjectPtr<UEGQuestEventCustom>> OnSuccessEvents;
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Objective|Feedback") TArray<TObjectPtr<UEGQuestEventCustom>> OnFailEvents;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|Feedback") TArray<FEGQuestObjectiveMilestone> Milestones;

private:
	/**
	 * Synthesises the inline tracker templates the authored fields describe. Idempotent: an authored
	 * Tracker always wins, and calling this twice is a no-op.
	 *
	 * Assets on disk store EventTag/RequiredCount rather than a Tracker subobject, so this is how
	 * every quest in the project actually acquires its trackers. Runs from PostLoad and again from
	 * ActivateEvaluator, so that "an objective has its trackers by the time it evaluates" is a
	 * property this class enforces rather than one inherited from where its instances come from.
	 */
	void EnsureTrackers();

	// Set only on the transient evaluator copies (see the class comment). Authored asset nodes keep
	// holding no runtime state; the counters themselves live in the replicated snapshot line.
	UPROPERTY(Transient)
	TWeakObjectPtr<UEGQuestComponent> EvaluatorOwner;

	FGuid EvaluatorInstanceGuid;
};
