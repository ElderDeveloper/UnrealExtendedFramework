// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTDecorator.h"
#include "EFBTDecorator_DistanceTo.generated.h"

UENUM(BlueprintType)
enum class EEFDecoratorDistanceCheck : uint8
{
	Less,
	Greater
};


UCLASS(DisplayName="Distance To")
class UNREALEXTENDEDFRAMEWORK_API UEFBTDecorator_DistanceTo final : public UBTDecorator
{
	GENERATED_BODY()

public:

	UEFBTDecorator_DistanceTo();

protected:

	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;
	virtual FString GetStaticDescription() const override;

	UPROPERTY(EditAnywhere, Category = "Condition")
	FBlackboardKeySelector From;

	UPROPERTY(EditAnywhere, Category = "Condition")
	FBlackboardKeySelector To;

	UPROPERTY(EditAnywhere, Category = "Condition")
	float Distance = 500.f;

	UPROPERTY(EditAnywhere, Category = "Condition")
	EEFDecoratorDistanceCheck DistanceCheck;
};
