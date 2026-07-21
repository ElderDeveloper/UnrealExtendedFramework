// Copyright Devil of the Plague. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "EGQuestFactsSubsystem.h"
#include "EGQuestPredicate.generated.h"

class APlayerState;
class UEGQuestComponent;

UENUM(BlueprintType)
enum class EEGQuestNumericComparison : uint8
{
	Equal,
	NotEqual,
	GreaterOrEqual,
	Greater,
	LessOrEqual,
	Less,
	InRangeInclusive
};

USTRUCT(BlueprintType)
struct UNREALEXTENDEDQUEST_API FEGQuestPredicateContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Quest|Predicate")
	TObjectPtr<UEGQuestComponent> Component = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Quest|Predicate")
	FGuid RunId;

	UPROPERTY(BlueprintReadOnly, Category = "Quest|Predicate")
	FGuid ObjectiveGuid;

	UPROPERTY(BlueprintReadOnly, Category = "Quest|Predicate")
	TObjectPtr<APlayerState> Player = nullptr;
};

/** Pure query contract: predicates hold no runtime state and bind no delegates. */
UCLASS(Abstract, Blueprintable, BlueprintType, EditInlineNew, DefaultToInstanced)
class UNREALEXTENDEDQUEST_API UEGQuestPredicate : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Quest|Predicate")
	bool Evaluate(const FEGQuestPredicateContext& Context) const;
	virtual bool Evaluate_Implementation(const FEGQuestPredicateContext& Context) const PURE_VIRTUAL(UEGQuestPredicate::Evaluate_Implementation, return false;);

	/** Declared reactive dependencies. Empty means the caller must explicitly pulse the predicate. */
	virtual void GetFactDependencies(FGameplayTagContainer& OutDependencies) const {}
};

UCLASS(BlueprintType, EditInlineNew)
class UNREALEXTENDEDQUEST_API UEGQuestPredicate_Fact : public UEGQuestPredicate
{
	GENERATED_BODY()
public:
	bool Evaluate_Implementation(const FEGQuestPredicateContext& Context) const override;
	void GetFactDependencies(FGameplayTagContainer& OutDependencies) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FGameplayTag FactTag;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EEGQuestFactScope Scope = EEGQuestFactScope::World;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EEGQuestNumericComparison Comparison = EEGQuestNumericComparison::GreaterOrEqual;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Value = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 MaxValue = 1;
};

UCLASS(BlueprintType, EditInlineNew)
class UNREALEXTENDEDQUEST_API UEGQuestPredicate_Not : public UEGQuestPredicate
{
	GENERATED_BODY()
public:
	bool Evaluate_Implementation(const FEGQuestPredicateContext& Context) const override;
	void GetFactDependencies(FGameplayTagContainer& OutDependencies) const override;
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite) TObjectPtr<UEGQuestPredicate> Child;
};

UCLASS(BlueprintType, EditInlineNew)
class UNREALEXTENDEDQUEST_API UEGQuestPredicate_All : public UEGQuestPredicate
{
	GENERATED_BODY()
public:
	bool Evaluate_Implementation(const FEGQuestPredicateContext& Context) const override;
	void GetFactDependencies(FGameplayTagContainer& OutDependencies) const override;
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite) TArray<TObjectPtr<UEGQuestPredicate>> Children;
};

UCLASS(BlueprintType, EditInlineNew)
class UNREALEXTENDEDQUEST_API UEGQuestPredicate_Any : public UEGQuestPredicate
{
	GENERATED_BODY()
public:
	bool Evaluate_Implementation(const FEGQuestPredicateContext& Context) const override;
	void GetFactDependencies(FGameplayTagContainer& OutDependencies) const override;
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite) TArray<TObjectPtr<UEGQuestPredicate>> Children;
};

/** Pure injected-clock query, useful for availability and reveal rules. */
UCLASS(BlueprintType, EditInlineNew)
class UNREALEXTENDEDQUEST_API UEGQuestPredicate_Time : public UEGQuestPredicate
{
	GENERATED_BODY()
public:
	bool Evaluate_Implementation(const FEGQuestPredicateContext& Context) const override;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EEGQuestNumericComparison Comparison = EEGQuestNumericComparison::GreaterOrEqual;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) double ServerTime = 0.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) double MaxServerTime = 0.0;
};

UNREALEXTENDEDQUEST_API bool EGQuestCompareNumber(double Actual, EEGQuestNumericComparison Comparison,
	double Value, double MaxValue);
