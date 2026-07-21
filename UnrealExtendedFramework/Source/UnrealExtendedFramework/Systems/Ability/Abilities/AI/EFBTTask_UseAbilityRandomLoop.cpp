// Fill out your copyright notice in the Description page of Project Settings.


#include "EFBTTask_UseAbilityRandomLoop.h"
#include "AIController.h"
#include "UnrealExtendedFramework/Systems/Ability/EFExtendedAbilityComponent.h"

void UEFBTTask_UseAbilityRandomLoop::FinishLater()
{
	FinishLatentTask(*OwnerTreeComp , EBTNodeResult::Succeeded);
}

void UEFBTTask_UseAbilityRandomLoop::UseAbility()
{
	CurrentAbilityUseCount++;

	ExtendedAbilityComp->StartExtendedAbilityByName(OwnerActor ,AbilityName);
	
	if (CurrentAbilityUseCount > AbilityUseCount)
	{
		GetWorld()->GetTimerManager().ClearTimer( AbilityWaitTimeHandle );
		
		if (bWaitAfterUse)
		{
			FTimerHandle TimerManager;
			GetWorld()->GetTimerManager().SetTimer(TimerManager, this, &UEFBTTask_UseAbilityRandomLoop::FinishLater, WaitTime, false);
		}
		else
		{
			FinishLatentTask(*OwnerTreeComp , EBTNodeResult::Succeeded);
		}
	}
}

EBTNodeResult::Type UEFBTTask_UseAbilityRandomLoop::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AbilityUseCount = FMath::RandRange(MinAbilityUse, MaxAbilityUse);
	OwnerTreeComp = &OwnerComp;
	CurrentAbilityUseCount = 0;
	
	if (OwnerComp.GetAIOwner() != nullptr)
	{
		if (OwnerComp.GetAIOwner()->GetPawn())
		{
			const auto AbilityComp = OwnerComp.GetAIOwner()->GetPawn()->GetComponentByClass(UEFExtendedAbilityComponent::StaticClass());

			if (const auto ExtendedAbility = Cast<UEFExtendedAbilityComponent>(AbilityComp))
			{
				if (ExtendedAbility->StartExtendedAbilityByName(OwnerActor ,AbilityName))
				{
					CurrentAbilityUseCount++;
					OwnerActor = OwnerComp.GetAIOwner()->GetPawn();
					ExtendedAbilityComp = ExtendedAbility;
					GetWorld()->GetTimerManager().SetTimer( AbilityWaitTimeHandle, this, &UEFBTTask_UseAbilityRandomLoop::UseAbility, TimeBetweenAbilities, true);
					return  EBTNodeResult::InProgress;
				}
			}
		}
	}
	return  EBTNodeResult::Failed;
}




#if WITH_EDITOR
FString UEFBTTask_UseAbilityRandomLoop::GetStaticDescription() const
{
	FString AbilitiesToUse = "Abilities: " + AbilityName.ToString() +  " Min: " + FString::FromInt(MinAbilityUse) + " Max: " + FString::FromInt(MaxAbilityUse);; 
	return AbilitiesToUse;
}
#endif

