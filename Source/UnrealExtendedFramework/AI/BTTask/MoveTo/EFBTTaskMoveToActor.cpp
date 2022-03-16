// Fill out your copyright notice in the Description page of Project Settings.


#include "EFBTTaskMoveToActor.h"

#include "AIController.h"
#include "BehaviorTree/BTFunctionLibrary.h"
#include "Tasks/AITask_MoveTo.h"

UEFBTTaskMoveToActor::UEFBTTaskMoveToActor()
{
	bNotifyTick = true;
}

EBTNodeResult::Type UEFBTTaskMoveToActor::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (const auto actor = UBTFunctionLibrary::GetBlackboardValueAsActor(this,Actor))
	{
		MoveToRef = UAITask_MoveTo::AIMoveTo(OwnerComp.GetAIOwner(),FVector::ZeroVector,actor,AcceptanceRadius);
		OwnerComp.GetAIOwner()->MoveToActor(actor,AcceptanceRadius);
	}
	return Super::ExecuteTask(OwnerComp, NodeMemory);
}

EBTNodeResult::Type UEFBTTaskMoveToActor::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	return Super::AbortTask(OwnerComp, NodeMemory);
}

void UEFBTTaskMoveToActor::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickTask(OwnerComp, NodeMemory, DeltaSeconds);

	/*
	if (MoveToRef)
	{
		if (MoveToRef->GetMoveResult() == )
		{
		}
	}
	UAITask_MoveTo::GetMoveResult()
	*/
}
