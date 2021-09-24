// Fill out your copyright notice in the Description page of Project Settings.


#include "BTTFindRandomLocation.h"

#include "AIController.h"
#include "NavigationSystem.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"


// Sets default values
UBTTFindRandomLocation::UBTTFindRandomLocation()
{

}

EBTNodeResult::Type UBTTFindRandomLocation::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (auto const AIController = Cast<AAIController>(OwnerComp.GetOwner()))
	{
		if (AIController->GetPawn())
		{
			if (const auto NavSystem = Cast<UNavigationSystemV1>(GetWorld()->GetNavigationSystem()))
			{
				FNavLocation FoundPosition;
				if (NavSystem->GetRandomPointInNavigableRadius(AIController->GetPawn()->GetActorLocation(),SearchRadius,FoundPosition))
				{
					AIController->GetBlackboardComponent()->SetValue<UBlackboardKeyType_Vector>(TargetLocationKey.SelectedKeyName,FoundPosition.Location);
					return EBTNodeResult::Succeeded;
				}
			}
		}
	}
	
	return Super::ExecuteTask(OwnerComp, NodeMemory);
}