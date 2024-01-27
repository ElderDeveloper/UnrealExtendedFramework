// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "EFBTTask_UseAbility.generated.h"

/**
 * 
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFBTTask_UseAbility : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Ability" )
	FName AbilityName;

protected:

	UPROPERTY(EditAnywhere, Category = "Ability" )
	bool bWaitAfterUse = false;

	UPROPERTY(EditAnywhere, Category = "Ability" )
	float WaitTime = 0.f;

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
