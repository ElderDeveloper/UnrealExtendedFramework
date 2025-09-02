// Fill out your copyright notice in the Description page of Project Settings.


#include "EFBTTask_RemoveGameplayEffect.h"
#include "GameplayEffect.h"
#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"

UEFBTTask_RemoveGameplayEffect::UEFBTTask_RemoveGameplayEffect()
{
	NodeName = "Remove Gameplay Effect";
}

EBTNodeResult::Type UEFBTTask_RemoveGameplayEffect::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AActor* TargetActor = nullptr;

	if (bRemoveFromSelf)
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

	if (IsValid(RemoveGameplayEffect))
	{
		AbilitySystemComp->RemoveActiveGameplayEffectBySourceEffect(RemoveGameplayEffect,AbilitySystemComp);
	}

	if (RemoveWithGrantedTags.Num() > 0)
	{
		AbilitySystemComp->RemoveActiveEffectsWithGrantedTags(RemoveWithGrantedTags);
	}

	if (RemoveWithSourceTags.Num() > 0)
	{
		AbilitySystemComp->RemoveActiveEffectsWithSourceTags(RemoveWithSourceTags);
	}

	if (RemoveWithAppliedTags.Num() > 0)
	{
		AbilitySystemComp->RemoveActiveEffectsWithAppliedTags(RemoveWithAppliedTags);
	}

	return EBTNodeResult::Succeeded;

}

FString UEFBTTask_RemoveGameplayEffect::GetStaticDescription() const
{
	if (RemoveGameplayEffect)
	{
		return FString::Printf(TEXT("%s: \n %s"), *NodeName, *RemoveGameplayEffect->GetName());
	}
	if (RemoveWithGrantedTags.Num() > 0 || RemoveWithSourceTags.Num() > 0 || RemoveWithAppliedTags.Num() > 0)
	{
		return FString::Printf(TEXT("%s: By Tags"), *NodeName);
	}
	return NodeName;
}
