// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "UObject/Object.h"
#include "EFBTTaskStrafe.generated.h"

/**
 * 
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFBTTaskStrafe : public UBTTaskNode
{
	GENERATED_BODY()

	UEFBTTaskStrafe();
public:

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FBlackboardKeySelector StrafeBlackboardKey;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	float StrafeTimeMin = 1;
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	float StrafeTimeMax = 2;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	bool SetOrientRotationToMovementAtFinish = true;

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory);
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

	void FinishThisTask();


	bool TaskFinished = false;
	float StrafeValue = 0;
	FTimerHandle TaskFinishHandle;
	UPROPERTY()
	UBehaviorTreeComponent* TaskOwnerComp;
};
