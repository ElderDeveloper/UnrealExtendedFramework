// Fill out your copyright notice in the Description page of Project Settings.


#include "EFBTDecorator_HasTag.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "BehaviorTree/BlackboardComponent.h"

UEFBTDecorator_HasTag::UEFBTDecorator_HasTag()
{
	NodeName = "Has Gameplay Tag";

	// This allows the decorator to re-evaluate its condition.
	bNotifyBecomeRelevant = true;
	bNotifyCeaseRelevant = true;
}

bool UEFBTDecorator_HasTag::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
	AActor* ActorToCheck = nullptr;
	if (UObject* TargetObject = OwnerComp.GetBlackboardComponent()->GetValueAsObject(TargetActorKey.SelectedKeyName))
	{
		ActorToCheck = Cast<AActor>(TargetObject);
	}

	if (ActorToCheck)
	{
		if (UAbilitySystemComponent* AbilitySystemComponent = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(ActorToCheck))
		{
			UE_LOG(LogTemp, Log, TEXT("EFBTDecorator_HasTag: Checking tags on %s"), *ActorToCheck->GetName());
			UE_LOG(LogTemp, Log, TEXT("EFBTDecorator_HasTag: Actor has tags: %s"), *AbilitySystemComponent->GetOwnedGameplayTags().ToString());
			UE_LOG(LogTemp, Log, TEXT("EFBTDecorator_HasTag: Searching for tag: %s"), *SearchTag.ToString());
			return AbilitySystemComponent->HasAnyMatchingGameplayTags(SearchTag);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("EFBTDecorator_HasTag: ActorToCheck is null") );
	}

	return false;
}

void UEFBTDecorator_HasTag::OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::OnBecomeRelevant(OwnerComp, NodeMemory);

	if (!bTrackTagChanges)
	{
		return;
	}
	
	// Cache the behavior tree component reference
	CachedOwnerComp = &OwnerComp;
	
	// Get the target actor and set up observation
	AActor* ActorToCheck = nullptr;
	if (UObject* TargetObject = OwnerComp.GetBlackboardComponent()->GetValueAsObject(TargetActorKey.SelectedKeyName))
	{
		ActorToCheck = Cast<AActor>(TargetObject);
	}

	if (ActorToCheck)
	{
		if (UAbilitySystemComponent* AbilitySystemComponent = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(ActorToCheck))
		{
			// Store reference to the ability system component
			ObservedAbilitySystemComponent = AbilitySystemComponent;
			
			// Bind to tag change events for each tag we're interested in
			for (const FGameplayTag& Tag : SearchTag)
			{
				AbilitySystemComponent->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::NewOrRemoved).AddUFunction(this, FName("OnGameplayTagsChanged"));
			}
		}
	}
}

void UEFBTDecorator_HasTag::OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::OnCeaseRelevant(OwnerComp, NodeMemory);
	
	// Clean up observers
	if (ObservedAbilitySystemComponent.IsValid())
	{
		for (const FGameplayTag& Tag : SearchTag)
		{
			ObservedAbilitySystemComponent->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::NewOrRemoved)
				.RemoveAll(this);
		}
		ObservedAbilitySystemComponent.Reset();
	}
	
	CachedOwnerComp.Reset();
}

void UEFBTDecorator_HasTag::OnGameplayTagsChanged(const FGameplayTag Tag, int32 NewCount)
{
	// When a relevant tag changes, request re-evaluation of the decorator
	if (CachedOwnerComp.IsValid())
	{
		CachedOwnerComp->RequestExecution(this);
	}
}


FString UEFBTDecorator_HasTag::GetStaticDescription() const
{
	return FString::Printf(TEXT("%s Has Tag: \n %s"), *TargetActorKey.SelectedKeyName.ToString(), *SearchTag.ToString());
}
