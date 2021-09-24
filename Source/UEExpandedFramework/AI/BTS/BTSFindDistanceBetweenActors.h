// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "GameFramework/Actor.h"
#include "BTSFindDistanceBetweenActors.generated.h"

UCLASS()
class UEEXPANDEDFRAMEWORK_API UBTSFindDistanceBetweenActors : public UBTService
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	UBTSFindDistanceBetweenActors();



	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FBlackboardKeySelector OwnerActorKey;
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FBlackboardKeySelector OwnerTargetKey;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FBlackboardKeySelector DistanceKey;
	
protected:
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
};
