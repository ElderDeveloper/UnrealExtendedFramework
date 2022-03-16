// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "EFBTTaskDelay.generated.h"

/**
 * 
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFBTTaskDelay : public UBTTaskNode
{
	GENERATED_BODY()


public:

	UPROPERTY(EditAnywhere)
	float DelayMin = 1;

	UPROPERTY(EditAnywhere)
	float DelayMax = 2;

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

protected:

	FTimerHandle DelayHandle;

	void OnDelayFinished();
};
