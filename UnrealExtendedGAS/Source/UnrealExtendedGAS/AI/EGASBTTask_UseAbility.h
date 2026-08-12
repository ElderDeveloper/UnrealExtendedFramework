#pragma once

#include "AI/EGASBTTask_UseAbilityBase.h"
#include "CoreMinimal.h"

#include "EGASBTTask_UseAbility.generated.h"

/**
 * Activates one gameplay ability on the AI's ability system component.
 *
 * Fails when the ability could not be activated (no ASC, nothing matching the
 * selector, blocked tags, cost or cooldown). Otherwise succeeds, optionally
 * staying latent for WaitTime first.
 */
UCLASS(meta = (DisplayName = "Use Ability"))
class UNREALEXTENDEDGAS_API UEGASBTTask_UseAbility : public UEGASBTTask_UseAbilityBase
{
	GENERATED_BODY()

public:
	UEGASBTTask_UseAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Ability to activate, keyed by tag or by class. */
	UPROPERTY(EditAnywhere, Category = "Ability")
	FEGASBTAbilitySelector Ability;

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;
};
