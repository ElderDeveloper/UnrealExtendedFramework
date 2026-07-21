// Fill out your copyright notice in the Description page of Project Settings.


#include "EFBTTask_SaveLocation.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"


EBTNodeResult::Type UEFBTTask_SaveLocation::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (auto const AIController = Cast<AAIController>(OwnerComp.GetOwner()))
	{
		if (const auto Actor = Cast<AActor>(AIController->GetBlackboardComponent()->GetValue<UBlackboardKeyType_Object>(TargetActorKey.SelectedKeyName)))
		{
			AIController->GetBlackboardComponent()->SetValue<UBlackboardKeyType_Vector>(SaveVectorKey.SelectedKeyName,Actor->GetActorLocation());
			return EBTNodeResult::Succeeded;
		}
	}
	return Super::ExecuteTask(OwnerComp, NodeMemory);
}


FString UEFBTTask_SaveLocation::GetStaticDescription() const
{
	return FString::Printf(TEXT("Save location of %s \nin %s"), *TargetActorKey.SelectedKeyName.ToString(), *SaveVectorKey.SelectedKeyName.ToString());
}