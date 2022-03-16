// Fill out your copyright notice in the Description page of Project Settings.


#include "EFBTTaskDelay.h"

#include "BehaviorTree/Tasks/BTTask_BlueprintBase.h"
#include "Kismet/KismetMathLibrary.h"

EBTNodeResult::Type UEFBTTaskDelay::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	GetWorld()->GetTimerManager().SetTimer(DelayHandle,
		this,&UEFBTTaskDelay::OnDelayFinished,
		UKismetMathLibrary::RandomFloatInRange(DelayMin , DelayMax),
		false);
	return Super::ExecuteTask(OwnerComp, NodeMemory);
}

void UEFBTTaskDelay::OnDelayFinished()
{
	UBehaviorTreeComponent* OwnerComp = Cast<UBehaviorTreeComponent>(GetOuter());
	FinishLatentTask(*OwnerComp,EBTNodeResult::Succeeded);
}
