// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "EGBTTaskExecuteRandomComboComponent.generated.h"


UCLASS()
class UNREALEXTENDEDGAMEPLAY_API UEGBTTaskExecuteRandomComboComponent : public UBTTaskNode
{
	GENERATED_BODY()


protected:
	
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
