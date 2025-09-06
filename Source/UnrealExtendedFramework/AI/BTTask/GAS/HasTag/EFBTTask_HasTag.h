// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "EFBTTask_HasTag.generated.h"

/**
 * 
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFBTTask_HasTag : public UBTTaskNode
{
	GENERATED_BODY()

public:

	UEFBTTask_HasTag();
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	bool bCheckSelf = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	FGameplayTagContainer SearchTag;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (EditCondition = "!bCheckSelf"))
	FBlackboardKeySelector TargetActorKey;

protected:

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;
};
