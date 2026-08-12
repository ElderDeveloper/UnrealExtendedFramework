#pragma once

#include "AI/EGASBTTask_Base.h"
#include "CoreMinimal.h"

#include "EGASBTTask_ApplyGameplayEffect.generated.h"

class UGameplayEffect;

/**
 * Applies a gameplay effect to the AI itself or to the actor in TargetActorKey.
 *
 * The spec is built on the AI's own ability system component, so instigator and
 * source attribution point at the AI rather than at whoever is being affected —
 * which is what damage and similar effects read.
 */
UCLASS(meta = (DisplayName = "Apply Gameplay Effect"))
class UNREALEXTENDEDGAS_API UEGASBTTask_ApplyGameplayEffect : public UEGASBTTask_Base
{
	GENERATED_BODY()

public:
	UEGASBTTask_ApplyGameplayEffect(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Effect to apply. The task fails when this is unset. */
	UPROPERTY(EditAnywhere, Category = "Effect")
	TSubclassOf<UGameplayEffect> GameplayEffect;

	/** Level the effect is applied at. */
	UPROPERTY(EditAnywhere, Category = "Effect", meta = (ClampMin = "0", UIMin = "0"))
	int32 GameplayEffectLevel = 1;

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;
};
