// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlueprintBase.h"
#include "EFBTTask_RemoveGameplayEffect.generated.h"

/**
 * 
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFBTTask_RemoveGameplayEffect : public UBTTask_BlueprintBase
{
	GENERATED_BODY()

public:

	UEFBTTask_RemoveGameplayEffect();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	bool bRemoveFromSelf = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	TSubclassOf<class UGameplayEffect> RemoveGameplayEffect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	FGameplayTagContainer RemoveWithGrantedTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	FGameplayTagContainer RemoveWithSourceTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	FGameplayTagContainer RemoveWithAppliedTags;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (EditCondition = "!bRemoveFromSelf"))
	FBlackboardKeySelector TargetActorKey;

protected:

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;
};
