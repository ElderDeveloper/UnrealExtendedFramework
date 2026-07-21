// Fill out your copyright notice in the Description page of Project Settings.


#include "EFBTTask_UseAbility.h"

#include "AIController.h"
#include "UnrealExtendedFramework/Systems/Ability/EFExtendedAbilityComponent.h"

void UEFBTTask_UseAbility::FinishLater()
{
	FinishLatentTask(*OwnerTreeComp , EBTNodeResult::Succeeded);
}

EBTNodeResult::Type UEFBTTask_UseAbility::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	OwnerTreeComp = &OwnerComp;
	
	if (OwnerComp.GetAIOwner() != nullptr)
	{
		if (OwnerComp.GetAIOwner()->GetPawn())
		{
			const auto AbilityComp = OwnerComp.GetAIOwner()->GetPawn()->GetComponentByClass(UEFExtendedAbilityComponent::StaticClass());

			if (const auto ExtendedAbility = Cast<UEFExtendedAbilityComponent>(AbilityComp))
			{
				ExtendedAbility->StartExtendedAbilityByName(OwnerComp.GetAIOwner()->GetPawn() ,AbilityName);
				if(!bWaitAfterUse)
				{
					return EBTNodeResult::Succeeded;
				}
			}
		}
	}
	FTimerHandle TimerManager;
	GetWorld()->GetTimerManager().SetTimer(TimerManager, this, &UEFBTTask_UseAbility::FinishLater, WaitTime, false);
	return  EBTNodeResult::InProgress;
}



#if WITH_EDITOR
FString UEFBTTask_UseAbility::GetStaticDescription() const
{
	const FString WaitDurationString =  bWaitAfterUse ?  " WaitTime: " + FString::SanitizeFloat(WaitTime) : "";
	FString AbilitiesToUse = "Abilities: " + AbilityName.ToString() + WaitDurationString; 
	return AbilitiesToUse;
}
#endif