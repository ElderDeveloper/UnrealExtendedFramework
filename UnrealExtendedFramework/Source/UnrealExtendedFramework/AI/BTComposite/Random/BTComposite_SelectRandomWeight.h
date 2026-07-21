// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTCompositeNode.h"
#include "BTComposite_SelectRandomWeight.generated.h"


UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UBTComposite_SelectRandomWeight : public UBTCompositeNode
{
	GENERATED_BODY()

public:
	UBTComposite_SelectRandomWeight(const FObjectInitializer& ObjectInitializer);

	/**
	 * The base weight for each child node.
	 * - Array size must match the number of children
	 * - Higher values = higher selection probability
	 * - Values are relative (e.g., [0.8, 0.2] = 80%/20% split)
	 * - Automatically resized to match child count
	 */
	UPROPERTY(EditAnywhere, Category = "Weights")
	mutable TArray<float> ChildWeights;
	
	
	/**
	 * How much each execution reduces the effective weight.
	 * - Applied per execution: Effective Weight = Base Weight - (Executions × Decay Factor)
	 * - Higher values = stronger balancing effect
	 * - 0.0 = no decay (pure weighted random)
	 * - Recommended range: 0.05 - 0.2
	 * - Example: Weight 1.0, Decay 0.1, after 5 executions = effective weight 0.5
	 */
	UPROPERTY(EditAnywhere, Category = "Weights")
	float DecayFactor;
	
	virtual int32 GetNextChildHandler(struct FBehaviorTreeSearchData& SearchData, int32 PrevChild, EBTNodeResult::Type LastResult) const override;
	virtual void CleanupMemory(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTMemoryClear::Type CleanupType) const override;
	virtual void InitializeMemory(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTMemoryInit::Type InitType) const override;
#if WITH_EDITOR
	virtual FName GetNodeIconName() const override;
	virtual FString GetStaticDescription() const override;
#endif

private:
	
	void ResizeArrays() const;
	void AddOneToExecutionCount(int32 ChildIndex) const;
	
	mutable TArray<float> DecayedChildWeights;
	mutable TMap<int32, int32> ExecutionCounts;
};
