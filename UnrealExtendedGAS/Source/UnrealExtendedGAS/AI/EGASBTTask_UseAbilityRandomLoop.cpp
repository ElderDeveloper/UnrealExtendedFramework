#include "AI/EGASBTTask_UseAbilityRandomLoop.h"

#include "BehaviorTree/BehaviorTreeComponent.h"

UEGASBTTask_UseAbilityRandomLoop::UEGASBTTask_UseAbilityRandomLoop(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NodeName = TEXT("Use Ability Random Loop");
}

uint16 UEGASBTTask_UseAbilityRandomLoop::GetInstanceMemorySize() const
{
	return sizeof(FEGASBTUseAbilityRandomLoopMemory);
}

EBTNodeResult::Type UEGASBTTask_UseAbilityRandomLoop::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	FEGASBTUseAbilityRandomLoopMemory* Memory = CastInstanceNodeMemory<FEGASBTUseAbilityRandomLoopMemory>(NodeMemory);

	// Tolerate the bounds being authored the wrong way round, and always use at least once.
	const int32 TotalUses = FMath::Max(1, FMath::RandRange(
		FMath::Min(MinAbilityUse, MaxAbilityUse),
		FMath::Max(MinAbilityUse, MaxAbilityUse)));

	Memory->RemainingUses = TotalUses - 1;
	Memory->bWaitingToFinish = false;

	if (!TryActivateAbility(OwnerComp, Ability))
	{
		return EBTNodeResult::Failed;
	}

	if (Memory->RemainingUses <= 0)
	{
		return FinishOrWait(NodeMemory);
	}

	SetNextTickTime(NodeMemory, TimeBetweenAbilities);
	return EBTNodeResult::InProgress;
}

void UEGASBTTask_UseAbilityRandomLoop::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	FEGASBTUseAbilityRandomLoopMemory* Memory = CastInstanceNodeMemory<FEGASBTUseAbilityRandomLoopMemory>(NodeMemory);

	if (Memory->bWaitingToFinish)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		return;
	}

	if (!TryActivateAbility(OwnerComp, Ability))
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	Memory->RemainingUses--;

	if (Memory->RemainingUses > 0)
	{
		SetNextTickTime(NodeMemory, TimeBetweenAbilities);
		return;
	}

	if (bWaitAfterUse && WaitTime > 0.f)
	{
		Memory->bWaitingToFinish = true;
		SetNextTickTime(NodeMemory, WaitTime);
		return;
	}

	FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
}

FString UEGASBTTask_UseAbilityRandomLoop::GetStaticDescription() const
{
	const int32 LowBound = FMath::Min(MinAbilityUse, MaxAbilityUse);
	const int32 HighBound = FMath::Max(MinAbilityUse, MaxAbilityUse);

	FString Description = (LowBound == HighBound)
		? FString::Printf(TEXT("Activate %s on %s %d times"), *Ability.ToDescription(), *GetTargetDescription(), HighBound)
		: FString::Printf(TEXT("Activate %s on %s %d-%d times"), *Ability.ToDescription(), *GetTargetDescription(), LowBound, HighBound);

	if (TimeBetweenAbilities > 0.f)
	{
		Description += FString::Printf(TEXT("\n%ss apart"), *FString::SanitizeFloat(TimeBetweenAbilities));
	}

	return Description + GetWaitDescription();
}
