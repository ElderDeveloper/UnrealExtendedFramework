// Fill out your copyright notice in the Description page of Project Settings.


#include "EFBTTask_UseRandomAbility.h"
#include "AIController.h"
#include "UnrealExtendedFramework/Systems/Ability/EFExtendedAbilityComponent.h"

void UEFBTTask_UseRandomAbility::FinishLater()
{
	FinishLatentTask(*OwnerTreeComp , EBTNodeResult::Succeeded);
}

EBTNodeResult::Type UEFBTTask_UseRandomAbility::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	FinishType = EBTNodeResult::Type::InProgress;
	OwnerTreeComp = &OwnerComp;
	
	if (OwnerComp.GetAIOwner() != nullptr)
	{
		if (OwnerComp.GetAIOwner()->GetPawn())
		{
			const auto AbilityComp = OwnerComp.GetAIOwner()->GetPawn()->GetComponentByClass(UEFExtendedAbilityComponent::StaticClass());

			if (const auto ExtendedAbility = Cast<UEFExtendedAbilityComponent>(AbilityComp))
			{
				const FName AbilityToUse = AbilityNames[FMath::RandRange(0, AbilityNames.Num() - 1)];
				ExtendedAbility->StartExtendedAbilityByName(OwnerComp.GetAIOwner()->GetPawn() ,AbilityToUse);
				FinishType =  EBTNodeResult::Succeeded;
			}
		}
	}

	if (FinishType ==  EBTNodeResult::InProgress)
	{
		FinishType = EBTNodeResult::Failed;
	}

	if (bWaitAfterUse)
	{
		FTimerHandle TimerManager;
		GetWorld()->GetTimerManager().SetTimer(TimerManager, this, &UEFBTTask_UseRandomAbility::FinishLater, WaitTime, false);
		return  EBTNodeResult::InProgress;
	}
	if (FinishType == EBTNodeResult::Type::Succeeded)
	{
		return  EBTNodeResult::Type::Succeeded;
	}
	return  EBTNodeResult::Type::Failed;

}




#if WITH_EDITOR
FString UEFBTTask_UseRandomAbility::GetStaticDescription() const
{
	FString AbilitiesToUse = "Abilities: ";

	for (auto& Ability : AbilityNames)
	{
		AbilitiesToUse += Ability.ToString() + " ";
	}
	return AbilitiesToUse;
}
#endif