// Fill out your copyright notice in the Description page of Project Settings.


#include "EFBTTaskSetBlackboardVector.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"


EBTNodeResult::Type UEFBTTaskSetBlackboardVector::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (auto const AIController = Cast<AAIController>(OwnerComp.GetOwner()))
	{
		if (const auto Actor = Cast<AActor>(AIController->GetBlackboardComponent()->GetValue<UBlackboardKeyType_Object>(ActorKey.SelectedKeyName)))
		{
			AIController->GetBlackboardComponent()->SetValue<UBlackboardKeyType_Vector>(VectorKey.SelectedKeyName,Actor->GetActorLocation());
			return EBTNodeResult::Succeeded;
		}
	}
	return Super::ExecuteTask(OwnerComp, NodeMemory);
}
