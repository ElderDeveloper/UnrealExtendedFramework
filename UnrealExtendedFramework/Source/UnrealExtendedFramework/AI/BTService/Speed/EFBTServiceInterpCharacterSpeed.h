// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "EFBTServiceInterpCharacterSpeed.generated.h"

/**
 * 
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFBTServiceInterpCharacterSpeed : public UBTService
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere , BlueprintReadWrite)
	float TargetSpeed = 600;

	UPROPERTY(EditAnywhere , BlueprintReadWrite)
	float InterpSpeed = 3;

protected:
	
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual FString GetStaticServiceDescription() const override;

	
};
