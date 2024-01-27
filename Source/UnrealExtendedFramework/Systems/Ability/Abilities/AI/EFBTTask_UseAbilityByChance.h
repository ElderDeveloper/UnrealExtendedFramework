// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "EFBTTask_UseAbilityByChance.generated.h"

/**
 * 
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFBTTask_UseAbilityByChance : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Ability" )
	FName AbilityName;

	UPROPERTY(EditAnywhere, Category = "Ability" )
	float Chance = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Ability" )
	bool bWaitAfterUse = false;

	UPROPERTY(EditAnywhere, Category = "Ability" )
	float WaitTime = 0.f;

protected:
	EBTNodeResult::Type FinishType;

	UPROPERTY()
	UBehaviorTreeComponent* OwnerTreeComp;

	
	void FinishLater();
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	#if WITH_EDITOR
		/** @return string containing description of this node with all setup values */
		virtual FString GetStaticDescription() const override;
	#endif
	
};
