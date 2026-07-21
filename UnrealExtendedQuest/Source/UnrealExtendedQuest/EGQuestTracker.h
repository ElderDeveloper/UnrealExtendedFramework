// Copyright Devil of the Plague. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "EGQuestPredicate.h"
#include "EGQuestTypes.h"
#include "GameplayTagContainer.h"
#include "EGQuestTracker.generated.h"

class AActor;
class APlayerState;
class UEGQuestFactsSubsystem;
class UEGQuestNode_Objective;

UENUM(BlueprintType)
enum class EEGQuestSequenceMismatchPolicy : uint8
{
	Ignore,
	Reset,
	Fail
};

UENUM(BlueprintType)
enum class EEGQuestCompositeMode : uint8
{
	All,
	Any,
	KOfN
};

/**
 * Stateful evaluation contract. Authored instances are inline templates; active objectives own a
 * transient duplicate. All persistent state is written through the objective into its run-record line.
 */
UCLASS(Abstract, Blueprintable, BlueprintType, EditInlineNew, DefaultToInstanced)
class UNREALEXTENDEDQUEST_API UEGQuestTracker : public UObject
{
	GENERATED_BODY()

public:
	void Activate(UEGQuestNode_Objective& InBinding, EEGQuestActivationReason Reason);
	void Deactivate(EEGQuestDeactivationReason Reason);
	void HandleEvent(const FEGQuestGameplayEvent& Event, int32 ProgressDelta);
	void Pulse();

	UFUNCTION(BlueprintPure, Category = "Quest|Tracker") bool IsActive() const { return Binding.IsValid(); }
	UFUNCTION(BlueprintPure, Category = "Quest|Tracker") bool IsSatisfied() const { return bSatisfied; }
	UFUNCTION(BlueprintPure, Category = "Quest|Tracker") UEGQuestNode_Objective* GetBinding() const { return Binding.Get(); }

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EEGQuestObjectiveOutcome OutcomeOnSatisfied = EEGQuestObjectiveOutcome::Succeeded;

protected:
	UFUNCTION(BlueprintNativeEvent, Category = "Quest|Tracker") void OnActivated(EEGQuestActivationReason Reason);
	virtual void OnActivated_Implementation(EEGQuestActivationReason Reason) {}
	UFUNCTION(BlueprintNativeEvent, Category = "Quest|Tracker") void OnDeactivated(EEGQuestDeactivationReason Reason);
	virtual void OnDeactivated_Implementation(EEGQuestDeactivationReason Reason) {}
	UFUNCTION(BlueprintNativeEvent, Category = "Quest|Tracker") void OnGameplayEvent(const FEGQuestGameplayEvent& Event, int32 ProgressDelta);
	virtual void OnGameplayEvent_Implementation(const FEGQuestGameplayEvent& Event, int32 ProgressDelta) {}
	UFUNCTION(BlueprintNativeEvent, Category = "Quest|Tracker") void OnPulse();
	virtual void OnPulse_Implementation() {}

	/** Idempotently proposes this tracker's authored outcome through the production executor. */
	UFUNCTION(BlueprintCallable, Category = "Quest|Tracker") void Satisfy();
	virtual void ChildSatisfied(UEGQuestTracker& Child);
	FEGQuestPredicateContext MakePredicateContext() const;
	APlayerState* ResolvePlayerScope() const;

private:
	friend class UEGQuestTracker_Composite;
	UPROPERTY(Transient) TWeakObjectPtr<UEGQuestNode_Objective> Binding;
	UPROPERTY(Transient) TWeakObjectPtr<UEGQuestTracker> ParentTracker;
	UPROPERTY(Transient) bool bSatisfied = false;
};

UCLASS(BlueprintType, EditInlineNew)
class UNREALEXTENDEDQUEST_API UEGQuestTracker_Immediate : public UEGQuestTracker
{
	GENERATED_BODY()
protected:
	void OnActivated_Implementation(EEGQuestActivationReason Reason) override { Satisfy(); }
};

UCLASS(BlueprintType, EditInlineNew)
class UNREALEXTENDEDQUEST_API UEGQuestTracker_Fact : public UEGQuestTracker
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FGameplayTag FactTag;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EEGQuestFactScope Scope = EEGQuestFactScope::World;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EEGQuestNumericComparison Comparison = EEGQuestNumericComparison::GreaterOrEqual;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Value = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 MaxValue = 1;
protected:
	void OnActivated_Implementation(EEGQuestActivationReason Reason) override;
	void OnDeactivated_Implementation(EEGQuestDeactivationReason Reason) override;
	void OnPulse_Implementation() override;
	UFUNCTION() void HandleFactChanged(FGameplayTag Tag, int32 OldValue, int32 NewValue,
		EEGQuestFactScope ChangedScope, APlayerState* Player, AActor* Instigator);
};

UCLASS(BlueprintType, EditInlineNew)
class UNREALEXTENDEDQUEST_API UEGQuestTracker_EventCount : public UEGQuestTracker
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FGameplayTagQuery EventQuery;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FGameplayTag ExactEventTag;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1")) int32 RequiredCount = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bUseMagnitudePredicate = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EEGQuestNumericComparison MagnitudeComparison = EEGQuestNumericComparison::GreaterOrEqual;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MagnitudeValue = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MaxMagnitudeValue = 0.0f;
protected:
	void OnGameplayEvent_Implementation(const FEGQuestGameplayEvent& Event, int32 ProgressDelta) override;
};

UCLASS(BlueprintType, EditInlineNew)
class UNREALEXTENDEDQUEST_API UEGQuestTracker_Timer : public UEGQuestTracker
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.0")) double DurationSeconds = 1.0;
protected:
	void OnActivated_Implementation(EEGQuestActivationReason Reason) override;
	void OnPulse_Implementation() override;
};

UCLASS(BlueprintType, EditInlineNew)
class UNREALEXTENDEDQUEST_API UEGQuestTracker_Sequence : public UEGQuestTracker
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FGameplayTag> OrderedTags;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EEGQuestSequenceMismatchPolicy MismatchPolicy = EEGQuestSequenceMismatchPolicy::Reset;
protected:
	void OnGameplayEvent_Implementation(const FEGQuestGameplayEvent& Event, int32 ProgressDelta) override;
};

UCLASS(BlueprintType, EditInlineNew)
class UNREALEXTENDEDQUEST_API UEGQuestTracker_Distinct : public UEGQuestTracker
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FGameplayTagQuery EventQuery;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FGameplayTag ExactEventTag;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1")) int32 RequiredDistinctCount = 1;
protected:
	void OnGameplayEvent_Implementation(const FEGQuestGameplayEvent& Event, int32 ProgressDelta) override;
};

UCLASS(BlueprintType, EditInlineNew)
class UNREALEXTENDEDQUEST_API UEGQuestTracker_Composite : public UEGQuestTracker
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EEGQuestCompositeMode Mode = EEGQuestCompositeMode::All;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1")) int32 RequiredChildren = 1;
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite) TArray<TObjectPtr<UEGQuestTracker>> Children;
protected:
	void OnActivated_Implementation(EEGQuestActivationReason Reason) override;
	void OnDeactivated_Implementation(EEGQuestDeactivationReason Reason) override;
	void OnGameplayEvent_Implementation(const FEGQuestGameplayEvent& Event, int32 ProgressDelta) override;
	void OnPulse_Implementation() override;
	void ChildSatisfied(UEGQuestTracker& Child) override;
private:
	void EvaluateChildren();
};

UCLASS(BlueprintType, EditInlineNew)
class UNREALEXTENDEDQUEST_API UEGQuestTracker_Predicate : public UEGQuestTracker
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite) TObjectPtr<UEGQuestPredicate> Predicate;
protected:
	void OnActivated_Implementation(EEGQuestActivationReason Reason) override;
	void OnDeactivated_Implementation(EEGQuestDeactivationReason Reason) override;
	void OnPulse_Implementation() override;
	UFUNCTION() void HandleFactChanged(FGameplayTag Tag, int32 OldValue, int32 NewValue,
		EEGQuestFactScope ChangedScope, APlayerState* Player, AActor* Instigator);
private:
	FGameplayTagContainer Dependencies;
};

/** Blueprint escape hatch; author a subclass and implement the base tracker events. */
UCLASS(Blueprintable, BlueprintType, EditInlineNew)
class UNREALEXTENDEDQUEST_API UEGQuestTracker_Scripted : public UEGQuestTracker
{
	GENERATED_BODY()
};
