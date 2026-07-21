// Fill out your copyright notice in the Description page of Project Settings.

#include "EFBTDecorator_GameplayAttribute.h"
#include "BehaviorTree/BlackboardComponent.h"

UEFBTDecorator_GameplayAttribute::UEFBTDecorator_GameplayAttribute()
{
	NodeName = "Compare Gameplay Attribute";

	// This allows the decorator to re-evaluate its condition.
	bNotifyBecomeRelevant = true;
	bNotifyCeaseRelevant = true;
}

bool UEFBTDecorator_GameplayAttribute::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
	// Get the target actor
	AActor* ActorToCheck = nullptr;
	if (UObject* TargetObject = OwnerComp.GetBlackboardComponent()->GetValueAsObject(TargetActorKey.SelectedKeyName))
	{
		ActorToCheck = Cast<AActor>(TargetObject);
	}

	if (!ActorToCheck)
	{
		UE_LOG(LogTemp, Warning, TEXT("EFBTDecorator_GameplayAttribute: ActorToCheck is null"));
		return false;
	}

	// Get the ability system component
	UAbilitySystemComponent* AbilitySystemComponent = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(ActorToCheck);
	if (!AbilitySystemComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("EFBTDecorator_GameplayAttribute: No AbilitySystemComponent found on %s"), *ActorToCheck->GetName());
		return false;
	}

	// Check if the attribute is valid
	if (!AttributeToCheck.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("EFBTDecorator_GameplayAttribute: AttributeToCheck is not valid"));
		return false;
	}

	// Get the attribute value
	float AttributeValue = AbilitySystemComponent->GetNumericAttribute(AttributeToCheck);

	float ComparisonTargetValue = 0.0f;

	// Determine what we're comparing against
	if (bCompareWithAttribute)
	{
		// Compare with another attribute
		AActor* SecondActorToCheck = nullptr;
		if (UObject* SecondTargetObject = OwnerComp.GetBlackboardComponent()->GetValueAsObject(SecondTargetActorKey.SelectedKeyName))
		{
			SecondActorToCheck = Cast<AActor>(SecondTargetObject);
		}

		if (!SecondActorToCheck)
		{
			UE_LOG(LogTemp, Warning, TEXT("EFBTDecorator_GameplayAttribute: SecondActorToCheck is null"));
			return false;
		}

		UAbilitySystemComponent* SecondAbilitySystemComponent = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(SecondActorToCheck);
		if (!SecondAbilitySystemComponent)
		{
			UE_LOG(LogTemp, Warning, TEXT("EFBTDecorator_GameplayAttribute: No AbilitySystemComponent found on second actor %s"), *SecondActorToCheck->GetName());
			return false;
		}

		if (!SecondAttributeToCheck.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("EFBTDecorator_GameplayAttribute: SecondAttributeToCheck is not valid"));
			return false;
		}

		ComparisonTargetValue = SecondAbilitySystemComponent->GetNumericAttribute(SecondAttributeToCheck);
		
		UE_LOG(LogTemp, Log, TEXT("EFBTDecorator_GameplayAttribute: Comparing %s (%.2f) with %s (%.2f)"), 
			*AttributeToCheck.GetName(), AttributeValue,
			*SecondAttributeToCheck.GetName(), ComparisonTargetValue);
	}
	else
	{
		// Compare with static value
		ComparisonTargetValue = ComparisonValue;
		
		UE_LOG(LogTemp, Log, TEXT("EFBTDecorator_GameplayAttribute: Comparing %s (%.2f) with static value (%.2f)"), 
			*AttributeToCheck.GetName(), AttributeValue, ComparisonTargetValue);
	}

	// Perform the comparison
	bool Result = PerformComparison(AttributeValue, ComparisonTargetValue);
	
	UE_LOG(LogTemp, Log, TEXT("EFBTDecorator_GameplayAttribute: Comparison result: %s"), Result ? TEXT("True") : TEXT("False"));
	
	return Result;
}

bool UEFBTDecorator_GameplayAttribute::PerformComparison(float Value1, float Value2) const
{
	switch (ComparisonType)
	{
	case 0: // Equal
		return FMath::IsNearlyEqual(Value1, Value2, KINDA_SMALL_NUMBER);
	case 1: // NotEqual
		return !FMath::IsNearlyEqual(Value1, Value2, KINDA_SMALL_NUMBER);
	case 2: // Greater
		return Value1 > Value2;
	case 3: // GreaterOrEqual
		return Value1 >= Value2;
	case 4: // Less
		return Value1 < Value2;
	case 5: // LessOrEqual
		return Value1 <= Value2;
	default:
		UE_LOG(LogTemp, Warning, TEXT("EFBTDecorator_GameplayAttribute: Invalid ComparisonType %d"), ComparisonType);
		return false;
	}
}

FString UEFBTDecorator_GameplayAttribute::GetComparisonOperatorString() const
{
	switch (ComparisonType)
	{
	case 0: return TEXT("==");
	case 1: return TEXT("!=");
	case 2: return TEXT(">");
	case 3: return TEXT(">=");
	case 4: return TEXT("<");
	case 5: return TEXT("<=");
	default: return TEXT("?");
	}
}

void UEFBTDecorator_GameplayAttribute::OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::OnBecomeRelevant(OwnerComp, NodeMemory);

	if (!bTrackAttributeChanges)
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

			// Bind to attribute change events
			if (AttributeToCheck.IsValid())
			{
				AttributeChangeDelegateHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(AttributeToCheck)
					.AddUObject(this, &UEFBTDecorator_GameplayAttribute::OnAttributeChanged);
			}
		}
	}

	// If comparing with another attribute, set up observation for that too
	if (bCompareWithAttribute)
	{
		AActor* SecondActorToCheck = nullptr;
		if (UObject* SecondTargetObject = OwnerComp.GetBlackboardComponent()->GetValueAsObject(SecondTargetActorKey.SelectedKeyName))
		{
			SecondActorToCheck = Cast<AActor>(SecondTargetObject);
		}

		if (SecondActorToCheck)
		{
			if (UAbilitySystemComponent* SecondAbilitySystemComponent = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(SecondActorToCheck))
			{
				// Store reference to the second ability system component
				SecondObservedAbilitySystemComponent = SecondAbilitySystemComponent;

				// Bind to attribute change events for the second attribute
				if (SecondAttributeToCheck.IsValid())
				{
					SecondAttributeChangeDelegateHandle = SecondAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(SecondAttributeToCheck)
						.AddUObject(this, &UEFBTDecorator_GameplayAttribute::OnAttributeChanged);
				}
			}
		}
	}
}

void UEFBTDecorator_GameplayAttribute::OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::OnCeaseRelevant(OwnerComp, NodeMemory);

	// Clean up observers for first attribute
	if (ObservedAbilitySystemComponent.IsValid() && AttributeToCheck.IsValid())
	{
		if (AttributeChangeDelegateHandle.IsValid())
		{
			ObservedAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(AttributeToCheck)
				.Remove(AttributeChangeDelegateHandle);
			AttributeChangeDelegateHandle.Reset();
		}
		ObservedAbilitySystemComponent.Reset();
	}

	// Clean up observers for second attribute
	if (SecondObservedAbilitySystemComponent.IsValid() && SecondAttributeToCheck.IsValid())
	{
		if (SecondAttributeChangeDelegateHandle.IsValid())
		{
			SecondObservedAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(SecondAttributeToCheck)
				.Remove(SecondAttributeChangeDelegateHandle);
			SecondAttributeChangeDelegateHandle.Reset();
		}
		SecondObservedAbilitySystemComponent.Reset();
	}

	CachedOwnerComp.Reset();
}

void UEFBTDecorator_GameplayAttribute::OnAttributeChanged(const FOnAttributeChangeData& Data)
{
	// When a relevant attribute changes, request re-evaluation of the decorator
	if (CachedOwnerComp.IsValid())
	{
		CachedOwnerComp->RequestExecution(this);
	}
}

FString UEFBTDecorator_GameplayAttribute::GetStaticDescription() const
{
	FString Description;

	// Build the description based on comparison mode
	if (bCompareWithAttribute)
	{
		Description = FString::Printf(TEXT("%s.%s %s %s.%s"),
			*TargetActorKey.SelectedKeyName.ToString(),
			*AttributeToCheck.GetName(),
			*GetComparisonOperatorString(),
			*SecondTargetActorKey.SelectedKeyName.ToString(),
			*SecondAttributeToCheck.GetName());
	}
	else
	{
		Description = FString::Printf(TEXT("%s.%s %s %.2f"),
			*TargetActorKey.SelectedKeyName.ToString(),
			*AttributeToCheck.GetName(),
			*GetComparisonOperatorString(),
			ComparisonValue);
	}

	return Description;
}
