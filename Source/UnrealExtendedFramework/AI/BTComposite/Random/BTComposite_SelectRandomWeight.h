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

	UPROPERTY(EditAnywhere, Category = "Weights")
	mutable TArray<float> ChildWeights;
	
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
