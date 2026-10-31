// Copyright Moon Punch Games. All Rights Reserved.


#include "EFBTTask_ClearBlackboardObject.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"

EBTNodeResult::Type UEFBTTask_ClearBlackboardObject::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (auto const AIController = Cast<AAIController>(OwnerComp.GetOwner()))
	{
		AIController->GetBlackboardComponent()->ClearValue(ObjectKey.SelectedKeyName);
		return EBTNodeResult::Succeeded;
	}
	return Super::ExecuteTask(OwnerComp, NodeMemory);
}

FString UEFBTTask_ClearBlackboardObject::GetStaticDescription() const
{
	return FString::Printf(TEXT("Clear %s"), *ObjectKey.SelectedKeyName.ToString());
}
