// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_MoveTo.h"
#include "Tasks/AITask_MoveTo.h"
#include "EFBTTask_MoveToDuration.generated.h"

/**
 * 
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFBTTask_MoveToDuration : public UBTTask_MoveTo
{
	GENERATED_BODY()

public:

	UEFBTTask_MoveToDuration(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere)
	float Duration = 1.f;

	// Add a random deviation to the duration to make it less predictable
	UPROPERTY(EditAnywhere)
	float RandomDeviation = 0.f;

protected:

	FTimerHandle Handle;

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;
	virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult) override;
	
};
