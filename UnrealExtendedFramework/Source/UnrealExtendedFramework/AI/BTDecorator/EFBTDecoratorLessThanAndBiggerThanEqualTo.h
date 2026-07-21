// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTDecorator.h"
#include "EFBTDecoratorLessThanAndBiggerThanEqualTo.generated.h"

/**
 * 
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFBTDecoratorLessThanAndBiggerThanEqualTo : public UBTDecorator
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FBlackboardKeySelector CheckValue;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	float LessThan = 100;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	float GreaterThan = 0;

protected:

	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;
	virtual FString GetStaticDescription() const override;
};
