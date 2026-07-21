// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "EFBTTask_SaveLocationVelocity.generated.h"

/**
 * 
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFBTTask_SaveLocationVelocity : public UBTTaskNode
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FBlackboardKeySelector TargetActorKey;
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FBlackboardKeySelector SaveVectorKey;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	float VelocityScale;

	
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;
};
