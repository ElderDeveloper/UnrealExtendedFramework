// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "GameFramework/Actor.h"
#include "BTTFindRandomLocation.generated.h"

UCLASS()
class UEEXPANDEDFRAMEWORK_API UBTTFindRandomLocation : public UBTTaskNode
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	UBTTFindRandomLocation();
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	float SearchRadius = 2000;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FBlackboardKeySelector TargetLocationKey;
	
	
	
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};