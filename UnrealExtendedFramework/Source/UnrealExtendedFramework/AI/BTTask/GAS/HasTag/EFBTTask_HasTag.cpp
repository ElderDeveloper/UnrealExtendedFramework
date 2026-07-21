// Fill out your copyright notice in the Description page of Project Settings.


#include "EFBTTask_HasTag.h"
#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"

UEFBTTask_HasTag::UEFBTTask_HasTag()
{
	NodeName = "GAS Has Tag";
}

EBTNodeResult::Type UEFBTTask_HasTag::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AActor* TargetActor = nullptr;

	if (bCheckSelf)
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

	const auto AbilitySystemComp = TargetActor->FindComponentByClass<UAbilitySystemComponent>();
	if (!AbilitySystemComp)
	{
		return EBTNodeResult::Failed;
	}
	
	if (AbilitySystemComp->HasAnyMatchingGameplayTags(SearchTag))
	{
		return EBTNodeResult::Succeeded;
	}

	return EBTNodeResult::Failed;
}

FString UEFBTTask_HasTag::GetStaticDescription() const
{
	return FString::Printf(TEXT("Target: %s \nTag: %s"), *TargetActorKey.SelectedKeyName.ToString(), *SearchTag.ToString());
}
