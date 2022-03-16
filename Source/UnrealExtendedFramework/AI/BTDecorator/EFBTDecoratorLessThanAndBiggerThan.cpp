// Fill out your copyright notice in the Description page of Project Settings.


#include "EFBTDecoratorLessThanAndBiggerThan.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"

bool UEFBTDecoratorLessThanAndBiggerThan::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp,uint8* NodeMemory) const
{
	
	if (auto const AIController = Cast<AAIController>(OwnerComp.GetOwner()))
	{
		const float value = AIController->GetBlackboardComponent()->GetValueAsFloat(CheckValue.SelectedKeyName);
		GEngine->AddOnScreenDebugMessage(-1,1,FColor::Red,FString::SanitizeFloat(value));
		if (value < LessThan && value > GreaterThan)
			return true;
	}
	return false;
}

FString UEFBTDecoratorLessThanAndBiggerThan::GetStaticDescription() const
{
	const FString Return = " Value is Less Than " + FString::SanitizeFloat(LessThan) + " And Greater Than " + FString::SanitizeFloat(GreaterThan) + " "; 
	return Return;
}
