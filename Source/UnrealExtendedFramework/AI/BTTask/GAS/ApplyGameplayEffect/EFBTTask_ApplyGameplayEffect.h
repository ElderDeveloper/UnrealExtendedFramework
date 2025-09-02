// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlueprintBase.h"
#include "EFBTTask_ApplyGameplayEffect.generated.h"

/**
 * 
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFBTTask_ApplyGameplayEffect : public UBTTask_BlueprintBase
{
	GENERATED_BODY()

public:

	UEFBTTask_ApplyGameplayEffect();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	TSubclassOf<class UGameplayEffect> GameplayEffect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	int32 GameplayEffectLevel = 1;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	bool bApplyToSelf = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (EditCondition = "!bApplyToSelf"))
	FBlackboardKeySelector TargetActorKey;

protected:

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;
};
