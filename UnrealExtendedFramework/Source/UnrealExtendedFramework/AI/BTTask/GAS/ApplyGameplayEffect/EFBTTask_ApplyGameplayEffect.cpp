// Fill out your copyright notice in the Description page of Project Settings.


#include "EFBTTask_ApplyGameplayEffect.h"


#include "BehaviorTree/BlackboardComponent.h"
 #include "GameplayEffect.h"
#include "AbilitySystemComponent.h"
#include "AIController.h"

UEFBTTask_ApplyGameplayEffect::UEFBTTask_ApplyGameplayEffect()
{
	NodeName = "Apply Gameplay Effect";
}

EBTNodeResult::Type UEFBTTask_ApplyGameplayEffect::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
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

	if (!GameplayEffect)
	{
		return EBTNodeResult::Failed;
	}

	FGameplayEffectContextHandle EffectContext = AbilitySystemComp->MakeEffectContext();
	EffectContext.AddSourceObject(OwnerComp.GetAIOwner()->GetPawn());

	FGameplayEffectSpecHandle SpecHandle = AbilitySystemComp->MakeOutgoingSpec(GameplayEffect, GameplayEffectLevel, EffectContext);
	if (!SpecHandle.IsValid())
	{
		return EBTNodeResult::Failed;
	}

	FActiveGameplayEffectHandle GEHandle = AbilitySystemComp->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data);
	if (!GEHandle.IsValid())
	{
		return EBTNodeResult::Failed;
	}

	return EBTNodeResult::Succeeded;
}

FString UEFBTTask_ApplyGameplayEffect::GetStaticDescription() const
{
	if (GameplayEffect)
	{
		return FString::Printf(TEXT("%s: Apply \n %s (Level %d) to %s"), *NodeName, *GameplayEffect->GetName(), GameplayEffectLevel, bApplyToSelf ? TEXT("Self") : *TargetActorKey.SelectedKeyName.ToString());
	}
	return FString::Printf(TEXT("%s: No Gameplay Effect"), *NodeName);
}
