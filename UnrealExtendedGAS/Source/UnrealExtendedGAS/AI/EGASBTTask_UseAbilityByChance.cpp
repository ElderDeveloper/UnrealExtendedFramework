#include "AI/EGASBTTask_UseAbilityByChance.h"

#include "BehaviorTree/BehaviorTreeComponent.h"

UEGASBTTask_UseAbilityByChance::UEGASBTTask_UseAbilityByChance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NodeName = TEXT("Use Ability By Chance");
}

EBTNodeResult::Type UEGASBTTask_UseAbilityByChance::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	// FRand is [0,1), so this fires with probability exactly Chance at both ends of the range.
	if (FMath::FRand() >= Chance)
	{
		return EBTNodeResult::Succeeded;
	}

	if (!TryActivateAbility(OwnerComp, Ability))
	{
		return EBTNodeResult::Failed;
	}

	return FinishOrWait(NodeMemory);
}

FString UEGASBTTask_UseAbilityByChance::GetStaticDescription() const
{
	return FString::Printf(TEXT("Activate %s on %s\n%.0f%% of the time%s"),
		*Ability.ToDescription(),
		*GetTargetDescription(),
		FMath::Clamp(Chance, 0.f, 1.f) * 100.f,
		*GetWaitDescription());
}
