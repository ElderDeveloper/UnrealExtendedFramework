// Fill out your copyright notice in the Description page of Project Settings.


#include "EFBTTask_RandomLocation.h"

#include "AIController.h"
#include "NavigationSystem.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"


EBTNodeResult::Type UEFBTTask_RandomLocation::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (auto const AIController = Cast<AAIController>(OwnerComp.GetOwner()))
	{
		if (AIController->GetPawn())
		{
			if (const auto NavSystem = Cast<UNavigationSystemV1>(GetWorld()->GetNavigationSystem()))
			{
				FNavLocation FoundPosition;

				AActor* TargetActor = AIController->GetPawn();
				if (const auto TargetObject = AIController->GetBlackboardComponent()->GetValueAsObject(TargetActorKey.SelectedKeyName))
				{
					TargetActor = Cast<AActor>(TargetObject);
				}
				
				if (NavSystem->GetRandomPointInNavigableRadius(TargetActor->GetActorLocation(),SearchRadius,FoundPosition))
				{
					AIController->GetBlackboardComponent()->SetValue<UBlackboardKeyType_Vector>(TargetLocationKey.SelectedKeyName,FoundPosition.Location);
					return EBTNodeResult::Succeeded;
				}
			}
		}
	}
	
	return Super::ExecuteTask(OwnerComp, NodeMemory);
}

FString UEFBTTask_RandomLocation::GetStaticDescription() const
{
	return FString::Printf(TEXT("Random Location Around %s \nto Key: %s \nRadius %.1f"),*TargetActorKey.SelectedKeyName.ToString(), *TargetLocationKey.SelectedKeyName.ToString(), SearchRadius);
}
