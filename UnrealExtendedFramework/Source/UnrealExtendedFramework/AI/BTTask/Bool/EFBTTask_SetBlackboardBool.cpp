// Fill out your copyright notice in the Description page of Project Settings.


#include "EFBTTask_SetBlackboardBool.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"

EBTNodeResult::Type UEFBTTask_SetBlackboardBool::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (auto const AIController = Cast<AAIController>(OwnerComp.GetOwner()))
	{
		AIController->GetBlackboardComponent()->SetValue<UBlackboardKeyType_Bool>(BoolKey.SelectedKeyName,Value);
		return EBTNodeResult::Succeeded;
	}
	return Super::ExecuteTask(OwnerComp, NodeMemory);
}

FString UEFBTTask_SetBlackboardBool::GetStaticDescription() const
{
	return FString::Printf(TEXT("Set %s to %s"), *BoolKey.SelectedKeyName.ToString(), Value ? TEXT("True") : TEXT("False"));
}
