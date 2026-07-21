// Copyright Devil of the Plague. All Rights Reserved.
#include "EGQuestPredicate.h"

#include "EGQuestComponent.h"
#include "Engine/World.h"

bool EGQuestCompareNumber(double Actual, EEGQuestNumericComparison Comparison, double Value, double MaxValue)
{
	switch (Comparison)
	{
		case EEGQuestNumericComparison::Equal: return Actual == Value;
		case EEGQuestNumericComparison::NotEqual: return Actual != Value;
		case EEGQuestNumericComparison::GreaterOrEqual: return Actual >= Value;
		case EEGQuestNumericComparison::Greater: return Actual > Value;
		case EEGQuestNumericComparison::LessOrEqual: return Actual <= Value;
		case EEGQuestNumericComparison::Less: return Actual < Value;
		case EEGQuestNumericComparison::InRangeInclusive: return Actual >= FMath::Min(Value, MaxValue) && Actual <= FMath::Max(Value, MaxValue);
		default: return false;
	}
}

bool UEGQuestPredicate_Fact::Evaluate_Implementation(const FEGQuestPredicateContext& Context) const
{
	const UWorld* World = Context.Component ? Context.Component->GetWorld() : nullptr;
	const UEGQuestFactsSubsystem* Facts = World ? World->GetSubsystem<UEGQuestFactsSubsystem>() : nullptr;
	return Facts && FactTag.IsValid() && EGQuestCompareNumber(Facts->GetFact(FactTag, Scope, Context.Player), Comparison, Value, MaxValue);
}

void UEGQuestPredicate_Fact::GetFactDependencies(FGameplayTagContainer& OutDependencies) const
{
	if (FactTag.IsValid()) OutDependencies.AddTag(FactTag);
}

bool UEGQuestPredicate_Not::Evaluate_Implementation(const FEGQuestPredicateContext& Context) const
{
	return !Child || !Child->Evaluate(Context);
}

void UEGQuestPredicate_Not::GetFactDependencies(FGameplayTagContainer& OutDependencies) const
{
	if (Child) Child->GetFactDependencies(OutDependencies);
}

bool UEGQuestPredicate_All::Evaluate_Implementation(const FEGQuestPredicateContext& Context) const
{
	for (const UEGQuestPredicate* Child : Children) if (Child && !Child->Evaluate(Context)) return false;
	return true;
}

void UEGQuestPredicate_All::GetFactDependencies(FGameplayTagContainer& OutDependencies) const
{
	for (const UEGQuestPredicate* Child : Children) if (Child) Child->GetFactDependencies(OutDependencies);
}

bool UEGQuestPredicate_Any::Evaluate_Implementation(const FEGQuestPredicateContext& Context) const
{
	for (const UEGQuestPredicate* Child : Children) if (Child && Child->Evaluate(Context)) return true;
	return false;
}

void UEGQuestPredicate_Any::GetFactDependencies(FGameplayTagContainer& OutDependencies) const
{
	for (const UEGQuestPredicate* Child : Children) if (Child) Child->GetFactDependencies(OutDependencies);
}

bool UEGQuestPredicate_Time::Evaluate_Implementation(const FEGQuestPredicateContext& Context) const
{
	return Context.Component && EGQuestCompareNumber(Context.Component->GetQuestServerTime(), Comparison, ServerTime, MaxServerTime);
}
