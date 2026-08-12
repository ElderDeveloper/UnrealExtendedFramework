#pragma once

#include "AI/EGASBTTask_Base.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "EGASBTTask_UseAbilityBase.generated.h"

class UGameplayAbility;

/**
 * Identifies one ability to activate, either by gameplay tag or by class.
 *
 * AbilityClass takes priority when both are filled in: it activates one exact
 * ability, where AbilityTag is matched against the granted abilities' asset tags
 * and may resolve to more than one.
 */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAS_API FEGASBTAbilitySelector
{
	GENERATED_BODY()

	/** Activated through TryActivateAbilitiesByTag, matched against the ability's asset tags. */
	UPROPERTY(EditAnywhere, Category = "Ability")
	FGameplayTag AbilityTag;

	/** Activated through TryActivateAbilityByClass. Takes priority over AbilityTag when set. */
	UPROPERTY(EditAnywhere, Category = "Ability")
	TSubclassOf<UGameplayAbility> AbilityClass;

	/** True when either keying field is filled in. */
	bool IsSet() const;

	/** Short label for behaviour tree node descriptions. */
	FString ToDescription() const;
};

/**
 * Shared plumbing for the behaviour tree tasks that activate a gameplay ability on
 * an ability system component — the AI's own by default, or the actor in
 * TargetActorKey.
 *
 * Per-execution state lives in node memory, never in members: one behaviour tree
 * task node is shared by every agent running the tree, so a member would be
 * overwritten by whichever agent executed last.
 */
UCLASS(Abstract)
class UNREALEXTENDEDGAS_API UEGASBTTask_UseAbilityBase : public UEGASBTTask_Base
{
	GENERATED_BODY()

public:
	UEGASBTTask_UseAbilityBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	/** Keep the task latent for WaitTime seconds after the ability was activated. */
	UPROPERTY(EditAnywhere, Category = "Ability")
	bool bWaitAfterUse = false;

	/** Seconds to stay latent once the ability was activated. Ignored unless bWaitAfterUse. */
	UPROPERTY(EditAnywhere, Category = "Ability", meta = (ClampMin = "0.0", UIMin = "0.0", EditCondition = "bWaitAfterUse"))
	float WaitTime = 0.f;

	/** Activates the selected ability on the target's ASC. False when the ASC or selector is unusable. */
	bool TryActivateAbility(const UBehaviorTreeComponent& OwnerComp, const FEGASBTAbilitySelector& Selector) const;

	/** Succeed straight away, or stay latent for WaitTime first when bWaitAfterUse is set. */
	EBTNodeResult::Type FinishOrWait(uint8* NodeMemory) const;

	/** Trailing "then wait Ns" line for GetStaticDescription, empty when not waiting. */
	FString GetWaitDescription() const;

	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
};
