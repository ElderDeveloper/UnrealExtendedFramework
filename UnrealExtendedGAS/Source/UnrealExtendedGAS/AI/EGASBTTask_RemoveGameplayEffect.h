#pragma once

#include "AI/EGASBTTask_Base.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "EGASBTTask_RemoveGameplayEffect.generated.h"

class UGameplayEffect;

/**
 * Removes gameplay effects from the AI itself or from the actor in TargetActorKey.
 *
 * Every filter that is filled in is applied, so one node can strip an effect class
 * and several tag groups at once. Fails when no filter is set or the target has no
 * ability system component.
 */
UCLASS(meta = (DisplayName = "Remove Gameplay Effect"))
class UNREALEXTENDEDGAS_API UEGASBTTask_RemoveGameplayEffect : public UEGASBTTask_Base
{
	GENERATED_BODY()

public:
	UEGASBTTask_RemoveGameplayEffect(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Remove active effects backed by this exact effect class. */
	UPROPERTY(EditAnywhere, Category = "Effect")
	TSubclassOf<UGameplayEffect> EffectClassToRemove;

	/**
	 * Restrict the class removal above to effects the AI itself applied. Off by
	 * default, so an effect is removed whoever applied it.
	 */
	UPROPERTY(EditAnywhere, Category = "Effect", meta = (EditCondition = "EffectClassToRemove != nullptr"))
	bool bOnlyRemoveEffectsInstigatedBySelf = false;

	/** Remove active effects that grant any of these tags. */
	UPROPERTY(EditAnywhere, Category = "Effect")
	FGameplayTagContainer RemoveWithGrantedTags;

	/** Remove active effects whose source carries any of these tags. */
	UPROPERTY(EditAnywhere, Category = "Effect")
	FGameplayTagContainer RemoveWithSourceTags;

	/** Remove active effects that were applied with any of these tags. */
	UPROPERTY(EditAnywhere, Category = "Effect")
	FGameplayTagContainer RemoveWithAppliedTags;

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;
};
