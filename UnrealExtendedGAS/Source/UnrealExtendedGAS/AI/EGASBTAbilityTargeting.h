#pragma once

#include "CoreMinimal.h"

class AActor;
class UAbilitySystemComponent;
class UBehaviorTreeComponent;
struct FBlackboardKeySelector;

/**
 * Target resolution shared by the GAS behaviour tree tasks and decorators.
 *
 * Tasks derive from UBTTaskNode and decorators from UBTDecorator, so the two
 * families cannot share a base class — they share these helpers instead.
 */
namespace EGASBTAbilityTargeting
{
	/** The AI's own pawn when bTargetSelf, otherwise the actor held in TargetActorKey. */
	UNREALEXTENDEDGAS_API AActor* ResolveTargetActor(const UBehaviorTreeComponent& OwnerComp, bool bTargetSelf, const FBlackboardKeySelector& TargetActorKey);

	/**
	 * Ability system component of the resolved target. Checks IAbilitySystemInterface
	 * first and falls back to a component search, so Blueprint-only pawns (which
	 * cannot implement that interface) still resolve. When self-targeting it also
	 * tries the controller, for setups that keep the ASC there.
	 */
	UNREALEXTENDEDGAS_API UAbilitySystemComponent* ResolveTargetAbilitySystem(const UBehaviorTreeComponent& OwnerComp, bool bTargetSelf, const FBlackboardKeySelector& TargetActorKey);

	/** The AI's own ability system component, used as the instigator when affecting another actor. */
	UNREALEXTENDEDGAS_API UAbilitySystemComponent* ResolveInstigatorAbilitySystem(const UBehaviorTreeComponent& OwnerComp);
}
