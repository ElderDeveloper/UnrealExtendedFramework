#include "AI/EGASBTTask_RemoveGameplayEffect.h"

#include "AbilitySystemComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "GameplayEffect.h"

UEGASBTTask_RemoveGameplayEffect::UEGASBTTask_RemoveGameplayEffect(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NodeName = TEXT("Remove Gameplay Effect");
}

EBTNodeResult::Type UEGASBTTask_RemoveGameplayEffect::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	const bool bHasAnyFilter = EffectClassToRemove != nullptr
		|| !RemoveWithGrantedTags.IsEmpty()
		|| !RemoveWithSourceTags.IsEmpty()
		|| !RemoveWithAppliedTags.IsEmpty();

	if (!bHasAnyFilter)
	{
		return EBTNodeResult::Failed;
	}

	UAbilitySystemComponent* TargetAbilitySystem = ResolveTargetAbilitySystem(OwnerComp);
	if (!TargetAbilitySystem)
	{
		return EBTNodeResult::Failed;
	}

	if (EffectClassToRemove)
	{
		// A null instigator matches any source; passing one restricts removal to
		// effects that particular ASC applied.
		UAbilitySystemComponent* InstigatorFilter = bOnlyRemoveEffectsInstigatedBySelf ? TargetAbilitySystem : nullptr;
		TargetAbilitySystem->RemoveActiveGameplayEffectBySourceEffect(EffectClassToRemove, InstigatorFilter);
	}

	if (!RemoveWithGrantedTags.IsEmpty())
	{
		TargetAbilitySystem->RemoveActiveEffectsWithGrantedTags(RemoveWithGrantedTags);
	}

	if (!RemoveWithSourceTags.IsEmpty())
	{
		TargetAbilitySystem->RemoveActiveEffectsWithSourceTags(RemoveWithSourceTags);
	}

	if (!RemoveWithAppliedTags.IsEmpty())
	{
		TargetAbilitySystem->RemoveActiveEffectsWithAppliedTags(RemoveWithAppliedTags);
	}

	return EBTNodeResult::Succeeded;
}

FString UEGASBTTask_RemoveGameplayEffect::GetStaticDescription() const
{
	TArray<FString, TInlineAllocator<4>> Filters;

	if (EffectClassToRemove)
	{
		Filters.Add(EffectClassToRemove->GetName());
	}
	if (!RemoveWithGrantedTags.IsEmpty())
	{
		Filters.Add(FString::Printf(TEXT("granting %s"), *RemoveWithGrantedTags.ToStringSimple()));
	}
	if (!RemoveWithSourceTags.IsEmpty())
	{
		Filters.Add(FString::Printf(TEXT("from source %s"), *RemoveWithSourceTags.ToStringSimple()));
	}
	if (!RemoveWithAppliedTags.IsEmpty())
	{
		Filters.Add(FString::Printf(TEXT("applied with %s"), *RemoveWithAppliedTags.ToStringSimple()));
	}

	if (Filters.IsEmpty())
	{
		return TEXT("No removal filter set");
	}

	return FString::Printf(TEXT("Remove %s\nfrom %s"), *FString::Join(Filters, TEXT(", ")), *GetTargetDescription());
}
