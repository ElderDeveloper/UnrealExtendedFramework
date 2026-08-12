#pragma once

#include "BehaviorTree/BTDecorator.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "CoreMinimal.h"

#include "EGASBTDecorator_Base.generated.h"

class UAbilitySystemComponent;

/**
 * Shared base for the behaviour tree decorators that read from an ability system
 * component.
 *
 * These decorators observe the ASC and hold per-agent delegate handles, so they
 * force node instancing: one behaviour tree node object is otherwise shared by
 * every agent running the tree, and the agents would overwrite and unbind each
 * other's observers.
 *
 * Change tracking only re-runs the branch when the node's Observer Aborts mode is
 * set to something other than None in the details panel — that is what gives the
 * decorator a flow abort mode to act on.
 */
UCLASS(Abstract)
class UNREALEXTENDEDGAS_API UEGASBTDecorator_Base : public UBTDecorator
{
	GENERATED_BODY()

public:
	UEGASBTDecorator_Base(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;

protected:
	/** Read from the AI's own pawn. Clear this to read the actor in TargetActorKey instead. */
	UPROPERTY(EditAnywhere, Category = "Target")
	bool bTargetSelf = true;

	/** Blackboard key holding the actor to read. Used only when bTargetSelf is false. */
	UPROPERTY(EditAnywhere, Category = "Target", meta = (EditCondition = "!bTargetSelf"))
	FBlackboardKeySelector TargetActorKey;

	/** The AI's pawn, or the blackboard actor. Null when it cannot be resolved. */
	AActor* ResolveTargetActor(const UBehaviorTreeComponent& OwnerComp) const;

	/** Ability system component of the resolved target, or null. */
	UAbilitySystemComponent* ResolveTargetAbilitySystem(const UBehaviorTreeComponent& OwnerComp) const;

	/** "Self" or the blackboard key name, for GetStaticDescription. */
	FString GetTargetDescription() const;
};
