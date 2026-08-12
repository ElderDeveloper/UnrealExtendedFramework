#include "AI/EGASBTTask_UseAbility.h"

#include "BehaviorTree/BehaviorTreeComponent.h"

UEGASBTTask_UseAbility::UEGASBTTask_UseAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NodeName = TEXT("Use Ability");
}

EBTNodeResult::Type UEGASBTTask_UseAbility::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (!TryActivateAbility(OwnerComp, Ability))
	{
		return EBTNodeResult::Failed;
	}

	return FinishOrWait(NodeMemory);
}

FString UEGASBTTask_UseAbility::GetStaticDescription() const
{
	return FString::Printf(TEXT("Activate %s on %s%s"), *Ability.ToDescription(), *GetTargetDescription(), *GetWaitDescription());
}
