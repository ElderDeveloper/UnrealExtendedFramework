#pragma once

#include "AI/EGASBTTask_UseAbilityBase.h"
#include "CoreMinimal.h"

#include "EGASBTTask_UseRandomAbility.generated.h"

/**
 * Picks one ability at random out of Abilities and activates it on the AI's
 * ability system component.
 *
 * Entries left blank are skipped when picking, so an unfinished row cannot make
 * the roll land on nothing. Fails when the list holds no usable entry or the
 * chosen ability could not be activated.
 */
UCLASS(meta = (DisplayName = "Use Random Ability"))
class UNREALEXTENDEDGAS_API UEGASBTTask_UseRandomAbility : public UEGASBTTask_UseAbilityBase
{
	GENERATED_BODY()

public:
	UEGASBTTask_UseRandomAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Candidate abilities, each keyed by tag or by class. One is picked per execution. */
	UPROPERTY(EditAnywhere, Category = "Ability")
	TArray<FEGASBTAbilitySelector> Abilities;

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;
};
