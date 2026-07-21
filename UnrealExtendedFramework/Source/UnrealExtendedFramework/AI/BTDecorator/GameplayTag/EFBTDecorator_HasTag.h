// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTDecorator.h"
#include "BehaviorTree/Decorators/BTDecorator_BlackboardBase.h"
#include "EFBTDecorator_HasTag.generated.h"

/**
 * 
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFBTDecorator_HasTag : public UBTDecorator
{
	GENERATED_BODY()

public:

	UEFBTDecorator_HasTag();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	bool bTrackTagChanges = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	FBlackboardKeySelector TargetActorKey;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	FGameplayTagContainer SearchTag;

protected:

	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;
	virtual FString GetStaticDescription() const override;

	virtual void OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

private:
	// Callback for when gameplay tags change
	UFUNCTION()
	void OnGameplayTagsChanged(const FGameplayTag Tag, int32 NewCount);
	
	// Store weak reference to the ability system component we're observing
	UPROPERTY()
	TWeakObjectPtr<class UAbilitySystemComponent> ObservedAbilitySystemComponent;
	
	// Store weak reference to the behavior tree component for re-evaluation
	UPROPERTY()
	TWeakObjectPtr<UBehaviorTreeComponent> CachedOwnerComp;
};

