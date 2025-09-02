// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlueprintBase.h"
#include "EFBTTask_TryActivateAbility.generated.h"

class UGameplayAbility;
/**
 * 
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFBTTask_TryActivateAbility : public UBTTask_BlueprintBase
{
	GENERATED_BODY()

public:

	UEFBTTask_TryActivateAbility();
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	bool bApplyToSelf = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (EditCondition = "!bApplyToSelf"))
	FGameplayTagContainer TryActivateAbilityTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	TSubclassOf<UGameplayAbility> TryActivateAbilityClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (EditCondition = "!bApplyToSelf"))
	FBlackboardKeySelector TargetActorKey;

protected:

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
