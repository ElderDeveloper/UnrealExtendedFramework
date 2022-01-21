// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "EFBTTaskSetBlackboardVector.generated.h"

/**
 * 
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFBTTaskSetBlackboardVector : public UBTTaskNode
{
	GENERATED_BODY()

public:
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FBlackboardKeySelector VectorKey;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FBlackboardKeySelector ActorKey;
	
	
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
