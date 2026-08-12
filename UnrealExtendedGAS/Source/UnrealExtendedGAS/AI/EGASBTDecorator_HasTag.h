#pragma once

#include "AI/EGASBTDecorator_Base.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "EGASBTDecorator_HasTag.generated.h"

class UAbilitySystemComponent;

/**
 * Passes while the AI itself, or the actor in TargetActorKey, owns a matching
 * gameplay tag.
 *
 * With bTrackTagChanges the decorator observes the tags and asks the tree to
 * re-evaluate the branch as soon as one is added or removed. That only takes effect
 * when the node's Observer Aborts mode is set to something other than None.
 */
UCLASS(meta = (DisplayName = "Has Gameplay Tag"))
class UNREALEXTENDEDGAS_API UEGASBTDecorator_HasTag : public UEGASBTDecorator_Base
{
	GENERATED_BODY()

public:
	UEGASBTDecorator_HasTag(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Tags to look for on the target. */
	UPROPERTY(EditAnywhere, Category = "Tags")
	FGameplayTagContainer SearchTags;

	/** Require every tag in SearchTags rather than any one of them. */
	UPROPERTY(EditAnywhere, Category = "Tags")
	bool bRequireAllTags = false;

	/** Observe the tags and re-evaluate the branch when one changes. */
	UPROPERTY(EditAnywhere, Category = "Tags")
	bool bTrackTagChanges = true;

protected:
	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;
	virtual FString GetStaticDescription() const override;

	virtual void OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

private:
	void HandleTagChanged(const FGameplayTag Tag, int32 NewCount);

	// Per-agent observer state — safe because the base forces node instancing.
	TWeakObjectPtr<UAbilitySystemComponent> ObservedAbilitySystem;
	TWeakObjectPtr<UBehaviorTreeComponent> CachedOwnerComp;
};
