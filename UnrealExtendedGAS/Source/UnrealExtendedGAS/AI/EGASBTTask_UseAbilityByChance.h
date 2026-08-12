#pragma once

#include "AI/EGASBTTask_UseAbilityBase.h"
#include "CoreMinimal.h"

#include "EGASBTTask_UseAbilityByChance.generated.h"

/**
 * Rolls against Chance and, if it hits, activates one gameplay ability on the
 * AI's ability system component.
 *
 * A missed roll succeeds without using anything, so the task can sit in a
 * sequence without breaking it. Only a roll that hit but failed to activate
 * reports Failed, and the post-use wait only applies when an ability was used.
 */
UCLASS(meta = (DisplayName = "Use Ability By Chance"))
class UNREALEXTENDEDGAS_API UEGASBTTask_UseAbilityByChance : public UEGASBTTask_UseAbilityBase
{
	GENERATED_BODY()

public:
	UEGASBTTask_UseAbilityByChance(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Ability to activate, keyed by tag or by class. */
	UPROPERTY(EditAnywhere, Category = "Ability")
	FEGASBTAbilitySelector Ability;

	/** Probability of using the ability at all. 0 never fires, 1 always fires. */
	UPROPERTY(EditAnywhere, Category = "Ability", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float Chance = 0.5f;

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;
};
