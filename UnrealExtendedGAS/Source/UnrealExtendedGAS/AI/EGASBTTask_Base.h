#pragma once

#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "CoreMinimal.h"

#include "EGASBTTask_Base.generated.h"

class UAbilitySystemComponent;

/**
 * Shared base for the behaviour tree tasks that act on an ability system component.
 *
 * Resolves which actor to act on — the AI's own pawn, or an actor read out of a
 * blackboard key — and finds its ability system component.
 */
UCLASS(Abstract)
class UNREALEXTENDEDGAS_API UEGASBTTask_Base : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UEGASBTTask_Base(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;

protected:
	/** Act on the AI's own pawn. Clear this to act on the actor in TargetActorKey instead. */
	UPROPERTY(EditAnywhere, Category = "Target")
	bool bTargetSelf = true;

	/** Blackboard key holding the actor to act on. Read only when bTargetSelf is false. */
	UPROPERTY(EditAnywhere, Category = "Target", meta = (EditCondition = "!bTargetSelf"))
	FBlackboardKeySelector TargetActorKey;

	/** The AI's pawn, or the blackboard actor. Null when it cannot be resolved. */
	AActor* ResolveTargetActor(const UBehaviorTreeComponent& OwnerComp) const;

	/** Ability system component of the resolved target, or null. */
	UAbilitySystemComponent* ResolveTargetAbilitySystem(const UBehaviorTreeComponent& OwnerComp) const;

	/** "Self" or the blackboard key name, for GetStaticDescription. */
	FString GetTargetDescription() const;
};
