#include "AI/EGASBTTask_UseAbilityBase.h"

#include "AbilitySystemComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"

bool FEGASBTAbilitySelector::IsSet() const
{
	return AbilityClass != nullptr || AbilityTag.IsValid();
}

FString FEGASBTAbilitySelector::ToDescription() const
{
	if (AbilityClass)
	{
		return AbilityClass->GetName();
	}

	if (AbilityTag.IsValid())
	{
		return AbilityTag.ToString();
	}

	return TEXT("<no ability set>");
}

UEGASBTTask_UseAbilityBase::UEGASBTTask_UseAbilityBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// The post-activation wait is driven by tick intervals rather than a timer, so
	// it is stored per agent in node memory and cannot outlive an aborted task.
	bTickIntervals = true;
	INIT_TASK_NODE_NOTIFY_FLAGS();
}

bool UEGASBTTask_UseAbilityBase::TryActivateAbility(const UBehaviorTreeComponent& OwnerComp, const FEGASBTAbilitySelector& Selector) const
{
	if (!Selector.IsSet())
	{
		return false;
	}

	UAbilitySystemComponent* AbilitySystemComponent = ResolveTargetAbilitySystem(OwnerComp);
	if (!AbilitySystemComponent)
	{
		return false;
	}

	if (Selector.AbilityClass)
	{
		return AbilitySystemComponent->TryActivateAbilityByClass(Selector.AbilityClass);
	}

	return AbilitySystemComponent->TryActivateAbilitiesByTag(FGameplayTagContainer(Selector.AbilityTag));
}

EBTNodeResult::Type UEGASBTTask_UseAbilityBase::FinishOrWait(uint8* NodeMemory) const
{
	if (bWaitAfterUse && WaitTime > 0.f)
	{
		SetNextTickTime(NodeMemory, WaitTime);
		return EBTNodeResult::InProgress;
	}

	return EBTNodeResult::Succeeded;
}

FString UEGASBTTask_UseAbilityBase::GetWaitDescription() const
{
	if (bWaitAfterUse && WaitTime > 0.f)
	{
		return FString::Printf(TEXT("\nthen wait %ss"), *FString::SanitizeFloat(WaitTime));
	}

	return FString();
}

void UEGASBTTask_UseAbilityBase::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	// Tick intervals mean this only runs once the post-activation wait has elapsed.
	FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
}
