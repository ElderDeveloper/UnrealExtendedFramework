// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UnrealExtendedFramework/AI/BTTask/Vector/EFBTTaskSetBlackboardVector.h"
#include "EFBTTaskMoveToActor.generated.h"

/**
 * 
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFBTTaskMoveToActor : public UBTTaskNode
{
	GENERATED_BODY()

	UEFBTTaskMoveToActor();

public:

	UPROPERTY(EditAnywhere)
	FBlackboardKeySelector Actor;

	UPROPERTY(EditAnywhere)
	bool bWaitForSuccess = false;

	UPROPERTY(EditAnywhere)
	float AcceptanceRadius = 50;

protected:

	UPROPERTY()
	class UAITask_MoveTo* MoveToRef;

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
};
