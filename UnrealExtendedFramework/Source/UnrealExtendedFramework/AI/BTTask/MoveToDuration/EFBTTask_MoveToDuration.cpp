// Fill out your copyright notice in the Description page of Project Settings.


#include "EFBTTask_MoveToDuration.h"

#include "Kismet/KismetMathLibrary.h"

UEFBTTask_MoveToDuration::UEFBTTask_MoveToDuration(const FObjectInitializer& ObjectInitializer)
{
	Duration = 1.f;
}


EBTNodeResult::Type UEFBTTask_MoveToDuration::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	float TotalDuration = Duration + UKismetMathLibrary::RandomFloatInRange(0.f,RandomDeviation);
	
	GetWorld()->GetTimerManager().SetTimer(Handle, [this, &OwnerComp]()
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
	}, TotalDuration, false);
	
	return Super::ExecuteTask(OwnerComp, NodeMemory);
}

FString UEFBTTask_MoveToDuration::GetStaticDescription() const
{
	return Super::GetStaticDescription();
}

void UEFBTTask_MoveToDuration::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory,
	EBTNodeResult::Type TaskResult)
{
	GetWorld()->GetTimerManager().ClearTimer(Handle);
	Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult);

}

