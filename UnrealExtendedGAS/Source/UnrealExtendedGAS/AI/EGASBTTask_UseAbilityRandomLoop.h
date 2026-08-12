#pragma once

#include "AI/EGASBTTask_UseAbilityBase.h"
#include "CoreMinimal.h"

#include "EGASBTTask_UseAbilityRandomLoop.generated.h"

/** Per-agent state for one execution of the looping task. */
struct FEGASBTUseAbilityRandomLoopMemory
{
	/** Activations still owed after the one just performed. */
	int32 RemainingUses = 0;

	/** True once the loop is done and the node is only serving the post-use wait. */
	bool bWaitingToFinish = false;
};

/**
 * Activates the same gameplay ability a random number of times in a row, spaced
 * TimeBetweenAbilities apart, then succeeds.
 *
 * The repeat count is rolled per execution between MinAbilityUse and
 * MaxAbilityUse inclusive, and at least one activation is always attempted.
 * An activation that fails at any point ends the task as Failed.
 */
UCLASS(meta = (DisplayName = "Use Ability Random Loop"))
class UNREALEXTENDEDGAS_API UEGASBTTask_UseAbilityRandomLoop : public UEGASBTTask_UseAbilityBase
{
	GENERATED_BODY()

public:
	UEGASBTTask_UseAbilityRandomLoop(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Ability to activate repeatedly, keyed by tag or by class. */
	UPROPERTY(EditAnywhere, Category = "Ability")
	FEGASBTAbilitySelector Ability;

	/** Lowest number of activations for one execution. */
	UPROPERTY(EditAnywhere, Category = "Ability", meta = (ClampMin = "1", UIMin = "1"))
	int32 MinAbilityUse = 1;

	/** Highest number of activations for one execution. */
	UPROPERTY(EditAnywhere, Category = "Ability", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaxAbilityUse = 1;

	/** Seconds between consecutive activations. Zero activates once per frame. */
	UPROPERTY(EditAnywhere, Category = "Ability", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float TimeBetweenAbilities = 0.f;

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual uint16 GetInstanceMemorySize() const override;
	virtual FString GetStaticDescription() const override;

protected:
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
};
