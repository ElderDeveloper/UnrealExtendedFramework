// Fill out your copyright notice in the Description page of Project Settings.

#include "EFBTDecorator_LessThanAndBiggerThan.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"

bool UEFBTDecorator_LessThanAndBiggerThan::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp,uint8* NodeMemory) const
{
	if (auto const AIController = Cast<AAIController>(OwnerComp.GetOwner()))
	{
		const float value = AIController->GetBlackboardComponent()->GetValueAsFloat(CheckValue.SelectedKeyName);
		if (bCheckEqual)
		{
			if (value <= LessThan && value >= GreaterThan)
				return true;
		}
		else if (value < LessThan && value > GreaterThan)
			return true;
	}
	return false;
}

FString UEFBTDecorator_LessThanAndBiggerThan::GetStaticDescription() const
{
	return FString::Printf(TEXT(" Value is Less Than %s %.1f And Greater Than %s %.1f "), bCheckEqual ? TEXT("Equal") : TEXT(""), LessThan, bCheckEqual ? TEXT("Equal") : TEXT(""), GreaterThan);
}
