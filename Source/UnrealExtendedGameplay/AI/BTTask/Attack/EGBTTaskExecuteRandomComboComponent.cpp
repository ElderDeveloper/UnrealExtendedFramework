// Fill out your copyright notice in the Description page of Project Settings.


#include "EGBTTaskExecuteRandomComboComponent.h"

#include "AIController.h"
#include "UnrealExtendedGameplay/Systems/Combat/Components/Combo/EGRandomComboComponent.h"

EBTNodeResult::Type UEGBTTaskExecuteRandomComboComponent::ExecuteTask(UBehaviorTreeComponent& OwnerComp,uint8* NodeMemory)
{
	if (auto const AIController = Cast<AAIController>(OwnerComp.GetOwner()))
	{
		if (AIController->GetPawn())
		{
			
			if (const auto RandomComboComponent = AIController->GetPawn()->FindComponentByClass<UEGRandomComboComponent>())
			{
				RandomComboComponent->ExecuteCombo();
				return EBTNodeResult::Succeeded;
			}
		}
	}
	return EBTNodeResult::Failed;
}
