// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "EFBTServiceRotateToActor.generated.h"

/**
 * 
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFBTServiceRotateToActor : public UBTService
{
	GENERATED_BODY()


public:

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FBlackboardKeySelector TargetActorKey;


	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FBlackboardKeySelector IsRotateBlocked;

	/**
	 *	Actor Rotation Speed
	 */
	UPROPERTY(EditAnywhere , BlueprintReadWrite)
	float InterpSpeed = 4;

	/**
	 *	If True Use Custom Rotate To Actor If False Use Built In Find Look At Rotation
	 */
	UPROPERTY(EditAnywhere , BlueprintReadWrite)
	bool bUseCustomRotation = true;

	/**
	 * If True Actor Will Rotate X , Y and Z axis If False The Actor Will Only Rotate In Y axis
	 * True Overrides bUseCustomRotation and Always Use Build In Find Look At Rotation
	 */
	UPROPERTY(EditAnywhere , BlueprintReadWrite)
	bool bUseFullRotation = false;

protected:
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
};
