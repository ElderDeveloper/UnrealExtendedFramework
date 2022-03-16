// Fill out your copyright notice in the Description page of Project Settings.


#include "EFBTDecoratorLessThanAndBiggerThanEqualTo.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"


bool UEFBTDecoratorLessThanAndBiggerThanEqualTo::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp,uint8* NodeMemory) const
{
	if (auto const AIController = Cast<AAIController>(OwnerComp.GetOwner()))
	{
		if (CheckValue.IsSet())
		{
			const float value = AIController->GetBlackboardComponent()->GetValueAsFloat(CheckValue.SelectedKeyName);
			if (value <= LessThan && value >= GreaterThan)
				return true;
		}
	}
	return false;
}

FString UEFBTDecoratorLessThanAndBiggerThanEqualTo::GetStaticDescription() const
{
	const FString Return = " Value is Less Than Equal  " + FString::SanitizeFloat(LessThan) + " And Greater Than Equal " + FString::SanitizeFloat(GreaterThan) + " "; 
	return Return;
}
