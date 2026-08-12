#include "AI/EGASBTAbilityTargeting.h"

#include "AIController.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Pawn.h"

namespace EGASBTAbilityTargeting
{
	AActor* ResolveTargetActor(const UBehaviorTreeComponent& OwnerComp, bool bTargetSelf, const FBlackboardKeySelector& TargetActorKey)
	{
		if (bTargetSelf)
		{
			const AAIController* AIOwner = OwnerComp.GetAIOwner();
			return AIOwner ? AIOwner->GetPawn() : nullptr;
		}

		const UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
		return Blackboard ? Cast<AActor>(Blackboard->GetValueAsObject(TargetActorKey.SelectedKeyName)) : nullptr;
	}

	UAbilitySystemComponent* ResolveInstigatorAbilitySystem(const UBehaviorTreeComponent& OwnerComp)
	{
		const AAIController* AIOwner = OwnerComp.GetAIOwner();
		if (!AIOwner)
		{
			return nullptr;
		}

		if (const APawn* Pawn = AIOwner->GetPawn())
		{
			if (UAbilitySystemComponent* PawnAbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn))
			{
				return PawnAbilitySystem;
			}
		}

		// Some setups keep the ASC on the controller rather than the pawn.
		return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(AIOwner);
	}

	UAbilitySystemComponent* ResolveTargetAbilitySystem(const UBehaviorTreeComponent& OwnerComp, bool bTargetSelf, const FBlackboardKeySelector& TargetActorKey)
	{
		if (bTargetSelf)
		{
			return ResolveInstigatorAbilitySystem(OwnerComp);
		}

		const AActor* TargetActor = ResolveTargetActor(OwnerComp, bTargetSelf, TargetActorKey);
		return TargetActor ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor) : nullptr;
	}
}
