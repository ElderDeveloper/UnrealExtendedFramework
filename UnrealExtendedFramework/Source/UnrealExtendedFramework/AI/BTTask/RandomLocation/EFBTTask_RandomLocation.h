// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "EFBTTask_RandomLocation.generated.h"


UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFBTTask_RandomLocation : public UBTTaskNode
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FBlackboardKeySelector TargetActorKey;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FBlackboardKeySelector TargetLocationKey;
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	float SearchRadius = 2000;
	
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;
};
