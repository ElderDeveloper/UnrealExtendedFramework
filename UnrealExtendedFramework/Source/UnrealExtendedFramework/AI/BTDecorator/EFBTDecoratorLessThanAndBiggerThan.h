// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTDecorator.h"
#include "EFBTDecoratorLessThanAndBiggerThan.generated.h"

/**
 * 
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFBTDecoratorLessThanAndBiggerThan : public UBTDecorator
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
