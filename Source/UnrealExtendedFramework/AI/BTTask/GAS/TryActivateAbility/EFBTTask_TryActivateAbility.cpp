// Fill out your copyright notice in the Description page of Project Settings.


#include "EFBTTask_TryActivateAbility.h"
#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Kismet/KismetSystemLibrary.h"

UEFBTTask_TryActivateAbility::UEFBTTask_TryActivateAbility()
{
	NodeName = "Try Activate Ability";
}

EBTNodeResult::Type UEFBTTask_TryActivateAbility::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AActor* TargetActor = nullptr;

	if (bApplyToSelf)
	{
		if (AAIController* AIController = OwnerComp.GetAIOwner())
		{
			TargetActor = AIController->GetPawn();
		}
	}
	else
	{
		if (UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent())
		{
			UObject* KeyValue = BlackboardComp->GetValueAsObject(TargetActorKey.SelectedKeyName);
			TargetActor = Cast<AActor>(KeyValue);
		}
	}

	if (!TargetActor)
	{
		return EBTNodeResult::Failed;
	}

	UAbilitySystemComponent* AbilitySystemComp = TargetActor->FindComponentByClass<UAbilitySystemComponent>();
	if (!AbilitySystemComp)
	{
		return EBTNodeResult::Failed;
	}

	if (IsValid(TryActivateAbilityClass))
	{
		bool bSuccess = AbilitySystemComp->TryActivateAbilityByClass(TryActivateAbilityClass);
		UKismetSystemLibrary::PrintString(this,FString::Printf(TEXT("TryActivateAbilityByClass %s: %s"),*TryActivateAbilityClass->GetName(),bSuccess ? TEXT("Succeeded") : TEXT("Failed")),false);
	}

	if (TryActivateAbilityTag.IsValid())
	{
		bool bSuccess = AbilitySystemComp->TryActivateAbilitiesByTag(TryActivateAbilityTag);
		UKismetSystemLibrary::PrintString(this,FString::Printf(TEXT("TryActivateAbilitiesByTag %s: %s"),*TryActivateAbilityTag.ToString(),bSuccess ? TEXT("Succeeded") : TEXT("Failed")),false);
	}

	return EBTNodeResult::Succeeded;
}
