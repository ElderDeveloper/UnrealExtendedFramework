// Fill out your copyright notice in the Description page of Project Settings.


#include "EFBTTask_UseAbilityByChance.h"
#include "AIController.h"
#include "UnrealExtendedFramework/Systems/Ability/EFExtendedAbilityComponent.h"

void UEFBTTask_UseAbilityByChance::FinishLater()
{
	FinishLatentTask(*OwnerTreeComp , EBTNodeResult::Succeeded);
}

EBTNodeResult::Type UEFBTTask_UseAbilityByChance::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	const float RandomChance = FMath::FRandRange(0.f, 1.f);

	OwnerTreeComp = &OwnerComp;
	EBTNodeResult::Type ReturnType = EBTNodeResult::InProgress;
	
	if (RandomChance <= Chance)
	{
		if (OwnerComp.GetAIOwner() != nullptr)
		{
			if (OwnerComp.GetAIOwner()->GetPawn())
			{
				const auto AbilityComp = OwnerComp.GetAIOwner()->GetPawn()->GetComponentByClass(UEFExtendedAbilityComponent::StaticClass());

				if (const auto ExtendedAbility = Cast<UEFExtendedAbilityComponent>(AbilityComp))
				{
					ExtendedAbility->StartExtendedAbilityByName(OwnerComp.GetAIOwner()->GetPawn() ,AbilityName);
					ReturnType = EBTNodeResult::Succeeded;
				}
			}
		}
		if (ReturnType ==  EBTNodeResult::InProgress )
		{
			ReturnType = EBTNodeResult::Failed;
		}
	}
	if (ReturnType == EBTNodeResult::InProgress)
	{
		ReturnType = EBTNodeResult::Succeeded;
	}

	if (bWaitAfterUse)
	{
		FTimerHandle TimerManager;
		GetWorld()->GetTimerManager().SetTimer(TimerManager, this, &UEFBTTask_UseAbilityByChance::FinishLater, WaitTime, false);
		return EBTNodeResult::InProgress;
	}

	return ReturnType;
}




#if WITH_EDITOR
FString UEFBTTask_UseAbilityByChance::GetStaticDescription() const
{
	FString AbilitiesToUse = "Abilities: " + AbilityName.ToString() + " Chance: " + FString::SanitizeFloat(Chance); 
	return AbilitiesToUse;
}
#endif