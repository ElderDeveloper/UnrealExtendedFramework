// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "EFBTTask_UseAbilityRandomLoop.generated.h"

class UEFExtendedAbilityComponent;
/**
 * 
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFBTTask_UseAbilityRandomLoop : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Ability" )
	FName AbilityName;

	UPROPERTY(EditAnywhere, Category = "Ability" )
	int32 MinAbilityUse = 1;

	UPROPERTY(EditAnywhere, Category = "Ability" )
	int32 MaxAbilityUse = 1;

	UPROPERTY(EditAnywhere, Category = "Ability" )
	float TimeBetweenAbilities = 0.f;
	
	UPROPERTY(EditAnywhere, Category = "Ability" )
	bool bWaitAfterUse = false;

	UPROPERTY(EditAnywhere, Category = "Ability" )
	float WaitTime = 0.f;

protected:

	FTimerHandle AbilityWaitTimeHandle;
	EBTNodeResult::Type FinishType;

	int32 AbilityUseCount = 0;
	int32 CurrentAbilityUseCount = 0;

	UPROPERTY()
	AActor* OwnerActor;

	UPROPERTY()
	UEFExtendedAbilityComponent* ExtendedAbilityComp;

	UPROPERTY()
	UBehaviorTreeComponent* OwnerTreeComp;

	
	void FinishLater();
	void UseAbility();
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

#if WITH_EDITOR
	/** @return string containing description of this node with all setup values */
	virtual FString GetStaticDescription() const override;
#endif
	
};
