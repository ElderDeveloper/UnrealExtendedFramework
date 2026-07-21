// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "EFBTServiceDistanceBetweenActors.generated.h"

/**
 * 
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFBTServiceDistanceBetweenActors : public UBTService
{
	GENERATED_BODY()
	
	
public:
	// Sets default values for this actor's properties
	UEFBTServiceDistanceBetweenActors();



	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FBlackboardKeySelector OwnerActorKey;
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FBlackboardKeySelector OwnerTargetKey;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FBlackboardKeySelector DistanceKey;
	
protected:
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
};
