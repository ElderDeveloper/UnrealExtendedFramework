#include "AI/EGASBTTask_UseRandomAbility.h"

#include "BehaviorTree/BehaviorTreeComponent.h"

UEGASBTTask_UseRandomAbility::UEGASBTTask_UseRandomAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NodeName = TEXT("Use Random Ability");
}

EBTNodeResult::Type UEGASBTTask_UseRandomAbility::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	TArray<const FEGASBTAbilitySelector*, TInlineAllocator<8>> Candidates;
	for (const FEGASBTAbilitySelector& Selector : Abilities)
	{
		if (Selector.IsSet())
		{
			Candidates.Add(&Selector);
		}
	}

	if (Candidates.IsEmpty())
	{
		return EBTNodeResult::Failed;
	}

	const FEGASBTAbilitySelector& Chosen = *Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
	if (!TryActivateAbility(OwnerComp, Chosen))
	{
		return EBTNodeResult::Failed;
	}

	return FinishOrWait(NodeMemory);
}

FString UEGASBTTask_UseRandomAbility::GetStaticDescription() const
{
	FString Description = FString::Printf(TEXT("Activate one of, on %s:"), *GetTargetDescription());
	for (const FEGASBTAbilitySelector& Selector : Abilities)
	{
		Description += FString::Printf(TEXT("\n  %s"), *Selector.ToDescription());
	}

	if (Abilities.IsEmpty())
	{
		Description += TEXT("\n  <no abilities set>");
	}

	return Description + GetWaitDescription();
}
